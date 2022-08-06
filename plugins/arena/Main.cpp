﻿/**
 * @date August, 2022
 * @author MadHunter (Ported by Raikkonen 2022)
 * @defgroup Arena Arena
 * @brief
 * This plugin is used to beam players to/from an arena system for the purpose of pvp.
 *
 * @paragraph cmds Player Commands
 * All commands are prefixed with '/' unless explicitly specified.
 * - arena (configurable) - This beams the player to the pvp system.
 * - return - This returns the player to their last docked base.
 *
 * @paragraph adminCmds Admin Commands
 * There are no admin commands in this plugin.
 *
 * @paragraph configuration Configuration
 * @code
 * {
 *     "command": "arena",
 *     "restrictedSystem": "Li01",
 *     "targetBase": "Li02_01_Base",
 *     "targetSystem": "Li02"
 * }
 * @endcode
 *
 * @paragraph ipc IPC Interfaces Exposed
 * This plugin does not expose any functionality.
 *
 * @paragraph optional Optional Plugin Dependencies
 * This plugin uses the "Base" plugin.
 */

#include "Main.h"

namespace Plugins::Arena
{
	const auto global = std::make_unique<Global>();

	/// Clear client info when a client connects.
	void ClearClientInfo(const uint& iClientID)
	{
		global->transferFlags[iClientID] = ClientState::None;
	}

	// Client command processing
	void UserCmd_Conn(const uint& iClientID, const std::wstring_view& wscParam);
	void UserCmd_Return(const uint& iClientID, const std::wstring_view& wscParam);
	const std::vector commands = {{
	    CreateUserCommand(L"/arena", L"", UserCmd_Conn, L""),
	    CreateUserCommand(L"/return", L"", UserCmd_Return, L""),
	}};

	/// Load the configuration
	void LoadSettings()
	{
		Config conf = Serializer::JsonToObject<Config>();
		conf.wscCommand = L"/" + stows(conf.command);
		conf.restrictedSystemId = CreateID(conf.restrictedSystem.c_str());
		conf.targetBaseId = CreateID(conf.targetBase.c_str());
		conf.targetSystemId = CreateID(conf.targetSystem.c_str());
		global->config = std::make_unique<Config>(std::move(conf));

		auto& cmd = const_cast<UserCommand&>(commands[0]);
		cmd = CreateUserCommand(global->config->wscCommand, cmd.usage, cmd.proc, cmd.description);

		global->baseCommunicator = static_cast<BaseCommunicator*>(PluginCommunicator::ImportPluginCommunicator(BaseCommunicator::pluginName));
	}

	/** @ingroup Arena
	 * @brief Returns true if the client is docked, returns false otherwise.
	 */
	bool IsDockedClient(unsigned int client)
	{
		unsigned int base = 0;
		pub::Player::GetBase(client, base);
		if (base)
			return true;

		return false;
	}

	/** @ingroup Arena
	 * @brief Returns true if the client doesn't hold any commodities, returns false otherwise. This is to prevent people using the arena system as a trade shortcut.
	 */
	bool ValidateCargo(unsigned int client)
	{
		std::wstring playerName = HkGetCharacterNameById(client);
		std::list<CARGO_INFO> cargo;
		int holdSize = 0;

		HkEnumCargo(playerName, cargo, holdSize);

		for (std::list<CARGO_INFO>::const_iterator it = cargo.begin(); it != cargo.end(); ++it)
		{
			const CARGO_INFO& item = *it;

			bool flag = false;
			pub::IsCommodity(item.iArchID, flag);

			// Some commodity present.
			if (flag)
				return false;
		}

		return true;
	}

	/** @ingroup Arena
	 * @brief Stores the return point for the client in their save file (this should be changed).
	 */
	void StoreReturnPointForClient(unsigned int client)
	{
		// It's not docked at a custom base, check for a regular base
		uint base = 0;

		if (global->baseCommunicator)
			base = global->baseCommunicator->GetCustomBaseId(client);

		if (!base)
			pub::Player::GetBase(client, base);
		if (!base)
			return;

		HkSetCharacterIni(client, L"conn.retbase", std::to_wstring(base));
	}

	/** @ingroup Arena
	 * @brief This returns the return base id that is stored in the client's save file.
	 */
	unsigned int ReadReturnPointForClient(unsigned int client) { return HkGetCharacterIniUint(client, L"conn.retbase"); }

	/** @ingroup Arena
	 * @brief Move the specified client to the specified base.
	 */
	void MoveClient(unsigned int client, unsigned int targetBase)
	{
		// Ask that another plugin handle the beam.
		if (global->baseCommunicator && global->baseCommunicator->CustomBaseBeam(client, targetBase))
			return;

		// No plugin handled it, do it ourselves.
		unsigned int system;
		pub::Player::GetSystem(client, system);
		Universe::IBase* base = Universe::get_base(targetBase);

		pub::Player::ForceLand(client, targetBase); // beam

		// if not in the same system, emulate F1 charload
		if (base->iSystemID != system)
		{
			Server.BaseEnter(targetBase, client);
			Server.BaseExit(targetBase, client);
			std::wstring wscCharFileName;
			HkGetCharFileName(client, wscCharFileName);
			wscCharFileName += L".fl";
			CHARACTER_ID cID;
			strcpy_s(cID.szCharFilename, wstos(wscCharFileName.substr(0, 14)).c_str());
			Server.CharacterSelect(cID, client);
		}
	}

