﻿

#include "PCH.hpp"

#include "KillTracker.hpp"

#include "API/FLHook/ClientList.hpp"
#include "API/Utils/Random.hpp"

namespace Plugins
{
    void KillTrackerPlugin::ClearDamageTaken(const ClientId client) { damageArray[client.GetValue()].fill({ 0.0f, 0.0f }); }
    void KillTrackerPlugin::ClearDamageDone(const ClientId client, const bool fullReset)
    {
        for (int i = 1; i <= MaxClientId; i++)
        {
            auto& [currDamage, lastUndockDamage] = damageArray[i][client.GetValue()];
            if (fullReset)
            {
                lastUndockDamage = 0.0f;
            }
            else
            {
                lastUndockDamage = currDamage;
            }
            currDamage = 0.0f;
        }
    }
    bool KillTrackerPlugin::OnLoadSettings()
    {
        LoadJsonWithValidation(Config, config, "config/killtracker.json");

        return true;
    }

    void KillTrackerPlugin::OnCharacterSelectAfter(ClientId client)
    {
        ClearDamageTaken(client);
        ClearDamageDone(client, true);
    }
    void KillTrackerPlugin::OnDisconnectAfter(ClientId client, EFLConnection connection)
    {
        ClearDamageTaken(client);
        ClearDamageDone(client, true);
    }
    void KillTrackerPlugin::OnClearClientInfo(ClientId client)
    {
        ClearDamageTaken(client);
        ClearDamageDone(client, true);
    }
    void KillTrackerPlugin::OnPlayerLaunchAfter(ClientId client, ShipId ship)
    {
        ClearDamageTaken(client);
        ClearDamageDone(client, false);
    }

    void KillTrackerPlugin::OnShipHullDmg(Ship* ship, float& damage, DamageList* dmgList)
    {
        if (!dmgList->inflictorPlayerId)
        {
            return;
        }

        if (const auto shipOwnerPlayer = ship->cship()->ownerPlayer; shipOwnerPlayer && shipOwnerPlayer != dmgList->inflictorPlayerId && damage > 0.0f)
        {
            damageArray[shipOwnerPlayer][dmgList->inflictorPlayerId].currDamage += damage;
        }
    }

    float KillTrackerPlugin::GetDamageDone(const DamageDone& damageDone)
    {
        if (damageDone.currDamage != 0.0f)
        {
            return damageDone.currDamage;
        }
        if (damageDone.lastUndockDamage != 0.0f)
        {
            return damageDone.lastUndockDamage;
        }
        return 0.0f;
    }

    enum MessageType
    {
        MSGNONE,
        MSGBLUE,
        MSGRED,
        MSGDARKRED
    };

    MessageType GetMessageType(const ClientId victimId, const PlayerData* pd, const SystemId system, bool isGroupInvolved)
    {
        const auto dieMsgType = victimId.GetData().dieMsg;
        if (dieMsgType == DieMsgType::None)
        {
            return MSGNONE;
        }

        if (isGroupInvolved)
        {
            return MSGBLUE;
        }

        if (dieMsgType == DieMsgType::Self)
        {
            if (pd->clientId == victimId.GetValue())
            {
                return MSGBLUE;
            }
        }
        else if (dieMsgType == DieMsgType::System)
        {
            if (pd->clientId == victimId.GetValue())
            {
                return MSGBLUE;
            }
            if (pd->systemId == system.GetValue())
            {
                return MSGRED;
            }
        }
        else if (dieMsgType == DieMsgType::All)
        {
            if (pd->clientId == victimId.GetValue())
            {
                return MSGBLUE;
            }
            if (pd->systemId == system.GetValue())
            {
                return MSGRED;
            }
            return MSGDARKRED;
        }
        return MSGNONE;
    }

    void ProcessDeath(ClientId victimId, const std::wstring_view message1, const std::wstring_view message2, const SystemId system, bool isPvP,
                      const std::set<CPlayerGroup*>& involvedGroups, const std::set<ClientId>& involvedPlayers)
    {
        const std::wstring deathMessageBlue1 = std::format(L"<TRA data=\"0xFF000001" // Blue, Bold
                                                           L"\" mask=\"-1\"/><TEXT>{}</TEXT>",
                                                           message1);
        const std::wstring deathMessageRed1 = std::format(L"<TRA data=\"0x0000CC01" // Red, Bold
                                                          L"\" mask=\"-1\"/><TEXT>{}</TEXT>",
                                                          message1);
        const std::wstring deathMessageDarkRed = std::format(L"<TRA data=\"0x18188c01" // Dark Red, Bold
                                                             L"\" mask=\"-1\"/><TEXT>{}</TEXT>",
                                                             message1);

        std::wstring deathMessageRed2;
        std::wstring deathMessageBlue2;
        if (!message2.empty())
        {
            deathMessageRed2 = std::format(L"<TRA data=\"0x0000CC01" // Red, Bold
                                           L"\" mask=\"-1\"/><TEXT>{}</TEXT>",
                                           message2);

            deathMessageBlue2 = std::format(L"<TRA data=\"0xFF000001" // Blue, Bold
                                            L"\" mask=\"-1\"/><TEXT>{}</TEXT>",
                                            message2);
        }

        PlayerData* pd = nullptr;
        while (pd = Players.traverse_active(pd))
        {
            const bool isInvolved = involvedGroups.contains(pd->playerGroup) || involvedPlayers.contains(ClientId(pd->clientId));

            if (const MessageType msgType = GetMessageType(victimId, pd, system, isInvolved); msgType == MSGBLUE)
            {
                if (isPvP)
                {
                    (void)ClientId(pd->clientId).MessageCustomXml(deathMessageBlue1);
                    if (!message2.empty())
                    {
                        (void)ClientId(pd->clientId).MessageCustomXml(deathMessageBlue2);
                    }
                }
                else
                {
                    (void)ClientId(pd->clientId).MessageCustomXml(deathMessageRed1);
                    if (!message2.empty())
                    {
                        (void)ClientId(pd->clientId).MessageCustomXml(deathMessageRed2);
                    }
                }
            }
            else if (msgType == MSGRED)
            {
                (void)ClientId(pd->clientId).MessageCustomXml(deathMessageRed1);
                if (!message2.empty())
                {
                    (void)ClientId(pd->clientId).MessageCustomXml(deathMessageRed2);
                }
            }
            else if (msgType == MSGDARKRED)
            {
                (void)ClientId(pd->clientId).MessageCustomXml(deathMessageDarkRed);
            }
        }
    }

