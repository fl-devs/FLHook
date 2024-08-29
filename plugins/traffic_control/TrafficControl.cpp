/**
 * @date Feb, 2010
 * @author Cannon, ported by Raikkonen
 * @defgroup SystemSensor System Sensor
 * @brief
 * The plugin allows players with proper equipment to see player traffic coming through
 * Trade Lanes and Jump Gates in the system, as well as being able to look up
 * their equipment and cargo at the time of using them.
 *
 * @paragraph cmds Player Commands
 * -net <all/jumponly/off> - if player has proper equipment, toggles his scanner between showing JG/TL transits,
 *   JG transits only, and disabling the feature
 * -shoan <name> - shows equipment and cargo carried by the specified player
 * -shoan$ <playerID> - same as above, but using player ID as paramenter, useful for to type difficult names
 *
 * @paragraph adminCmds Admin Commands
 * None
 *
 * @paragraph configuration Configuration
 * @code
 * {
 *     "sensors": [
 *         {"systemId": "Li01",
 *          "equipId": "li_gun01_mark01",
 *          "networkId": 1}
 *          ]
 * }
 * @endcode
 *
 * @paragraph ipc IPC Interfaces Exposed
 * This plugin does not expose any functionality.
 */

#include "PCH.hpp"

#include "TrafficControl.hpp"

#include "API/FLHook/InfocardManager.hpp"

using namespace Plugins;
using namespace magic_enum::bitwise_operators;

void TrafficControlPlugin::ActivateNetwork(const ClientId client, const NetworkId networkId, const Permissions& permissions)
{
    if (auto clientData = clientInfo[client.GetValue()]; clientData.availableNetworks.contains(networkId))
    {
        auto& newNetwork = config.networks.at(networkId);
        config.subscribedClients[client] = &newNetwork;

        clientData.activeNetwork = &newNetwork;
        clientData.basePermissions = permissions;
        clientData.currPermissions = permissions;

        client.Message(std::format(L"Traffic monitoring active for {} region.", config.networks.at(networkId).networkName));
    }
    else
    {
        client.Message(L"Error: selected network not available!");
    }
}

bool TrafficControlPlugin::OnSystemSwitchOutPacket(const ClientId client, FLPACKET_SYSTEM_SWITCH_OUT& packet)
{
    if (const auto activeNetwork = clientInfo[client.GetValue()].activeNetwork; activeNetwork)
    {
        // TODO: Fix once optimizer is embedded into core
        auto* jumpObj = reinterpret_cast<CSolar*>(CObject::Find(packet.jumpObjectId, CObject::CSOLAR_OBJECT));
        jumpObj->Release();

        const auto targetSystem = SystemId(jumpObj->jumpDestSystem);
        if (activeNetwork->networkSystems.contains(targetSystem))
        {
            return true;
        }

        client.Message(std::format(L"You have left the are of {} monitoring network", activeNetwork->networkName));

        for (const auto& availableNetworks = clientInfo[client.GetValue()].availableNetworks;
             const auto [permissions, network] : availableNetworks | std::views::values)
        {
            if (network->networkSystems.contains(targetSystem))
            {
                ActivateNetwork(client, network->networkId, permissions);
                break;
            }
        }
    }
    return true;
}

void TrafficControlPlugin::OnPlayerLaunchAfter(const ClientId client, const ShipId ship)
{
    auto& playerNetworks = clientInfo[client.GetValue()].availableNetworks;
    playerNetworks.clear();

    for (EquipDesc& equip : *client.GetEquipCargo().Handle())
    {
        if (const auto findResult = config.equipAccesses.find(equip.archId); findResult != config.equipAccesses.end())
        {
            playerNetworks[findResult->second.second->networkId] = findResult->second;
        }
    }

    if (const auto findResult = config.IFFAccesses.find(client.GetReputation().Handle().GetAffiliation().Handle().GetValue());
        findResult != config.IFFAccesses.end())
    {
        playerNetworks[findResult->second.second->networkId] = findResult->second;
    }

    if (playerNetworks.empty())
    {
        return;
    }

    ActivateNetwork(client, playerNetworks.begin()->first, playerNetworks.begin()->second.first);
}

