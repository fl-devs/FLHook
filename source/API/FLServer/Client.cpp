#include "PCH.hpp"

#include "Global.hpp"
#include "API/FLServer/Client.hpp"

namespace Hk::Client
{

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Action<uint> GetClientIdFromAccount(const CAccount* acc)
    {
        PlayerData* playerDb = nullptr;
        while ((playerDb = Players.traverse_active(playerDb)))
        {
            if (playerDb->account == acc)
            {
                return { playerDb->onlineId };
            }
        }

        return { cpp::fail(Error::PlayerNotLoggedIn) };
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Action<CAccount*> GetAccountByCharName(std::wstring_view character)
    {
        st6::wstring fr((ushort*)character.data(), character.size());
        CAccount* acc = Players.FindAccountFromCharacterName(fr);

        if (!acc)
        {
            return { cpp::fail(Error::CharacterDoesNotExist) };
        }

        return { acc };
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Action<uint> GetClientIdFromCharName(std::wstring_view character)
    {
        const auto acc = GetAccountByCharName(character).Raw();
        if (acc.has_error())
        {
            return { cpp::fail(acc.error()) };
        }

        const auto client = GetClientIdFromAccount(acc.value()).Raw();
        if (client.has_error())
        {
            return { cpp::fail(client.error()) };
        }

        const auto newCharacter = GetCharacterNameByID(client.value()).Raw();
        if (newCharacter.has_error())
        {
            return { cpp::fail(newCharacter.error()) };
        }

        if (StringUtils::ToLower(newCharacter.value()) == StringUtils::ToLower(character))
        {
            return { cpp::fail(Error::CharacterDoesNotExist) };
        }

        return { client };
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Action<std::wstring> GetAccountID(CAccount* acc)
    {
        if (acc && acc->accId)
        {
            return { acc->accId };
        }

        return { cpp::fail(Error::CannotGetAccount) };
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // TODO: DEPRECATE
    bool IsEncoded(const std::wstring& fileName)
    {
        bool retVal = false;

        auto f = std::wifstream(fileName);

        if (!f)
        {
            return false;
        }

        // Checks first 4 bytes of a file and if the first 4 bytes are the listed string,
        const wchar_t magic[] = L"FLS1";
        wchar_t file[sizeof magic] = L"";
        f.read(0, sizeof magic);
        if (!wcsncmp(magic, file, sizeof magic - 1))
        {
            retVal = true;
        }

        f.close();

        return retVal;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    bool IsInCharSelectMenu(const uint& player)
    {
        ClientId client = ExtractClientID(player);
        if (client == UINT_MAX)
        {
            return false;
        }

        uint base = 0;
        uint system = 0;
        pub::Player::GetBase(client, base);
        pub::Player::GetSystem(client, system);
        if (!base && !system)
        {
            return true;
        }
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    bool IsValidClientID(ClientId client)
    {
        if (client == 0 || client >= 255)
        {
            return false;
        }

        PlayerData* playerDb = nullptr;
        while ((playerDb = Players.traverse_active(playerDb)))
        {
            if (playerDb->onlineId == client)
            {
                return true;
            }
        }

        return false;
    }

    Action<std::wstring> GetCharacterNameByID(ClientId client)
    {
        if (!IsValidClientID(client) || IsInCharSelectMenu(client))
        {
            return { cpp::fail(Error::InvalidClientId) };
        }

        return { reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(client)) };
    }

    Action<ClientId> ResolveShortCut(const std::wstring& shortcut)
    {
        std::wstring shortcutLower = StringUtils::ToLower(shortcut);
        if (shortcutLower.find(L"sc ") != 0)
        {
            return { cpp::fail(Error::InvalidShortcutString) };
        }

        shortcutLower = shortcutLower.substr(3);

        uint clientFound = UINT_MAX;
        PlayerData* playerDb = nullptr;
        while ((playerDb = Players.traverse_active(playerDb)))
        {
            const auto characterName = GetCharacterNameByID(playerDb->onlineId).Raw();
            if (characterName.has_error())
            {
                continue;
            }

            if (StringUtils::ToLower(characterName.value()).find(shortcutLower) != -1)
            {
                if (clientFound == UINT_MAX)
                {
                    clientFound = playerDb->onlineId;
                }
                else
                {
                    return { cpp::fail(Error::AmbiguousShortcut) };
                }
            }
        }

        if (clientFound == UINT_MAX)
        {
            return { cpp::fail(Error::NoMatchingPlayer) };
        }

        return { clientFound };
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Action<ClientId> GetClientIdByShip(const ShipId ship)
    {
        if (const auto foundClient = std::ranges::find_if(ClientInfo, [ship](const CLIENT_INFO& ci) { return ci.ship == ship; });
            foundClient != ClientInfo.end())
        {
            return { std::ranges::distance(ClientInfo.begin(), foundClient) };
        }

        return { cpp::fail(Error::InvalidShip) };
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::wstring GetAccountDirName(const CAccount* acc)
    {
        const auto GetFLName = reinterpret_cast<_GetFLName>(reinterpret_cast<char*>(server) + 0x66370);

        char Dir[1024] = "";
        GetFLName(Dir, acc->accId);
        return StringUtils::stows(Dir);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Action<std::wstring> GetCharFileName(const std::variant<uint, std::wstring_view>& player, bool returnValueIfNoFile)
    {
        static _GetFLName GetFLName = nullptr;
        if (!GetFLName)
        {
            GetFLName = (_GetFLName)((char*)server + 0x66370);
        }

        std::wstring buffer;
        buffer.reserve(1024);

        if (ClientId client = ExtractClientID(player); client != UINT_MAX)
        {
            if (const auto character = GetCharacterNameByID(client).Raw(); character.has_error())
            {
                return { cpp::fail(character.error()) };
            }

            GetFLName(reinterpret_cast<char*>(buffer.data()), reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(client)));
        }
        else if ((player.index() && GetAccountByCharName(std::get<std::wstring_view>(player)).Raw()) || returnValueIfNoFile)
        {
            GetFLName(reinterpret_cast<char*>(buffer.data()), std::get<std::wstring_view>(player).data());
        }
        else
        {
            return { cpp::fail(Error::InvalidClientId) };
        }

        return { buffer };
    }

    Action<std::wstring> GetCharFileName(const std::variant<uint, std::wstring_view>& player) { return GetCharFileName(player, false); }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Action<std::wstring> GetBaseNickByID(uint baseId)
    {
        std::wstring base;
        base.resize(1024);
        pub::GetBaseNickname(reinterpret_cast<char*>(base.data()), base.capacity(), baseId);
        base.resize(1024); // Without calling another core function will result in length not being updated

        if (base.empty())
        {
            return { cpp::fail(Error::InvalidBase) };
        }

        return { base };
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Action<std::wstring> GetSystemNickByID(uint systemId)
    {
        std::wstring system;
        system.resize(1024);
        pub::GetSystemNickname(reinterpret_cast<char*>(system.data()), system.capacity(), systemId);
        system.resize(1024); // Without calling another core function will result in length not being updated

        if (system.empty())
        {
            return { cpp::fail(Error::InvalidSystem) };
        }

        return { system };
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Action<std::wstring> GetPlayerSystem(ClientId client)
    {
        if (!IsValidClientID(client))
        {
            return { cpp::fail(Error::InvalidClientId) };
        }

        uint systemId;
        pub::Player::GetSystem(client, systemId);
        return GetSystemNickByID(systemId);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Action<void> LockAccountAccess(CAccount* acc, bool kick)
    {
        const std::array<char, 1> jmp = { '\xEB' };
        const std::array<char, 1> jbe = { '\x76' };

        const auto accountId = GetAccountID(acc).Raw();
        if (accountId.has_error())
        {
            return { cpp::fail(accountId.error()) };
        }

        st6::wstring fr((ushort*)accountId.value().c_str());

        if (!kick)
        {
            MemUtils::WriteProcMem((void*)0x06D52A6A, jmp.data(), 1);
        }

        Players.LockAccountAccess(fr); // also kicks player on this account
        if (!kick)
        {
            MemUtils::WriteProcMem((void*)0x06D52A6A, jbe.data(), 1);
        }

        return { {} };
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Action<void> UnlockAccountAccess(CAccount* acc)
    {
        const auto accountId = GetAccountID(acc).Raw();
        if (accountId.has_error())
        {
            return { cpp::fail(accountId.error()) };
        }

        st6::wstring fr((ushort*)accountId.value().c_str());
        Players.UnlockAccountAccess(fr);
        return { {} };
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void GetItemsForSale(uint baseId, std::list<uint>& items)
    {
        items.clear();
        const std::array<char, 2> nop = { '\x90', '\x90' };
        const std::array<char, 2> jnz = { '\x75', '\x1D' };
        MemUtils::WriteProcMem(SRV_ADDR(ADDR_SRV_GETCOMMODITIES), nop.data(), 2); // patch, else we only get commodities

        std::array<int, 1024> arr;
        int size = 256;
        pub::Market::GetCommoditiesForSale(baseId, reinterpret_cast<uint* const>(arr.data()), &size);
        MemUtils::WriteProcMem(SRV_ADDR(ADDR_SRV_GETCOMMODITIES), jnz.data(), 2);

        for (int i = 0; i < size; i++)
        {
            items.push_back(arr[i]);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Action<IObjInspectImpl*> GetInspect(ClientId client)
    {
        uint ship;
        pub::Player::GetShip(client, ship);
        uint dunno;
        IObjInspectImpl* inspect;
        if (!GetShipInspect(ship, inspect, dunno))
        {
            return { cpp::fail(Error::InvalidShip) };
        }
        return { inspect };
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    EngineState GetEngineState(ClientId client)
    {
        if (ClientInfo[client].tradelane)
        {
            return ES_TRADELANE;
        }
        if (ClientInfo[client].cruiseActivated)
        {
            return ES_CRUISE;
        }
        if (ClientInfo[client].thrusterActivated)
        {
            return ES_THRUSTER;
        }
        if (!ClientInfo[client].engineKilled)
        {
            return ES_ENGINE;
        }
        return ES_KILLED;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    EquipmentType GetEqType(Archetype::Equipment* eq)
    {
        const uint VFTableMine = (uint)common + ADDR_COMMON_VFTABLE_MINE;
        const uint VFTableCM = (uint)common + ADDR_COMMON_VFTABLE_CM;
        const uint VFTableGun = (uint)common + ADDR_COMMON_VFTABLE_GUN;
        const uint VFTableShieldGen = (uint)common + ADDR_COMMON_VFTABLE_SHIELDGEN;
        const uint VFTableThruster = (uint)common + ADDR_COMMON_VFTABLE_THRUSTER;
        const uint VFTableShieldBat = (uint)common + ADDR_COMMON_VFTABLE_SHIELDBAT;
        const uint VFTableNanoBot = (uint)common + ADDR_COMMON_VFTABLE_NANOBOT;
        const uint VFTableMunition = (uint)common + ADDR_COMMON_VFTABLE_MUNITION;
        const uint VFTableEngine = (uint)common + ADDR_COMMON_VFTABLE_ENGINE;
        const uint VFTableScanner = (uint)common + ADDR_COMMON_VFTABLE_SCANNER;
        const uint VFTableTractor = (uint)common + ADDR_COMMON_VFTABLE_TRACTOR;
        const uint VFTableLight = (uint)common + ADDR_COMMON_VFTABLE_LIGHT;

        const uint VFTable = *(uint*)eq;
        if (VFTable == VFTableGun)
        {
            const Archetype::Gun* gun = static_cast<Archetype::Gun*>(eq);
            Archetype::Equipment* eqAmmo = Archetype::GetEquipment(gun->projectileArchId);
            int missile;
            memcpy(&missile, (char*)eqAmmo + 0x90, 4);
            const uint gunType = gun->get_hp_type_by_index(0);
            if (gunType == 36)
            {
                return ET_TORPEDO;
            }
            if (gunType == 35)
            {
                return ET_CD;
            }
            if (missile)
            {
                return ET_MISSILE;
            }
            return ET_GUN;
        }
        if (VFTable == VFTableCM)
        {
            return ET_CM;
        }
        if (VFTable == VFTableShieldGen)
        {
            return ET_SHIELDGEN;
        }
        if (VFTable == VFTableThruster)
        {
            return ET_THRUSTER;
        }
        if (VFTable == VFTableShieldBat)
        {
            return ET_SHIELDBAT;
        }
        if (VFTable == VFTableNanoBot)
        {
            return ET_NANOBOT;
        }
        if (VFTable == VFTableMunition)
        {
            return ET_MUNITION;
        }
        if (VFTable == VFTableMine)
        {
            return ET_MINE;
        }
        if (VFTable == VFTableEngine)
        {
            return ET_ENGINE;
        }
        if (VFTable == VFTableLight)
        {
            return ET_LIGHT;
        }
        if (VFTable == VFTableScanner)
        {
            return ET_SCANNER;
        }
        if (VFTable == VFTableTractor)
        {
            return ET_TRACTOR;
        }
        return ET_OTHER;
    }

    
    uint ExtractClientID(const std::variant<uint, std::wstring_view>& player)
    {
        // If index is 0, we just use the client Id we are given
        if (!player.index())
        {
            const uint id = std::get<uint>(player);
            return IsValidClientID(id) ? id : -1;
        }

        // Otherwise we have a character name
        const std::wstring_view characterName = std::get<std::wstring_view>(player);


        const auto client = GetClientIdFromCharName(characterName).Raw();
        if (client.has_error())
        {
            return -1;
        }

        return client.value();
    }

    cpp::result<CAccount*, Error> ExtractAccount(const std::variant<uint, std::wstring_view>& player)
    {
        if (ClientId client = ExtractClientID(player); client != UINT_MAX)
        {
            return Players.FindAccountFromClientID(client);
        }

        if (!player.index())
        {
            return nullptr;
        }

        const auto acc = GetAccountByCharName(std::get<std::wstring_view>(player)).Raw();
        if (acc.has_error())
        {
            return { cpp::fail(acc.error()) };
        }

        return acc.value();
    }

    CAccount* GetAccountByClientID(ClientId client)
    {
        if (!IsValidClientID(client))
        {
            return nullptr;
        }

        return Players.FindAccountFromClientID(client);
    }

    std::wstring GetAccountIdByClientID(ClientId client)
    {
        if (IsValidClientID(client))
        {
            const CAccount* acc = GetAccountByClientID(client);
            if (acc && acc->accId)
            {
                return acc->accId;
            }
        }
        return L"";
    }

    Action<void> PlaySoundEffect(ClientId client, uint soundId)
    {
        if (IsValidClientID(client))
        {
            pub::Audio::PlaySoundEffect(client, soundId);
            return { {} };
        }
        return { cpp::fail(Error::PlayerNotLoggedIn) };
    }

    std::vector<uint> getAllPlayersInSystem(SystemId system)
    {
        PlayerData* playerData = nullptr;
        std::vector<uint> playersInSystem;
        while ((playerData = Players.traverse_active(playerData)))
        {
            if (playerData->systemId == system)
            {
                playersInSystem.push_back(playerData->onlineId);
            }
        }
        return playersInSystem;
    }
} // namespace Hk::Client