	/** @ingroup Arena
	 * @brief Checks the client is in the specified base. Returns true is so, returns false otherwise.
	 */
	bool CheckReturnDock(unsigned int client, unsigned int target)
	{
		unsigned int base = 0;
		pub::Player::GetBase(client, base);

		if (base == target)
			return true;

		return false;
	}

	/** @ingroup Arena
	 * @brief Hook on CharacterSelect. Sets their transfer flag to "None".
	 */
	void __stdcall CharacterSelect(std::string& szCharFilename, uint& client) { global->transferFlags[client] = ClientState::None; }

	/** @ingroup Arena
	 * @brief Hook on PlayerLaunch. If their transfer flags are set appropriately, redirect the undock to either the arena base or the return point
	 */
	void __stdcall PlayerLaunch_AFTER(uint& ship, uint& client)
	{
		if (global->transferFlags[client] == ClientState::Transfer)
		{
			if (!ValidateCargo(client))
			{
				PrintUserCmdText(client, StrInfo2);
				return;
			}

			global->transferFlags[client] = ClientState::None;
			MoveClient(client, global->config->targetBaseId);
			return;
		}

		if (global->transferFlags[client] == ClientState::Return)
		{
			if (!ValidateCargo(client))
			{
				PrintUserCmdText(client, StrInfo2);
				return;
			}

			global->transferFlags[client] = ClientState::None;
			unsigned int returnPoint = ReadReturnPointForClient(client);

			if (!returnPoint)
				return;

			MoveClient(client, returnPoint);
			HkSetCharacterIni(client, L"conn.retbase", L"0");
			return;
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// USER COMMANDS
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/** @ingroup Arena
	 * @brief Used to switch to the arena system
	 */
	void UserCmd_Conn(const uint& iClientID, const std::wstring_view& wscParam)
	{
		// Prohibit jump if in a restricted system or in the target system
		uint system = 0;
		pub::Player::GetSystem(iClientID, system);
		if (system == global->config->restrictedSystemId || system == global->config->targetSystemId ||
		    (global->baseCommunicator && global->baseCommunicator->GetCustomBaseId(iClientID)))
		{
			PrintUserCmdText(iClientID, L"ERR Cannot use command in this system or base");
			return;
		}

		if (!IsDockedClient(iClientID))
		{
			PrintUserCmdText(iClientID, StrInfo1);
			return;
		}

		if (!ValidateCargo(iClientID))
		{
			PrintUserCmdText(iClientID, StrInfo2);
			return;
		}

		StoreReturnPointForClient(iClientID);
		PrintUserCmdText(iClientID, L"Redirecting undock to Arena.");
		global->transferFlags[iClientID] = ClientState::Transfer;
	}

	/** @ingroup Arena
	 * @brief Used to return from the arena system.
	 */
	void UserCmd_Return(const uint& iClientID, const std::wstring_view& wscParam)
	{
		if (!ReadReturnPointForClient(iClientID))
		{
			PrintUserCmdText(iClientID, L"No return possible");
			return;
		}

		if (!IsDockedClient(iClientID))
		{
			PrintUserCmdText(iClientID, StrInfo1);
			return;
		}

		if (!CheckReturnDock(iClientID, global->config->targetBaseId))
		{
			PrintUserCmdText(iClientID, L"Not in correct base");
			return;
		}

		if (!ValidateCargo(iClientID))
		{
			PrintUserCmdText(iClientID, StrInfo2);
			return;
		}

		PrintUserCmdText(iClientID, L"Redirecting undock to previous base");
		global->transferFlags[iClientID] = ClientState::Return;
	}
}

using namespace Plugins::Arena;

REFL_AUTO(type(Config), field(command), field(targetBase), field(targetSystem), field(restrictedSystem))

DefaultDllMainSettings(LoadSettings)

// Functions to hook
extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	pi->name("Arena");
	pi->shortName("arena");
	pi->mayUnload(true);
	pi->commands(commands);
	pi->returnCode(&global->returnCode);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
	pi->emplaceHook(HookedCall::FLHook__LoadSettings, &LoadSettings, HookStep::After);
	pi->emplaceHook(HookedCall::IServerImpl__CharacterSelect, &CharacterSelect);
	pi->emplaceHook(HookedCall::IServerImpl__PlayerLaunch, &PlayerLaunch_AFTER, HookStep::After);
	pi->emplaceHook(HookedCall::FLHook__ClearClientInfo, &ClearClientInfo);	
}