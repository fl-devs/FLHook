#include "PCH.hpp"

#include <Psapi.h>

#include "Global.hpp"

#include "API/API.hpp"

PerfTimer::PerfTimer(const std::wstring& func, uint warn) : function(func), warning(warn) {}

void PerfTimer::Start() { tmStart = TimeUtils::UnixMilliseconds(); }

uint PerfTimer::Stop()
{
    auto timeDelta = static_cast<uint>(TimeUtils::UnixMilliseconds() - tmStart);

    if (FLHookConfig::i()->debug.logPerformanceTimers)
    {
        if (timeDelta > max && timeDelta > warning)
        {
            Logger::i()->Log(LogLevel::Info, std::format(L"Spent {} ms in {}, longest so far.", timeDelta, function));
            max = timeDelta;
        }
        else if (timeDelta > 100)
        {
            Logger::i()->Log(LogLevel::Info, std::format(L"Spent {} ms in {}", timeDelta, function));
        }
    }
    return timeDelta;
}

std::vector<std::shared_ptr<Timer>> Timer::timers;

std::shared_ptr<Timer> Timer::Add(std::function<void()> function, void* funcAddrRaw, uint interval)
{
    DWORD funcAddr = reinterpret_cast<DWORD>(funcAddrRaw);
    if (std::ranges::any_of(timers, [funcAddr](const std::shared_ptr<Timer>& existing) { return funcAddr == existing->funcAddr; }))
    {
        return nullptr;
    }

    auto ptr = std::make_shared<Timer>(function, funcAddr, interval);
    timers.emplace_back(ptr);

    return ptr;
}

void Timer::Remove(const DWORD funcAddr)
{
    const auto timer = std::ranges::find_if(timers, [funcAddr](const std::shared_ptr<Timer>& existing) { return funcAddr == existing->funcAddr; });
    if (timer == timers.end())
    {
        return;
    }

    timers.erase(timer);
}

// -- Timers

//! Fanout Player stats to subscribers
void PublishServerStats()
{
    ServerStats stats;
    const auto globals = CoreGlobals::c();

    stats.npcsEnabled = !globals->disableNpcs;
    stats.serverLoad = globals->serverLoadInMs;

    PROCESS_MEMORY_COUNTERS memCounter;
    GetProcessMemoryInfo(GetCurrentProcess(), &memCounter, sizeof memCounter);
    stats.memoryUsage = memCounter.WorkingSetSize;

    PlayerData* pd = nullptr;
    for (pd = Players.traverse_active(pd); pd; pd = Players.traverse_active(pd))
    {
        const auto info = Hk::Admin::GetPlayerInfo(pd->onlineId, false).Raw();
        if (info.has_error())
        {
            continue;
        }

        ServerStats::Player player;
        player.clientId = pd->onlineId;
        player.playerName = info.value().character;
        player.systemName = info.value().systemName;
        player.systemNick = StringUtils::stows(Universe::get_system(info.value().system)->nickname);
        player.ipAddress = info.value().IP;

        stats.players.emplace_back(player);
    }

    const auto json = Serializer::ObjToJson(stats);
    MessageHandler::i()->Publish(StringUtils::stows(json.dump()), std::wstring(MessageHandler::QueueToStr(MessageHandler::Queue::ServerStats)), L"");
}

//! check if temporary ban has expired
void TimerTempBanCheck()
{
    const auto* config = FLHookConfig::c();
    if (config->general.tempBansEnabled)
    {
        TempBanManager::i()->ClearFinishedTempBans();
    }
}

//! check if players should be kicked
void TimerCheckKick()
{
    CallPluginsBefore(HookedCall::FLHook__TimerCheckKick);

    TRY_HOOK
    {
        // for all players
        PlayerData* playerData = nullptr;
        while ((playerData = Players.traverse_active(playerData)))
        {
            ClientId client = playerData->onlineId;
            if (client < 1 || client > MaxClientId)
            {
                continue;
            }

            if (ClientInfo[client].tmKickTime)
            {
                if (TimeUtils::UnixMilliseconds() >= ClientInfo[client].tmKickTime)
                {
                    Hk::Player::Kick(client); // kick time expired
                    ClientInfo[client].tmKickTime = 0;
                }
                continue; // player will be kicked anyway
            }
            const auto* config = FLHookConfig::c();
            if (config->general.antiBaseIdle)
            {
                // anti base-idle check
                uint baseId;
                pub::Player::GetBase(client, baseId);
                if (baseId && ClientInfo[client].baseEnterTime && time(nullptr) - ClientInfo[client].baseEnterTime >= config->general.antiBaseIdle)
                {
                    // AddKickLog(client, "base idling");
                    Hk::Player::MsgAndKick(client, L"Base idling", 10);
                    ClientInfo[client].baseEnterTime = 0;
                }
            }

            if (config->general.antiCharMenuIdle)
            {
                // anti charmenu-idle check
                if (Hk::Client::IsInCharSelectMenu(client))
                {
                    if (!ClientInfo[client].charMenuEnterTime)
                    {
                        ClientInfo[client].charMenuEnterTime = static_cast<uint>(time(nullptr));
                    }
                    else if (time(nullptr) - ClientInfo[client].charMenuEnterTime >= config->general.antiCharMenuIdle)
                    {
                        // AddKickLog(client, "Charmenu idling");
                        Hk::Player::Kick(client);
                        ClientInfo[client].charMenuEnterTime = 0;
                    }
                }
                else
                {
                    ClientInfo[client].charMenuEnterTime = 0;
                }
            }
        }
    }
    CATCH_HOOK({})
}

