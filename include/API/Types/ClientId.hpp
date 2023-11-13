#pragma once

#include "API/Types/BaseId.hpp"
#include "API/Types/ShipId.hpp"
#include "API/Types/SystemId.hpp"

class ShipId;
class ClientData;
class GroupId;

// All methods associated with ClientId will return a failure of Invalid clientId if the client Id is not an active client or outside acceptable range (1 -255)
class ClientId
{
        uint value = 0;

        [[nodiscard]]
        Action<void, Error> AdjustCash(int amount) const;

        [[nodiscard]]
        bool IsValidClientId() const;

        [[nodiscard]]
        static uint GetClientIdFromCharacterName(std::wstring_view name);

    public:
        explicit ClientId(const uint val) : value(val) {}
        explicit ClientId(const SpecialChatIds id) { value = static_cast<const uint>(id); }
        explicit ClientId(const std::wstring_view str) : value(GetClientIdFromCharacterName(str)){};
        explicit ClientId() = default;

        explicit operator uint() const noexcept { return value; }
        bool operator==(const ClientId next) const { return value == next.value; }
        explicit operator bool() const;

        // Returns the underlying value of the ClientId, it is generally recommended to not use this.

        [[nodiscard]]
        uint GetValue() const
        {
            return value;
        }

        // Type Conversions

        /**
         * @brief Gets the character name of the character the client is logged in as.
         * @returns On success : string_view of the name
         * @returns On fail : Will return error code of character not selected if in character select.
         */
        [[nodiscard]]
        Action<std::wstring_view, Error> GetCharacterName() const;

        /**
         * @brief Gets the BaseId of the base the client is docked on if they are.
         * @returns On success : BaseId of the base the client is docked on
         * @returns On fail : Error code of PlayerNotDocked or InCharacterSelect
         */
        [[nodiscard]]
        Action<BaseId, Error> GetCurrentBase() const;

        /**
         * @brief Gets the SystemId of the system the client currently is in.
         * @returns On success : BaseId of the base the client is docked on
         * @returns On fail : Error code of InvalidSystem or InCharacterSelect.
         */
        [[nodiscard]]
        Action<SystemId, Error> GetSystemId() const;

        /**
         * @brief Gets the Account of the clientId
         * @returns On success : CAccount pointer of the Account.
         * @returns On fail : InvalidClientId if the client check fails.
         */
        [[nodiscard]]
        Action<CAccount *, Error> GetAccount() const;

        /**
         * @brief Gets the Shiparch of the ship the client is using
         * @returns On success : Archetype::Ship pointer.
         * @returns On fail : InvalidShip error
         */
        [[nodiscard]]
        Action<const Archetype::Ship *, Error> GetShipArch() const;

        /**
         * @brief Gets the ShipId of the ship the client is using
         * @returns On success : ShipId
         * @returns On fail : InvalidShip error or InCharacterSelect error
         */
        [[nodiscard]]
        Action<ShipId, Error> GetShipId() const;

        /**
         * @brief Gets the Group of the player
         * @returns On success : CPlayerGroup pointer
         * @returns On fail : PlayerNotInGroup error or InCharacterSelect.
         */
        [[nodiscard]]
        Action<CPlayerGroup *, Error> GetGroup() const;

        /**
         * @brief Gets the Reputation of the player
         * @returns On success : RepId
         * @returns On fail : InvalidReputation error  or InCharacterSelect.
         */
        [[nodiscard]]
        Action<RepId, Error> GetReputation() const;

        /**
         * @brief Gets the CShip of the ship the player is using
         * @returns On success : CShip pointer
         * @returns On fail : PlayerNotInSpace or InCharacterSelect.
         */
        [[nodiscard]]
        Action<CShip *, Error> GetShip() const;

        /**
         * @brief Gets the rank of the character the client is logged on as
         * @returns On success : Returns the rank of the player.
         * @returns On fail : InCharacterSelect
         */
        [[nodiscard]]
        Action<uint, Error> GetRank() const;

        /**
         * @brief Gets the wealth, a combination of asset value and cash
         * @returns On success : Returns the wealth of the player.
         * @returns On fail : InCharacterSelect
         */
        [[nodiscard]]
        Action<uint, Error> GetWealth() const;

        /**
         * @brief Gets PVP kills of the character the player is logged in as
         * @returns On success : Returns the kill count as an integer.
         * @returns On fail : InCharacterSelect
         */
        [[nodiscard]]
        Action<int, Error> GetPvpKills() const;

        /**
         * @brief Gets the cash of the character the player is logged in as.
         * @returns On success : Returns cash as an unsigned int.
         * @returns On fail : InCharacterSelect
         */
        [[nodiscard]]
        Action<uint, Error> GetCash() const;

