#include "PCH.hpp"

#include "Core/ClientServerInterface.hpp"
#include "Global.hpp"

#include "API/API.hpp"
#include "API/Utils/PerfTimer.hpp"

void PlayerLaunchInner(uint shipId, ClientId client)
{
    TryHook
    {
        auto& data = client.GetData();
        data.ship = ShipId(shipId);
        data.killsInARow = 0;
        data.cruiseActivated = false;
        data.thrusterActivated = false;
        data.engineKilled = false;
        data.inTradelane = false;

        // adjust cash, this is necessary when cash was added while use was in charmenu/had other char selected
        std::wstring charName = StringUtils::ToLower(client.GetCharacterName().Handle());
        for (const auto& i : data.moneyFix)
        {
            if (i.character == charName)
            {
                Hk::Player::AddCash(charName, i.amount);
                data.moneyFix.remove(i);
                break;
            }
        }
    }
    CatchHook({})
}

void PlayerLaunchInnerAfter([[maybe_unused]] uint shipId, ClientId client)
{
    TryHook
    {
        auto& data = client.GetData();
        if (!data.lastExitedBaseId)
        {
            data.lastExitedBaseId = 1;
        }
    }
    CatchHook({})
}

void __stdcall IServerImplHook::PlayerLaunch(uint shipId, ClientId client)
{
    FLHook::GetLogger().Log(LogLevel::Trace, std::format(L"PlayerLaunch(\n\tuint shipId = {}\n\tClientId client = {}\n)", shipId, client));

    const auto skip = CallPlugins(&Plugin::OnPlayerLaunch, client, shipId);

    CheckForDisconnect;

    PlayerLaunchInner(shipId, client);

    if (!skip)
    {
        CallServerPreamble { Server.PlayerLaunch(shipId, client.GetValue()); }
        CallServerPostamble(true, );
    }
    PlayerLaunchInnerAfter(shipId, client);

    CallPlugins(&Plugin::OnPlayerLaunchAfter, client, shipId);
}