void TrafficControlPlugin::AddShipCargoSnapshot(ClientId client) const
{
    auto clientData = clientInfo[client.GetValue()];

    clientData.scanCache.clear();

    clientData.scanCache.emplace_back(std::format(L"Scan snapshot of {}:", client.GetCharacterName().Handle()));
    clientData.scanCache.emplace_back(std::format(L"Ship: {}", FLHook::GetInfocardManager().GetInfocard(client.GetShip().Handle()->archetype->idsName)));
    auto& shipEqManager = client.GetShip().Handle()->equip_manager;
    CEquipTraverser tr(static_cast<int>(EquipmentClass::Cargo));
    CECargo* cargo;
    while (cargo = reinterpret_cast<CECargo*>(shipEqManager.Traverse(tr)))
    {
        clientData.scanCache.emplace_back(std::format(L"| {}x{}", FLHook::GetInfocardManager().GetInfocard(cargo->archetype->idsInfo), cargo->count));
    }

    clientData.timestamp = TimeUtils::UnixTime<std::chrono::seconds>();
}

void TrafficControlPlugin::OnTradelaneStart(ClientId client, const XGoTradelane& tradelane)
{
    const auto networksToNotify = config.systemToNetworkMap.find(client.GetSystemId().Handle());
    if (networksToNotify == config.systemToNetworkMap.end())
    {
        return;
    }

    bool isFirstNotification = true;

    std::vector<std::wstring> message;
    for (const auto& notifiedClient : config.subscribedClients)
    {
        if (networksToNotify->second.contains(notifiedClient.second))
        {
            continue;
        }
        if (magic_enum::enum_flags_test(clientInfo[notifiedClient.first.GetValue()].currPermissions, Permissions::LaneNet))
        {
            SystemId clientSystem = notifiedClient.first.GetSystemId().Handle();
            notifiedClient.first.Message(std::format(L"{} has entered tradelane in {} system, sector {}",
                                               client.GetCharacterName().Handle(),
                                               clientSystem.GetName().Handle(),
                                               clientSystem.PositionToSectorCoord(notifiedClient.first.GetShip().Handle()->position).Handle()));
        }

        if (isFirstNotification && magic_enum::enum_flags_test(clientInfo[notifiedClient.first.GetValue()].currPermissions, Permissions::Scan))
        {
            isFirstNotification = false;
            AddShipCargoSnapshot(client);
        }
    }
}

void TrafficControlPlugin::OnClearClientInfo(const ClientId client)
{
    auto clientData = clientInfo[client.GetValue()];
    clientData.availableNetworks.clear();
    clientData.activeNetwork = nullptr;
}

std::optional<DOCK_HOST_RESPONSE> TrafficControlPlugin::OnDockCall(ShipId shipId, ObjectId spaceId, int dockPortIndex, DOCK_HOST_RESPONSE response)
{
    const auto client = ClientId(shipId.GetPlayer().value());
    if (!client)
    {
        return {};
    }

    auto& data = clientInfo[client.GetValue()];
    if (!data.nodockTimestamp)
    {
        return {};
    }

    const auto currTime = TimeUtils::UnixTime<std::chrono::seconds>();
    if (data.nodockTimestamp > currTime)
    {
        return {};
    }

    client.Message(L"Your dock access has been temporarily suspended");
    return DOCK_HOST_RESPONSE::DockDenied;
}

void TrafficControlPlugin::OnLoadSettings()
{
    if (const auto conf = Json::Load<ConfigLoad>("config/traffic_control.json"); !conf.has_value())
    {
        Json::Save(configLoad, "config/traffic_control.json");
    }
    else
    {
        configLoad = conf.value();

        config.nodockDuration = configLoad.nodockDuration;
        config.nodockMessage = configLoad.nodockMessage;
        for(auto& network : configLoad.networks)
        {
            config.networks[network.networkId] = network;
        }

        for (auto& [networkId, perms, type, key] : configLoad.accessList)
        {
            if (type == "equip")
            {
                config.equipAccesses[CreateID(key.c_str())] = { perms, &config.networks.at(networkId) };
            }
            else if (type == "affiliation")
            {
                config.IFFAccesses[CreateID(key.c_str())] = { perms, &config.networks.at(networkId) };
            }
            else if (type == "tag")
            {
                config.tagsAccesses[StringUtils::stows(key)] = { perms, &config.networks.at(networkId) };
            }
        }

        for (auto network : config.networks | std::views::values)
        {
            for (auto system : network.networkSystems)
            {
                config.systemToNetworkMap[system].insert(&network);
            }
        }
    }
}