        /**
         * @brief Gets the cargo items in the ship of the player.
         * @returns On success : List of CargoInfo of the cargo the player has.
         * @returns On fail : InCharacterSelect
         */
        [[nodiscard]]
        Action<std::list<CargoInfo>, Error> EnumCargo(int &remainingHoldSize) const;

        /**
         * @brief Gets overall data related to the clientID
         * @returns On success : ClientData associated with that clientId
         * @returns On fail : InvalidClientId
         */
        [[nodiscard]]
        ClientData &GetData() const;
        [[nodiscard]]
        EngineState GetEngineState() const;

        // State Checks

        /**
         * @brief Checks if the player is in Space.
         * @returns On success : True if player is in space, false if they are docked or in character select.
         * @returns On fail : No fail condition.
         */
        [[nodiscard]]
        bool InSpace() const;

        /**
         * @brief Checks if the player is docked at a base.
         * @returns On success : True if player is docked, false if they are in space or in character select.
         * @returns On fail : No fail condition.
         */
        [[nodiscard]]
        bool IsDocked() const;

        /**
         * @brief Checks if the player is in character select.
         * @returns On success : True if player is in character select, false if they are in space or docked.
         * @returns On fail : No fail condition.
         */
        [[nodiscard]]
        bool InCharacterSelect() const;

        /**
         * @brief Checks if the player is currently alive
         * @returns On success : True if the player is alive or in Character Select, False if dead and on the Respawn menu.
         * @returns On fail : No fail condition.
         */
        [[nodiscard]]
        bool IsAlive() const;

        // Manipulation

        /**
         * Adds the player to the group associated with the GroupId
         * @param group the group you want to add the player to
         * @returns On success : void
         * @returns On fail : InCharacterSelect or InvalidGroup
         */
        Action<void, Error> AddToGroup(GroupId group) const;

        /**
         * Kicks the player
         * @param  reason if provided will post a global message, delay: if provided will kick them after that in seconds.
         * @param  delay if provided will kick them after that in seconds.
         * @returns On success : void
         * @returns On fail : InCharacterSelect
         */
        Action<void, Error> Kick(const std::optional<std::wstring_view> &reason = {}, std::optional<uint> delay = {}) const;

        /**
         * Saves the account of the player
         * @returns On success : void
         * @returns On fail : InCharacterSelect
         */
        Action<void, Error> SaveChar() const;

        /**
         * Sets the pvp kills of the character the client is logged in as.
         * @param killAmount the amount of kills you want to set the character to.
         * @returns On success : void
         * @returns On fail : InCharacterSelect
         */
        Action<void, Error> SetPvpKills(uint killAmount) const;

        /**
         * Adds specified amount of cash to the character the client is logged in as.
         * @param amount the amount of cash you wish to add to the character.
         * @returns On success : void
         * @returns On fail : InCharacterSelect
         */
        Action<void, Error> AddCash(uint amount) const;

        /**
         * Removes specified amount of cash to the character the client is logged in as.
         * @param amount the amount of cash you wish to remove from the character.
         * @returns On success : void
         * @returns On fail : InCharacterSelect
         */
        Action<void, Error> RemoveCash(uint amount) const;

        /**
         * Beams the character the client is logged on to a specified base, will fail if they are docked.
         * @param base either the BaseId or name of the base you wish to beam the client to.
         * @returns On success : void
         * @returns On fail : InCharacterSelect or PlayerNotInSpace
         */
        Action<void, Error> Beam(std::variant<BaseId, std::wstring_view> base) const;

        /**
         * Renames and kicks the character the player is logged in as.
         * @param name the name you wish to rename the character to.
         * @returns On success :
         * @returns On fail : InCharacterSelect.
         */
        Action<void, Error> Rename(std::wstring_view name) const;

        void MarkObject(uint objId, int markStatus) const;

        // Chat

        /**
         * Messages the player with provided text and format/color.
         * @param message the text you wish to send.
         * @param format format style of the message
         * @param color text color of the message
         * @returns On success : void
         * @returns On fail : InCharacterSelect.
         */
        Action<void, Error> Message(std::wstring_view message, MessageFormat format = MessageFormat::Normal, MessageColor color = MessageColor::Default) const;

        /**
         * sends a local message to the player and other players within a specified radius.
         * @param message the text you wish to send.
         * @param range the range in meters of which other players will receive the message
         * @param format format style of the message
         * @param color text color of the message
         * @returns On success : void
         * @returns On fail : InCharacterSelect or PlayerNotInSpace.
         */
        Action<void, Error> MessageLocal(std::wstring_view message, float range = 10'000.0f, MessageFormat format = MessageFormat::Normal,
                                         MessageColor color = MessageColor::Default) const;

        /**
         * sends a private message to a specified client as if this client sent it.
         * @param destinationClient the client you wish to send the message to.
         * @param message the text you wish to send.
         * @returns On success : void
         * @returns On fail : InCharacterSelect.
         */
        Action<void, Error> MessageFrom(ClientId destinationClient, std::wstring message) const;
};