    std::wstring KillTrackerPlugin::SelectRandomDeathMessage(const ClientId client)
    {
        const uint shipClass = client.GetShipArch().Handle()->shipClass;
        if (const auto& deathMsgList = config.shipClassToDeathMsgMap.find(shipClass); deathMsgList == config.shipClassToDeathMsgMap.end())
        {
            return config.defaultDeathDamageTemplate;
        }
        else
        {
            const uint randomIndex = Random::Uniform(0u, deathMsgList->second.size() - 1);
            return deathMsgList->second.at(randomIndex);
        }
    }

    void KillTrackerPlugin::OnSendDeathMessage(ClientId killer, ClientId victim, SystemId system, std::wstring_view msg)
    {
        returnCode = ReturnCode::SkipAll;

        std::map<float, ClientId> damageToInflictorMap; // damage is the key instead of value because keys are sorted, used to render top contributors in order
        std::set<CPlayerGroup*> involvedGroups;
        std::set<ClientId> involvedPlayers;

        float totalDamageTaken = 0.0f;
        PlayerData* pd = nullptr;
        while (pd = Players.traverse_active(pd))
        {
            auto& damageData = damageArray[victim.GetValue()][pd->clientId];
            float damageToAdd = GetDamageDone(damageData);

            if (pd->playerGroup && (pd->clientId == victim.GetValue() || damageToAdd > 0.0f))
            {
                involvedGroups.insert(pd->playerGroup);
            }
            else if (damageToAdd > 0.0f)
            {
                involvedPlayers.insert(ClientId(pd->clientId));
            }
            if (damageToAdd == 0.0f)
            {
                continue;
            }

            damageToInflictorMap[damageToAdd] = ClientId(pd->clientId);
            totalDamageTaken += damageToAdd;
        }

        if (killer && totalDamageTaken == 0.0f)
        {
            return;
        }

        const Archetype::Ship* shipArch = victim.GetShipArch().Handle();

        if (totalDamageTaken < shipArch->hitPoints * 0.02 && (!killer || killer == victim))
        {
            ClearDamageTaken(victim);
            ProcessDeath(victim, StringUtils::XmlText(msg), nullptr, system, false, involvedGroups, involvedPlayers);
            return;
        }

        auto victimName = victim.GetCharacterName().Handle();

        std::wstring deathMessage = SelectRandomDeathMessage(victim);
        std::wstring assistMessage;

        uint killerCounter = 0;

        for (auto i = damageToInflictorMap.rbegin(); i != damageToInflictorMap.rend(); ++i) // top values at the end
        {
            if (i == damageToInflictorMap.rend() || killerCounter >= config.numberOfListedKillers)
            {
                break;
            }
            float contributionPercentage = round(i->first / totalDamageTaken);
            if (contributionPercentage < config.minimumAssistancePercentage)
            {
                break;
            }

            contributionPercentage *= 100;

            std::wstring_view inflictorName = ClientId(i->second).GetCharacterName().Handle();
            if (killerCounter == 0)
            {
                deathMessage = std::vformat(deathMessage, std::make_wformat_args(victimName, inflictorName, contributionPercentage));
            }
            else if (killerCounter == 1)
            {
                assistMessage = std::vformat(L"Assisted by: {0} ({1:0.0f}%)", std::make_wformat_args(inflictorName, contributionPercentage));
            }
            else
            {
                assistMessage += std::vformat(assistMessage + L", {0} ({1:0.0f}%)", std::make_wformat_args(inflictorName, contributionPercentage));
            }
            killerCounter++;
        }

        if (assistMessage.empty())
        {
            ProcessDeath(victim, StringUtils::XmlText(deathMessage), L"", system, true, involvedGroups, involvedPlayers);
        }
        else
        {
            ProcessDeath(victim, StringUtils::XmlText(deathMessage), StringUtils::XmlText(assistMessage), system, true, involvedGroups, involvedPlayers);
        }
        ClearDamageTaken(victim);
    }

    KillTrackerPlugin::KillTrackerPlugin(const PluginInfo& info) : Plugin(info) {}
} // namespace Plugins

using namespace Plugins;

DefaultDllMain();

const PluginInfo Info(L"KillTracker", L"killtracker", PluginMajorVersion::V05, PluginMinorVersion::V00);
SetupPlugin(KillTrackerPlugin, Info);