void TrafficControlPlugin::UserCmdNetSwitch(ClientId client, std::wstring_view networkName)
{
    auto clientData = clientInfo[client.GetValue()];
    if (clientData.availableNetworks.empty())
    {
        client.Message(L"No networks available!");
        return;
    }

    for (auto& [permissions, network] : clientData.availableNetworks | std::views::values)
    {
        if (!StringUtils::CompareCaseInsensitive(networkName, std::wstring_view(network->networkName)))
        {
            continue;
        }

        ActivateNetwork(client, network->networkId, permissions);
        return;
    }

    client.Message(L"Provided network doesn't exist or you don't have access");
}

void TrafficControlPlugin::UserCmdNetList(ClientId client)
{
    auto clientData = clientInfo[client.GetValue()];
    if (clientData.availableNetworks.empty())
    {
        client.Message(L"No networks available for connection!");
        return;
    }

    for (const auto network : clientData.availableNetworks | std::views::values | std::views::values)
    {
        if (network == clientData.activeNetwork)
        {
            client.Message(std::format(L"{} - Active", network->networkName));
        }
        else
        {
            client.Message(std::format(L"{}", network->networkName));
        }
    }
}

void TrafficControlPlugin::UserCmdNet(ClientId client, const std::wstring_view setting, const bool newState)
{
    auto clientData = clientInfo[client.GetValue()];
    if (!clientData.activeNetwork)
    {
        client.Message(L"No network connection!");
        return;
    }

    if (StringUtils::ToLower(setting) == L"lane")
    {
        if (!magic_enum::enum_flags_test(clientData.basePermissions, Permissions::LaneNet) && newState)
        {
            client.Message(L"No lane monitoring permissions!");
            return;
        }

        if (newState)
        {
            clientData.basePermissions |= Permissions::LaneNet;
        }
        else
        {
            clientData.basePermissions &= Permissions::LaneNet;
        }
    }
    else if (StringUtils::ToLower(setting) == L"gate")
    {
        if (!magic_enum::enum_flags_test(clientData.basePermissions, Permissions::GateNet) && newState)
        {
            client.Message(L"No gate monitoring permissions!");
            return;
        }

        if (newState)
        {
            clientData.basePermissions |= Permissions::GateNet;
        }
        else
        {
            clientData.basePermissions &= Permissions::GateNet;
        }
    }
    else
    {
        client.Message(L"Incorrect parameters!");
    }
}

void TrafficControlPlugin::UserCmdNodockInfo(ClientId client)
{
    const auto network = clientInfo[client.GetValue()].activeNetwork;
    if (!network)
    {
        client.Message(L"No network connection!");
        return;
    }

    if (network->nodockFactions.empty())
    {
        client.Message(L"This network doesn't allow of dock restrictions!");
        return;
    }

    client.Message(L"Restrictable station affiliations:");
    for (auto& faction : network->nodockFactions)
    {
        client.Message(std::format(L"| {}", faction.GetName().Handle()));
    }
}

void TrafficControlPlugin::UserCmdNodock(ClientId client)
{
    const auto data = clientInfo[client.GetValue()];

    if (!data.activeNetwork)
    {
        client.Message(L"No active traffic network!");
        return;
    }

    if (!magic_enum::enum_flags_test(data.currPermissions, Permissions::NoDock))
    {
        client.Message(L"No permission for this action!");
        return;
    }

    const auto ship = client.GetShip().Handle();

    if (!ship)
    {
        client.Message(L"Not in space!");
        return;
    }

    const auto target = ship->get_target();
    if (!target)
    {
        client.Message(L"No target!");
        return;
    }

    auto targetPlayerId = ClientId(reinterpret_cast<CSimple*>(target->cobject())->ownerPlayer);
    if (!targetPlayerId)
    {
        client.Message(L"Target not a player!");
        return;
    }

    targetPlayerId.Message(config.nodockMessage);
    clientInfo[targetPlayerId.GetValue()].nodockTimestamp = TimeUtils::UnixTime<std::chrono::seconds>() + config.nodockDuration;
}

TrafficControlPlugin::TrafficControlPlugin(const PluginInfo& info) : Plugin(info) {}
TrafficControlPlugin::~TrafficControlPlugin() = default;

DefaultDllMain();

const PluginInfo Info(L"TrafficControl", L"trafficcontrol", PluginMajorVersion::V05, PluginMinorVersion::V00);
SetupPlugin(TrafficControlPlugin, Info);