/**************************************************************************************************************
Check if NPC spawns should be disabled
**************************************************************************************************************/

void TimerNPCAndF1Check()
{
    CallPluginsBefore(HookedCall::FLHook__TimerNPCAndF1Check);

    TRY_HOOK
    {
        PlayerData* playerData = nullptr;
        while ((playerData = Players.traverse_active(playerData)))
        {
            ClientId client = playerData->onlineId;
            if (client < 1 || client > MaxClientId)
            {
                continue;
            }

            if (ClientInfo[client].tmF1Time && TimeUtils::UnixMilliseconds() >= ClientInfo[client].tmF1Time)
            {
                // f1
                Server.CharacterInfoReq(client, false);
                ClientInfo[client].tmF1Time = 0;
            }
            else if (ClientInfo[client].tmF1TimeDisconnect && TimeUtils::UnixMilliseconds() >= ClientInfo[client].tmF1TimeDisconnect)
            {
                ulong dataArray[64] = { 0 };
                dataArray[26] = client;

                __asm {
						pushad
						lea ecx, dataArray
						mov eax, [remoteClient]
						add eax, ADDR_RC_DISCONNECT
						call eax ; disconncet
						popad
                }

                ClientInfo[client].tmF1TimeDisconnect = 0;
                continue;
            }
        }

        const auto* config = FLHookConfig::c();
        if (config->general.disableNPCSpawns && CoreGlobals::c()->serverLoadInMs >= config->general.disableNPCSpawns)
        {
            Hk::Admin::ChangeNPCSpawn(true); // serverload too high, disable npcs
        }
        else
        {
            Hk::Admin::ChangeNPCSpawn(false);
        }
    }
    CATCH_HOOK({})
}

/**************************************************************************************************************
**************************************************************************************************************/

CRITICAL_SECTION csIPResolve;
std::list<RESOLVE_IP> resolveIPs;
std::list<RESOLVE_IP> resolveIPsResult;
HANDLE hThreadResolver;

void ThreadResolver()
{
    TRY_HOOK
    {
        while (true)
        {
            EnterCriticalSection(&csIPResolve);
            std::list<RESOLVE_IP> ips = resolveIPs;
            resolveIPs.clear();
            LeaveCriticalSection(&csIPResolve);

            for (auto& [client, connects, IP, hostname] : ips)
            {
                SOCKADDR_IN addr{ AF_INET, 2302, {}, { 0 } };
                InetPtonW(AF_INET, IP.c_str(), &addr.sin_addr);

                wchar_t hostbuf[255];
                GetNameInfoW(reinterpret_cast<const SOCKADDR*>(&addr), sizeof addr, hostbuf, std::size(hostbuf), nullptr, 0, 0);

                hostname = hostbuf;
            }

            EnterCriticalSection(&csIPResolve);
            for (auto& ip : ips)
            {
                if (ip.hostname.length())
                {
                    resolveIPsResult.push_back(ip);
                }
            }
            LeaveCriticalSection(&csIPResolve);

            Sleep(50);
        }
    }
    CATCH_HOOK({})
}

/**************************************************************************************************************
**************************************************************************************************************/

void TimerCheckResolveResults()
{
    TRY_HOOK
    {
        EnterCriticalSection(&csIPResolve);
        for (const auto& [client, connects, IP, hostname] : resolveIPsResult)
        {
            if (connects != ClientInfo[client].connects)
            {
                continue; // outdated
            }

            // check if banned
            for (const auto* config = FLHookConfig::c(); const auto& ban : config->bans.banWildcardsAndIPs)
            {
                if (Wildcard::Fit(StringUtils::wstos(ban).c_str(), StringUtils::wstos(hostname).c_str()))
                {
                    // AddKickLog(ip.client, StringUtils::wstos(std::format(L"IP/hostname ban({} matches {})", ip.hostname.c_str(), ban.c_str())));
                    if (config->bans.banAccountOnMatch)
                    {
                        Hk::Player::Ban(client, true);
                    }
                    Hk::Player::Kick(client);
                }
            }
            ClientInfo[client].hostname = hostname;
        }

        resolveIPsResult.clear();
        LeaveCriticalSection(&csIPResolve);
    }
    CATCH_HOOK({})
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////