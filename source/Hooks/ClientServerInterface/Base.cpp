#include "PCH.hpp"

#include "API/FLHook/ClientList.hpp"
#include "API/Utils/PerfTimer.hpp"
#include "Core/ClientServerInterface.hpp"
#include "Core/Logger.hpp"

void BaseEnterInner([[maybe_unused]] uint baseId, [[maybe_unused]] ClientId client)
{
    // TODO: implement base enter event
}

void BaseEnterInnerAfter([[maybe_unused]] uint baseId, ClientId client)
{
    TryHook
    {
        auto& data = client.GetData();
        // adjust cash, this is necessary when cash was added while use was in charmenu/had other char selected
        const std::wstring charName = StringUtils::ToLower(client.GetCharacterName().Unwrap());
        for (const auto& i : data.moneyFix)
        {
            if (i.character == charName)
            {
                client.AddCash(i.amount);
                data.moneyFix.remove(i);
                break;
            }
        }

        data.baseId = BaseId(baseId);

        // anti base-idle
        data.baseEnterTime = static_cast<uint>(time(nullptr));

        // print to log if the char has too much money
        if (const auto value = client.GetWealth().Unwrap(); value > 2000000000)
        {
            FLHook::GetLogger().Log(LogLevel::Trace, std::format(L"Possible corrupt ship charname={} asset_value={}", charName, value));
        }
    }
    CatchHook({})
}

void __stdcall IServerImplHook::BaseEnter(uint baseId, ClientId client)
{
    FLHook::GetLogger().Log(LogLevel::Trace, std::format(L"BaseEnter(\n\tuint baseId = {}\n\tClientId client = {}\n)", baseId, client));

    const auto skip = CallPlugins(&Plugin::OnBaseEnter, baseId, client);

    CheckForDisconnect;

    BaseEnterInner(baseId, client);

    if (!skip)
    {
        CallServerPreamble { Server.BaseEnter(baseId, client.GetValue()); }
        CallServerPostamble(true, );
    }
    BaseEnterInnerAfter(baseId, client);

    CallPlugins(&Plugin::OnBaseEnterAfter, baseId, client);
}

void BaseExitInner(uint baseId, ClientId client)
{
    TryHook
    {
        auto& data = client.GetData();
        data.baseEnterTime = 0;
        data.lastExitedBaseId = baseId;
    }
    CatchHook({})
}

void BaseExitInnerAfter([[maybe_unused]] uint baseId, [[maybe_unused]] ClientId client)
{
    // TODO: implement base exit event
}
void __stdcall IServerImplHook::BaseExit(uint baseId, ClientId client)
{
    FLHook::GetLogger().Log(LogLevel::Trace, std::format(L"BaseExit(\n\tuint baseId = {}\n\tClientId client = {}\n)", baseId, client));

    const auto skip = CallPlugins(&Plugin::OnBaseExit, baseId, client);

    CheckForDisconnect;

    BaseExitInner(baseId, client);

    if (!skip)
    {
        CallServerPreamble { Server.BaseExit(baseId, client.GetValue()); }
        CallServerPostamble(true, );
    }
    BaseExitInnerAfter(baseId, client);

    CallPlugins(&Plugin::OnBaseExitAfter, baseId, client);
}

void __stdcall IServerImplHook::BaseInfoRequest(unsigned int unk1, unsigned int unk2, bool unk3)
{
    FLHook::GetLogger().Log(LogLevel::Trace,
                            std::format(L"BaseInfoRequest(\n\tunsigned int unk1 = {}\n\tunsigned int unk2 = {}\n\tbool unk3 = {}\n)", unk1, unk2, unk3));

    if (const auto skip = CallPlugins(&Plugin::OnRequestBaseInfo, unk1, unk2, unk3); !skip)
    {
        CallServerPreamble { Server.BaseInfoRequest(unk1, unk2, unk3); }
        CallServerPostamble(true, );
    }

    CallPlugins(&Plugin::OnRequestBaseInfoAfter, unk1, unk2, unk3);
}

void __stdcall IServerImplHook::Dock([[maybe_unused]] const uint& genArg1, [[maybe_unused]] const uint& genArg2) {}
