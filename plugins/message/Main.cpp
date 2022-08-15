/**
 * @date Feb, 2010
 * @author Cannon (Ported by Raikkonen)
 * @defgroup Message Message
 * @brief Handles different types of messages, banners and faction invitations.
 *
 * @paragraph cmds Player Commands
 * All commands are prefixed with '/' unless explicitly specified.
 * - setmsg <n> <msg text> - Sets a preset message.
 * - showmsgs - Show your preset messages.
 * - {0-9} - Send preset message from slot 0-9.
 * - l{0-9} - Send preset message to local chat from slot 0-9.
 * - g{0-9} - Send preset message to group chat from slot 0-9.
 * - t{0-9} - Send preset message to last target from slot 0-9.
 * - target <message> - Send a message to previous/current target.
 * - reply <message> - Send a message to the last person to PM you.
 * - privatemsg <charname> <message> - Send private message to the specified player.
 * - factionmsg <tag> <message> - Send a message to the specified faction tag.
 * - factioninvite <tag> - Send a group invite to online members of the specified tag.
 * - lastpm - Shows the send of the last PM received.
 * - set chattime <on/off> - Enable time stamps on chat.
 * - me - Prints your name plus other text to system. Eg, /me thinks would say Bob thinks in system chat.
 * - do - Prints red text to system chat.
 * - time - Prints the current server time.
 *
 * @paragraph adminCmds Admin Commands
 * There are no admin commands in this plugin.
 *
 * @paragraph configuration Configuration
 * @code
 * {
 *     "CustomHelp": false,
 *     "DisconnectSwearingInSpaceMsg": "%player has been kicked for swearing",
 *     "DisconnectSwearingInSpaceRange": 5000.0,
 *     "EnableDo": true,
 *     "EnableMe": true,
 *     "EnableSetMessage": false,
 *     "GreetingBannerLines": ["<TRA data="0xDA70D608" mask="-1"/><TEXT>Welcome to the server.</TEXT>"],
 *     "SpecialBannerLines": ["<TRA data="0x40B60000" mask="-1"/><TEXT>This is a special banner.</TEXT>"],
 *     "SpecialBannerTimeout": 5,
 *     "StandardBannerLines": ["<TRA data="0x40B60000" mask="-1"/><TEXT>Here is a standard banner.</TEXT>"],
 *     "StandardBannerTimeout": 60,
 *     "SuppressMistypedCommands": true,
 *     "SwearWords": [""]
 * }
 * @endcode
 *
 * @paragraph ipc IPC Interfaces Exposed
 * This plugin does not expose any functionality.
 *
 * @paragraph optional Optional Plugin Dependencies
 * - Mail
 * - Tempban
 */

#include "Main.h"

namespace Plugins::Message
{
	const std::unique_ptr<Global> global = std::make_unique<Global>();

	/** @ingroup Message
	 * @brief Load the msgs for specified client ID into memory.
	 */
	static void LoadMsgs(uint iClientID)
	{
		if (!global->config->EnableSetMessage)
			return;

		// Load from disk the messages.
		for (int iMsgSlot = 0; iMsgSlot < numberOfSlots; iMsgSlot++)
		{
			global->Info[iClientID].slot[iMsgSlot] = HkGetCharacterIniString(iClientID, L"msg." + std::to_wstring(iMsgSlot));
		}

		// Chat time settings.
		global->Info[iClientID].showChatTime = HkGetCharacterIniBool(iClientID, L"msg.chat_time");
	}

	/** @ingroup Message
	 * @brief Show the greeting banner to the specified player.
	 */
	static void ShowGreetingBanner(int iClientID)
	{
		if (!global->Info[iClientID].greetingShown)
		{
			global->Info[iClientID].greetingShown = true;
			for (auto& line : global->config->GreetingBannerLines)
			{
				if (line.find(L"<TRA") == 0)
					HkFMsg(iClientID, line);
				else
					PrintUserCmdText(iClientID, L"%s", line.c_str());
			}
		}
	}

	/** @ingroup Message
	 * @brief Show the special banner to all players.
	 */
	static void ShowSpecialBanner()
	{
		struct PlayerData* pPD = 0;
		while (pPD = Players.traverse_active(pPD))
		{
			uint iClientID = HkGetClientIdFromPD(pPD);
			for (auto& line : global->config->SpecialBannerLines)
			{
				if (line.find(L"<TRA") == 0)
					HkFMsg(iClientID, line);
				else
					PrintUserCmdText(iClientID, L"%s", line.c_str());
			}
		}
	}

	/** @ingroup Message
	 * @brief Show the next standard banner to all players.
	 */
	static void ShowStandardBanner()
	{
		if (global->config->StandardBannerLines.empty())
			return;

		static size_t iCurStandardBanner = 0;
		if (++iCurStandardBanner >= global->config->StandardBannerLines.size())
			iCurStandardBanner = 0;

		struct PlayerData* pPD = nullptr;
		while (pPD = Players.traverse_active(pPD))
		{
			const uint clientId = HkGetClientIdFromPD(pPD);

			if (global->config->StandardBannerLines[iCurStandardBanner].find(L"<TRA") == 0)
				HkFMsg(clientId, global->config->StandardBannerLines[iCurStandardBanner]);
			else
				PrintUserCmdText(clientId, L"%s", global->config->StandardBannerLines[iCurStandardBanner].c_str());
		}
	}

	/** @ingroup Message
	 * @brief Replace #t and #c tags with current target name and current ship location. Return false if tags cannot be replaced.
	 */
	static bool ReplaceMessageTags(uint iClientID, ClientInfo& clientData, std::wstring& wscMsg)
	{
		if (wscMsg.find(L"#t") != -1)
		{
			if (clientData.targetClientID == -1)
			{
				PrintUserCmdText(iClientID, L"ERR Target not available");
				return false;
			}

			std::wstring wscTargetName = (const wchar_t*)Players.GetActiveCharacterName(clientData.targetClientID);
			wscMsg = ReplaceStr(wscMsg, L"#t", wscTargetName);
		}

		if (wscMsg.find(L"#c") != -1)
		{
			std::wstring wscCurrLocation = GetLocation(iClientID);
			wscMsg = ReplaceStr(wscMsg, L"#c", wscCurrLocation);
		}

		return true;
	}

	/** @ingroup Message
	 * @brief Returns a string with the preset message
	 */
	std::wstring GetPresetMessage(uint iClientID, int iMsgSlot)
	{
		auto iter = global->Info.find(iClientID);
		if (iter == global->Info.end() || iter->second.slot[iMsgSlot].empty())
		{
			PrintUserCmdText(iClientID, L"ERR No message defined");
			return L"";
		}

		// Replace the tag #t with name of the targeted player.
		std::wstring wscMsg = iter->second.slot[iMsgSlot];
		if (!ReplaceMessageTags(iClientID, iter->second, wscMsg))
			return L"";

		return wscMsg;
	}

	/** @ingroup Message
	 * @brief Send an preset message to the local system chat
	 */
	void SendPresetLocalMessage(uint iClientID, int iMsgSlot)
	{
		if (!global->config->EnableSetMessage)
			return;

		if (iMsgSlot < 0 || iMsgSlot > 9)
		{
			PrintUserCmdText(iClientID, L"ERR Invalid parameters");
			PrintUserCmdText(iClientID, L"Usage: /ln (n=0-9)");
			return;
		}

		SendLocalSystemChat(iClientID, GetPresetMessage(iClientID, iMsgSlot));
	}

	/** @ingroup Message
	 * @brief Send a preset message to the last/current target.
	 */
	void SendPresetToLastTarget(uint iClientID, int iMsgSlot)
	{
		if (!global->config->EnableSetMessage)
			return;

		UserCmd_SendToLastTarget(iClientID, GetPresetMessage(iClientID, iMsgSlot));
	}

	/** @ingroup Message
	 * @brief Send an preset message to the system chat 
	 */
	void SendPresetSystemMessage(uint iClientID, int iMsgSlot)
	{
		if (!global->config->EnableSetMessage)
			return;

		SendSystemChat(iClientID, GetPresetMessage(iClientID, iMsgSlot));
	}

	/** @ingroup Message
	 * @brief Send an preset message to the last PM sender
	 */
	void SendPresetLastPMSender(const uint& iClientID, int iMsgSlot, const std::wstring_view& wscMsg)
	{
		if (!global->config->EnableSetMessage)
			return;

		UserCmd_ReplyToLastPMSender(iClientID, GetPresetMessage(iClientID, iMsgSlot));
	}

	/** @ingroup Message
	 * @brief Send an preset message to the group chat
	 */
	void SendPresetGroupMessage(const uint& iClientID, int iMsgSlot)
	{
		if (!global->config->EnableSetMessage)
			return;

		SendGroupChat(iClientID, GetPresetMessage(iClientID, iMsgSlot));
	}

	/** @ingroup Message
	 * @brief Clean up when a client disconnects
	 */
	void ClearClientInfo(uint& iClientID) { global->Info.erase(iClientID); }

	/** @ingroup Message
	 * @brief This function is called when the admin command rehash is called and when the module is loaded.
	 */
	void LoadSettings()
	{
		// For every active player load their msg settings.
		const std::list<HKPLAYERINFO> players = HkGetPlayers();
		for (auto& p : players)
			LoadMsgs(p.iClientID);

		auto config = Serializer::JsonToObject<Config>();
		global->config = std::make_unique<Config>(config);

		// Load our communicators
		global->mailCommunicator = static_cast<Mail::MailCommunicator*>(PluginCommunicator::ImportPluginCommunicator(Mail::MailCommunicator::pluginName));
		global->tempBanCommunicator =
		    static_cast<Tempban::TempBanCommunicator*>(PluginCommunicator::ImportPluginCommunicator(Tempban::TempBanCommunicator::pluginName));
	}

	/** @ingroup Message
	 * @brief On this timer display banners
	 */
	void OneSecondTimer()
	{
		if (static int iSpecialBannerTimer = 0; ++iSpecialBannerTimer > global->config->SpecialBannerTimeout)
		{
			iSpecialBannerTimer = 0;
			ShowSpecialBanner();
		}

		if (static int iStandardBannerTimer = 0; ++iStandardBannerTimer > global->config->StandardBannerTimeout)
		{
			iStandardBannerTimer = 0;
			ShowStandardBanner();
		}
	}

	/** @ingroup Message
	 * @brief On client disconnect remove any references to this client.
	 */
	void DisConnect(uint& iClientID, enum EFLConnection& p2)
	{
		auto iter = global->Info.begin();
		while (iter != global->Info.end())
		{
			if (iter->second.lastPmClientID == iClientID)
				iter->second.lastPmClientID = -1;
			if (iter->second.targetClientID == iClientID)
				iter->second.targetClientID = -1;
			++iter;
		}
	}

	/** @ingroup Message
	 * @brief On client F1 or entry to char select menu.
	 */
	void CharacterInfoReq(unsigned int iClientID, bool p2)
	{
		auto iter = global->Info.begin();
		while (iter != global->Info.end())
		{
			if (iter->second.lastPmClientID == iClientID)
				iter->second.lastPmClientID = -1;
			if (iter->second.targetClientID == iClientID)
				iter->second.targetClientID = -1;
			++iter;
		}
	}

	/** @ingroup Message
	 * @brief On launch events and reload the msg cache for the client.
	 */
	void PlayerLaunch(uint& iShip, uint& iClientID)
	{
		LoadMsgs(iClientID);
		ShowGreetingBanner(iClientID);
	}

	/** @ingroup Message
	 * @brief On base entry events and reload the msg cache for the client.
	 */
	void BaseEnter(uint iBaseID, uint iClientID)
	{
		LoadMsgs(iClientID);
		ShowGreetingBanner(iClientID);
	}

	/** @ingroup Message
	 * @brief When a char selects a target and the target is a player ship then record the target's clientID.
	 */
	void SetTarget(uint& uClientID, struct XSetTarget const& p2)
	{
		// The iSpaceID *appears* to represent a player ship ID when it is
		// targeted but this might not be the case. Also note that
		// HkGetClientIDByShip returns 0 on failure not -1.
		uint uTargetClientID = HkGetClientIDByShip(p2.iSpaceID);
		if (uTargetClientID)
		{
			auto iter = global->Info.find(uClientID);
			if (iter != global->Info.end())
			{
				iter->second.targetClientID = uTargetClientID;
			}
		}
	}

	/** @ingroup Message
	 * @brief Hook on SubmitChat. Suppresses swearing. Records the last user to PM.
	 */
	bool SubmitChat(uint& cId, unsigned long& iSize, const void** rdlReader, uint& cIdTo, int& p2)
	{
		// Ignore group join/leave commands
		if (cIdTo == 0x10004)
			return false;

		// Extract text from rdlReader
		BinaryRDLReader rdl;
		wchar_t wszBuf[1024];
		uint iRet1;
		rdl.extract_text_from_buffer((unsigned short*)wszBuf, sizeof(wszBuf), iRet1, (const char*)*rdlReader, iSize);

		std::wstring wscChatMsg = ToLower(wszBuf);
		uint iClientID = cId;

		bool bIsGroup = (cIdTo == 0x10003 || !wscChatMsg.find(L"/g ") || !wscChatMsg.find(L"/group "));
		if (!bIsGroup)
		{
			// If a restricted word appears in the message take appropriate action.
			for (auto& word : global->config->SwearWords)
			{
				if (wscChatMsg.find(word) != -1)
				{
					PrintUserCmdText(iClientID, L"This is an automated message.");
					PrintUserCmdText(iClientID, L"Please do not swear or you may be sanctioned.");

					global->Info[iClientID].swearWordWarnings++;
					if (global->Info[iClientID].swearWordWarnings > 2)
					{
						std::wstring wscCharname = reinterpret_cast<const wchar_t*>(Players.GetActiveCharacterName(iClientID));
						AddLog(LogType::Kick,
						    LogLevel::Info,
						    L"Swearing tempban on %s (%s) reason='%s'",
						    wscCharname.c_str(),
						    HkGetAccountID(HkGetAccountByCharname(wscCharname)).c_str(),
						    (wscChatMsg).c_str());

						if (global->tempBanCommunicator)
							global->tempBanCommunicator->TempBan(wscCharname, 10);

						HkDelayedKick(iClientID, 1);

						if (global->config->DisconnectSwearingInSpaceRange > 0.0f)
						{
							std::wstring wscMsg = global->config->DisconnectSwearingInSpaceMsg;
							wscMsg = ReplaceStr(wscMsg, L"%time", GetTimeString(FLHookConfig::i()->general.dieMsg));
							wscMsg = ReplaceStr(wscMsg, L"%player", wscCharname);
							PrintLocalUserCmdText(iClientID, wscMsg, global->config->DisconnectSwearingInSpaceRange);
						}
					}
					return true;
				}
			}
		}

		/// When a private chat message is sent from one client to another record
		/// who sent the message so that the receiver can reply using the /r command
		/// */
		if (iClientID < 0x10000 && cIdTo > 0 && cIdTo < 0x10000)
		{
			auto iter = global->Info.find(cIdTo);
			if (iter != global->Info.end())
			{
				iter->second.lastPmClientID = iClientID;
			}
		}
		return false;
	}

	/** @ingroup Message
	 * @brief Prints RedText in the style of New Player messages.
	 */
	void RedText(std::wstring wscXMLMsg, uint iSystemID)
	{
		char szBuf[0x1000];
		uint iRet;
		if (!HKHKSUCCESS(HkFMsgEncodeXML(wscXMLMsg, szBuf, sizeof(szBuf), iRet)))
			return;

		// Send to all players in system
		struct PlayerData* pPD = 0;
		while (pPD = Players.traverse_active(pPD))
		{
			uint iClientID = HkGetClientIdFromPD(pPD);
			uint iClientSystemID = 0;
			pub::Player::GetSystem(iClientID, iClientSystemID);

			if (iSystemID == iClientSystemID)
				HkFMsgSendChat(iClientID, szBuf, iRet);
		}
	}

	/** @ingroup Message
	 * @brief When a chat message is sent to a client and this client has showchattime on insert the time on the line immediately before the chat message
	 */
	bool SendChat(uint& iClientID, uint& iTo, uint& iSize, void** rdlReader)
	{
		// Return immediately if the chat line is the time.
		if (global->SendingTime)
			return false;

		// Ignore group messages (I don't know if they ever get here
		if (iTo == 0x10004)
			return false;

		if (global->config->SuppressMistypedCommands)
		{
			// Extract text from rdlReader
			BinaryRDLReader rdl;
			wchar_t wszBuf[1024];
			uint iRet1;
			void* rdlReader2 = *rdlReader;
			rdl.extract_text_from_buffer((unsigned short*)wszBuf, sizeof(wszBuf), iRet1, (const char*)rdlReader2, iSize);
			std::wstring wscChatMsg = wszBuf;

			// Find the ': ' which indicates the end of the sending player name.
			size_t iTextStartPos = wscChatMsg.find(L": ");
			if (iTextStartPos != std::string::npos)
			{
				if ((wscChatMsg.find(L": /") == iTextStartPos && wscChatMsg.find(L": //") != iTextStartPos) || wscChatMsg.find(L": .") == iTextStartPos)
				{
					return true;
				}
			}
		}

		if (global->Info[iClientID].showChatTime)
		{
			// Send time with gray color (BEBEBE) in small text (90) above the chat
			// line.
			global->SendingTime = true;
			HkFMsg(iClientID, L"<TRA data=\"0xBEBEBE90\" mask=\"-1\"/><TEXT>" + XMLText(GetTimeString(FLHookConfig::i()->general.dieMsg)) + L"</TEXT>");
			global->SendingTime = false;
		}
		return false;
	}

	/** @ingroup Message
	 * @brief Set a preset message
	 */
	void UserCmd_SetMsg(const uint& iClientID, const std::wstring_view& wscParam)
	{
		if (!global->config->EnableSetMessage)
			return;

		int iMsgSlot = ToInt(GetParam(wscParam, ' ', 0));
		std::wstring_view wscMsg = GetParamToEnd(ViewToWString(wscParam), ' ', 1);

		if (iMsgSlot < 0 || iMsgSlot > 9 || wscParam.size() == 0)
		{
			PrintUserCmdText(iClientID, L"ERR Invalid parameters");
			PrintUserCmdText(iClientID, L"Usage: /setmsg <n> <msg text>");
			return;
		}

		HkSetCharacterIni(iClientID, L"msg." + std::to_wstring(iMsgSlot), ViewToWString(wscMsg));

		// Reload the character cache
		LoadMsgs(iClientID);
		PrintUserCmdText(iClientID, L"OK");
	}

	/** @ingroup Message
	 * @brief Show preset messages
	 */
	void UserCmd_ShowMsgs(const uint& iClientID, const std::wstring_view& wscParam)
	{
		if (!global->config->EnableSetMessage)
			return;

		auto iter = global->Info.find(iClientID);
		if (iter == global->Info.end())
		{
			PrintUserCmdText(iClientID, L"ERR No messages");
			return;
		}

		for (int i = 0; i < numberOfSlots; i++)
		{
			PrintUserCmdText(iClientID, L"%d: %s", i, iter->second.slot[i].c_str());
		}
		PrintUserCmdText(iClientID, L"OK");
	}

	/** @ingroup Message
	 * @brief User Commands for /r0-9
	 */
	void UserCmd_RMsg0(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLastPMSender(iClientID, 0, wscParam); }

	void UserCmd_RMsg1(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLastPMSender(iClientID, 1, wscParam); }

	void UserCmd_RMsg2(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLastPMSender(iClientID, 2, wscParam); }

	void UserCmd_RMsg3(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLastPMSender(iClientID, 3, wscParam); }

	void UserCmd_RMsg4(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLastPMSender(iClientID, 4, wscParam); }

	void UserCmd_RMsg5(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLastPMSender(iClientID, 5, wscParam); }

	void UserCmd_RMsg6(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLastPMSender(iClientID, 6, wscParam); }

	void UserCmd_RMsg7(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLastPMSender(iClientID, 7, wscParam); }

	void UserCmd_RMsg8(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLastPMSender(iClientID, 8, wscParam); }

	void UserCmd_RMsg9(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLastPMSender(iClientID, 9, wscParam); }

	/** @ingroup Message
	 * @brief User Commands for /s0-9
	 */
	void UserCmd_SMsg0(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetSystemMessage(iClientID, 0); }

	void UserCmd_SMsg1(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetSystemMessage(iClientID, 1); }

	void UserCmd_SMsg2(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetSystemMessage(iClientID, 2); }

	void UserCmd_SMsg3(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetSystemMessage(iClientID, 3); }

	void UserCmd_SMsg4(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetSystemMessage(iClientID, 4); }

	void UserCmd_SMsg5(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetSystemMessage(iClientID, 5); }

	void UserCmd_SMsg6(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetSystemMessage(iClientID, 6); }

	void UserCmd_SMsg7(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetSystemMessage(iClientID, 7); }

	void UserCmd_SMsg8(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetSystemMessage(iClientID, 8); }

	void UserCmd_SMsg9(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetSystemMessage(iClientID, 0); }

	/** @ingroup Message
	 * @brief User Commands for /l0-9
	 */
	void UserCmd_LMsg0(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLocalMessage(iClientID, 0); }

	void UserCmd_LMsg1(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLocalMessage(iClientID, 1); }

	void UserCmd_LMsg2(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLocalMessage(iClientID, 2); }

	void UserCmd_LMsg3(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLocalMessage(iClientID, 3); }

	void UserCmd_LMsg4(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLocalMessage(iClientID, 4); }

	void UserCmd_LMsg5(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLocalMessage(iClientID, 5); }

	void UserCmd_LMsg6(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLocalMessage(iClientID, 6); }

	void UserCmd_LMsg7(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLocalMessage(iClientID, 7); }

	void UserCmd_LMsg8(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLocalMessage(iClientID, 8); }

	void UserCmd_LMsg9(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetLocalMessage(iClientID, 9); }

	/** @ingroup Message
	 * @brief User Commands for /g0-9
	 */
	void UserCmd_GMsg0(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetGroupMessage(iClientID, 0); }

	void UserCmd_GMsg1(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetGroupMessage(iClientID, 1); }

	void UserCmd_GMsg2(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetGroupMessage(iClientID, 2); }

	void UserCmd_GMsg3(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetGroupMessage(iClientID, 3); }

	void UserCmd_GMsg4(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetGroupMessage(iClientID, 4); }

	void UserCmd_GMsg5(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetGroupMessage(iClientID, 5); }

	void UserCmd_GMsg6(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetGroupMessage(iClientID, 6); }

	void UserCmd_GMsg7(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetGroupMessage(iClientID, 7); }

	void UserCmd_GMsg8(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetGroupMessage(iClientID, 8); }

	void UserCmd_GMsg9(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetGroupMessage(iClientID, 9); }

	/** @ingroup Message
	 * @brief User Commands for /t0-9
	 */
	void UserCmd_TMsg0(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetToLastTarget(iClientID, 0); }

	void UserCmd_TMsg1(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetToLastTarget(iClientID, 1); }

	void UserCmd_TMsg2(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetToLastTarget(iClientID, 2); }

	void UserCmd_TMsg3(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetToLastTarget(iClientID, 3); }

	void UserCmd_TMsg4(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetToLastTarget(iClientID, 4); }

	void UserCmd_TMsg5(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetToLastTarget(iClientID, 5); }

	void UserCmd_TMsg6(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetToLastTarget(iClientID, 6); }

	void UserCmd_TMsg7(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetToLastTarget(iClientID, 7); }

	void UserCmd_TMsg8(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetToLastTarget(iClientID, 8); }

	void UserCmd_TMsg9(const uint& iClientID, const std::wstring_view& wscParam) { SendPresetToLastTarget(iClientID, 9); }

	/** @ingroup Message
	 * @brief Send an message to the last person that PM'd this client.
	 */
	void UserCmd_ReplyToLastPMSender(const uint& iClientID, const std::wstring_view& wscParam)
	{
		auto iter = global->Info.find(iClientID);
		if (iter == global->Info.end())
		{
			// There's no way for this to happen! yeah right.
			PrintUserCmdText(iClientID, L"ERR No message defined");
			return;
		}

		std::wstring_view wscMsg = GetParamToEnd(wscParam, ' ', 0);

		if (iter->second.lastPmClientID == -1)
		{
			PrintUserCmdText(iClientID, L"ERR PM sender not available");
			return;
		}

		global->Info[iter->second.lastPmClientID].lastPmClientID = iClientID;
		SendPrivateChat(iClientID, iter->second.lastPmClientID, ViewToWString(wscMsg));
	}

	/** @ingroup Message
	 * @brief Shows the sender of the last PM and the last char targeted
	 */
	void UserCmd_ShowLastPMSender(const uint& iClientID, const std::wstring_view& wscParam)
	{
		auto iter = global->Info.find(iClientID);
		if (iter == global->Info.end())
		{
			// There's no way for this to happen! yeah right.
			PrintUserCmdText(iClientID, L"ERR No message defined");
			return;
		}

		std::wstring wscSenderCharname = L"<not available>" + std::to_wstring(iter->second.lastPmClientID);
		if (iter->second.lastPmClientID != -1 && HkIsValidClientID(iter->second.lastPmClientID))
			wscSenderCharname = (const wchar_t*)Players.GetActiveCharacterName(iter->second.lastPmClientID);

		std::wstring wscTargetCharname = L"<not available>" + std::to_wstring(iter->second.targetClientID);
		if (iter->second.targetClientID != -1 && HkIsValidClientID(iter->second.targetClientID))
			wscTargetCharname = (const wchar_t*)Players.GetActiveCharacterName(iter->second.targetClientID);

		PrintUserCmdText(iClientID, L"OK sender=" + wscSenderCharname + L" target=" + wscTargetCharname);
	}

	/** @ingroup Message
	 * @brief Send a message to the last/current target. 
	 */
	void UserCmd_SendToLastTarget(const uint& iClientID, const std::wstring_view& wscParam)
	{
		auto iter = global->Info.find(iClientID);
		if (iter == global->Info.end())
		{
			// There's no way for this to happen! yeah right.
			PrintUserCmdText(iClientID, L"ERR No message defined");
			return;
		}

		std::wstring_view wscMsg = GetParamToEnd(wscParam, ' ', 0);

		if (iter->second.targetClientID == -1)
		{
			PrintUserCmdText(iClientID, L"ERR PM target not available");
			return;
		}

		global->Info[iter->second.targetClientID].lastPmClientID = iClientID;
		SendPrivateChat(iClientID, iter->second.targetClientID, ViewToWString(wscMsg));
	}

	/** @ingroup Message
	 * @brief Send a private message to the specified charname. If the player is offline the message will be delivery when they next login.
	 */
	void UserCmd_PrivateMsg(const uint& iClientID, const std::wstring_view& wscParam)
	{
		std::wstring usage = L"Usage: /privatemsg <charname> <messsage> or /pm ...";
		std::wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
		const std::wstring& wscTargetCharname = GetParam(wscParam, ' ', 0);
		const std::wstring_view wscMsg = GetParamToEnd(wscParam, ' ', 1);

		if (wscCharname.size() == 0 || wscMsg.size() == 0)
		{
			PrintUserCmdText(iClientID, L"ERR Invalid parameters");
			PrintUserCmdText(iClientID, usage);
			return;
		}

		if (!HkGetAccountByCharname(wscTargetCharname))
		{
			PrintUserCmdText(iClientID, L"ERR charname does not exist");
			return;
		}

		uint iToClientID = HkGetClientIdFromCharname(wscTargetCharname);
		if (iToClientID == -1)
		{
			if (global->mailCommunicator)
			{
				global->mailCommunicator->SendMail(wscTargetCharname, wscMsg);
				PrintUserCmdText(iClientID, L"OK message saved to mailbox");
			}
			else
			{
				PrintUserCmdText(iClientID, L"ERR: Player offline");
			}
		}
		else
		{
			global->Info[iToClientID].lastPmClientID = iClientID;
			SendPrivateChat(iClientID, iToClientID, ViewToWString(wscMsg));
		}
	}

	/** @ingroup Message
	 * @brief Send a private message to the specified clientid.
	 */
	void UserCmd_PrivateMsgID(const uint& iClientID, const std::wstring_view& wscParam)
	{
		std::wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
		const std::wstring& wscClientID = GetParam(wscParam, ' ', 0);
		const std::wstring_view wscMsg = GetParamToEnd(wscParam, ' ', 1);

		uint iToClientID = ToInt(wscClientID);
		if (!HkIsValidClientID(iToClientID) || HkIsInCharSelectMenu(iToClientID))
		{
			PrintUserCmdText(iClientID, L"ERR Invalid client-id");
			return;
		}

		global->Info[iToClientID].lastPmClientID = iClientID;
		SendPrivateChat(iClientID, iToClientID, ViewToWString(wscMsg));
	}

	/** @ingroup Message
	 * @brief Send a message to all players with a particular prefix.
	 */
	void UserCmd_FactionMsg(const uint& iClientID, const std::wstring_view& wscParam)
	{
		std::wstring wscSender = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
		const std::wstring& wscCharnamePrefix = GetParam(wscParam, ' ', 0);
		const std::wstring_view& wscMsg = GetParamToEnd(wscParam, ' ', 1);

		if (wscCharnamePrefix.size() < 3 || wscMsg.size() == 0)
		{
			PrintUserCmdText(iClientID, L"ERR Invalid parameters");
			PrintUserCmdText(iClientID, L"Usage: /factionmsg <tag> <message> or /fm ...");
			return;
		}

		bool bSenderReceived = false;
		bool bMsgSent = false;
		for (auto& player : HkGetPlayers())
		{
			if (ToLower(player.wscCharname).find(ToLower(wscCharnamePrefix)) == std::string::npos)
				continue;

			if (player.iClientID == iClientID)
				bSenderReceived = true;

			FormatSendChat(player.iClientID, wscSender, ViewToWString(wscMsg), L"FF7BFF");
			bMsgSent = true;
		}
		if (!bSenderReceived)
			FormatSendChat(iClientID, wscSender, ViewToWString(wscMsg), L"FF7BFF");

		if (bMsgSent == false)
			PrintUserCmdText(iClientID, L"ERR No chars found");
	}

	/** @ingroup Message
	 * @brief Send a faction invite message to all players with a particular prefix.
	 */
	void UserCmd_FactionInvite(const uint& iClientID, const std::wstring_view& wscParam)
	{
		const std::wstring& wscCharnamePrefix = GetParam(wscParam, ' ', 0);

		bool msgSent = false;

		if (wscCharnamePrefix.size() < 3)
		{
			PrintUserCmdText(iClientID, L"ERR Invalid parameters");
			PrintUserCmdText(iClientID, L"Usage: /factioninvite <tag> or /fi ...");
			return;
		}

		for (auto& player : HkGetPlayers())
		{
			if (ToLower(player.wscCharname).find(ToLower(wscCharnamePrefix)) == std::string::npos)
				continue;
			if (player.iClientID == iClientID)
				continue;

			std::wstring wscMsg = L"/i " + player.wscCharname;

			uint iRet;
			char szBuf[1024];
			if (HkError err; (err = HkFMsgEncodeXML(wscMsg, szBuf, sizeof(szBuf), iRet)) != HKE_OK)
			{
				PrintUserCmdText(iClientID, L"ERR " + HkErrGetText(err));
				return;
			}

			struct CHAT_ID cId = {iClientID};
			struct CHAT_ID cIdTo = {0x10001};

			Server.SubmitChat(cId, iRet, szBuf, cIdTo, -1);
			msgSent = true;
		}

		if (msgSent == false)
			PrintUserCmdText(iClientID, L"ERR No chars found");
	}

	/** @ingroup Message
	 * @brief User command for enabling the chat timestamps.
	 */
	void UserCmd_SetChatTime(const uint& iClientID, const std::wstring_view& wscParam)
	{
		std::wstring wscParam1 = ToLower(GetParam(wscParam, ' ', 0));
		bool bShowChatTime = false;
		if (!wscParam1.compare(L"on"))
			bShowChatTime = true;
		else if (!wscParam1.compare(L"off"))
			bShowChatTime = false;
		else
		{
			PrintUserCmdText(iClientID, L"ERR Invalid parameters");
			PrintUserCmdText(iClientID, L"Usage: /set chattime [on|off]");
		}

		std::wstring wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);

		HkSetCharacterIni(iClientID, L"msg.chat_time", bShowChatTime ? L"true" : L"false");

		// Update the client cache.
		auto iter = global->Info.find(iClientID);
		if (iter != global->Info.end())
			iter->second.showChatTime = bShowChatTime;

		// Send confirmation msg
		PrintUserCmdText(iClientID, L"OK");
	}

	/** @ingroup Message
	 * @brief Prints the current server time.
	 */
	void UserCmd_Time(const uint& iClientID, const std::wstring_view& wscParam)
	{
		// Send time with gray color (BEBEBE) in small text (90) above the chat line.
		PrintUserCmdText(iClientID, GetTimeString(FLHookConfig::i()->general.localTime));
	}

	/** @ingroup Message
	 * @brief Me command allow players to type "/me powers his engines" which would print: "Trent powers his engines"
	 */
	void UserCmd_Me(const uint& iClientID, const std::wstring_view& wscParam)
	{
		if (global->config->EnableMe)
		{
			std::wstring charname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);
			uint iSystemID;
			pub::Player::GetSystem(iClientID, iSystemID);

			// Encode message using the death message style (red text).
			std::wstring wscXMLMsg = L"<TRA data=\"" + FLHookConfig::i()->msgStyle.deathMsgStyle + L"\" mask=\"-1\"/> <TEXT>";
			wscXMLMsg += charname + L" ";
			wscXMLMsg += XMLText(ViewToWString(GetParamToEnd(wscParam, ' ', 0)));
			wscXMLMsg += L"</TEXT>";

			RedText(wscXMLMsg, iSystemID);
		}
		else
		{
			PrintUserCmdText(iClientID, L"Command not enabled.");
		}
	}

	/** @ingroup Message
	 * @brief Do command allow players to type "/do Nomad fighters detected" which would print: "Nomad fighters detected" in the standard red text 
	 */
	void UserCmd_Do(const uint& iClientID, const std::wstring_view& wscParam)
	{
		if (global->config->EnableDo)
		{
			uint iSystemID;
			pub::Player::GetSystem(iClientID, iSystemID);

			// Encode message using the death message style (red text).
			std::wstring wscXMLMsg = L"<TRA data=\"" + FLHookConfig::i()->msgStyle.deathMsgStyle + L"\" mask=\"-1\"/> <TEXT>";
			wscXMLMsg += XMLText(ViewToWString(GetParamToEnd(ViewToWString(wscParam), ' ', 0)));
			wscXMLMsg += L"</TEXT>";

			RedText(wscXMLMsg, iSystemID);
		}
		else
		{
			PrintUserCmdText(iClientID, L"Command not enabled.");
		}
	}

	// Client command processing
	const std::vector commands = {{
	    CreateUserCommand(L"/setmsg", L"<n> <msg text>", UserCmd_SetMsg, L"Sets a preset message."),
	    CreateUserCommand(L"/showmsgs", L"", UserCmd_ShowMsgs, L"Show your preset messages."),
	    CreateUserCommand(L"/0", L"", UserCmd_SMsg0, L"Send preset message from slot 0."),
	    CreateUserCommand(L"/1", L"", UserCmd_SMsg1, L""),
	    CreateUserCommand(L"/2", L"", UserCmd_SMsg2, L""),
	    CreateUserCommand(L"/3", L"", UserCmd_SMsg3, L""),
	    CreateUserCommand(L"/4", L"", UserCmd_SMsg4, L""),
	    CreateUserCommand(L"/5", L"", UserCmd_SMsg5, L""),
	    CreateUserCommand(L"/6", L"", UserCmd_SMsg6, L""),
	    CreateUserCommand(L"/7", L"", UserCmd_SMsg7, L""),
	    CreateUserCommand(L"/8", L"", UserCmd_SMsg8, L""),
	    CreateUserCommand(L"/9", L"", UserCmd_SMsg9, L""),
	    CreateUserCommand(L"/l0", L"", UserCmd_LMsg0, L"Send preset message to local chat from slot 0."),
	    CreateUserCommand(L"/l1", L"", UserCmd_LMsg1, L""),
	    CreateUserCommand(L"/l2", L"", UserCmd_LMsg2, L""),
	    CreateUserCommand(L"/l3", L"", UserCmd_LMsg3, L""),
	    CreateUserCommand(L"/l4", L"", UserCmd_LMsg4, L""),
	    CreateUserCommand(L"/l5", L"", UserCmd_LMsg5, L""),
	    CreateUserCommand(L"/l6", L"", UserCmd_LMsg6, L""),
	    CreateUserCommand(L"/l7", L"", UserCmd_LMsg7, L""),
	    CreateUserCommand(L"/l8", L"", UserCmd_LMsg8, L""),
	    CreateUserCommand(L"/l9", L"", UserCmd_LMsg9, L""),
	    CreateUserCommand(L"/g0", L"", UserCmd_GMsg0, L"Send preset message to group chat from slot 0."),
	    CreateUserCommand(L"/g1", L"", UserCmd_GMsg1, L""),
	    CreateUserCommand(L"/g2", L"", UserCmd_GMsg2, L""),
	    CreateUserCommand(L"/g3", L"", UserCmd_GMsg3, L""),
	    CreateUserCommand(L"/g4", L"", UserCmd_GMsg4, L""),
	    CreateUserCommand(L"/g5", L"", UserCmd_GMsg5, L""),
	    CreateUserCommand(L"/g6", L"", UserCmd_GMsg6, L""),
	    CreateUserCommand(L"/g7", L"", UserCmd_GMsg7, L""),
	    CreateUserCommand(L"/g8", L"", UserCmd_GMsg8, L""),
	    CreateUserCommand(L"/g9", L"", UserCmd_GMsg9, L""),
	    CreateUserCommand(L"/t0", L"", UserCmd_TMsg0, L"Send preset message to last target from slot 0."),
	    CreateUserCommand(L"/t1", L"", UserCmd_TMsg1, L""),
	    CreateUserCommand(L"/t2", L"", UserCmd_TMsg2, L""),
	    CreateUserCommand(L"/t3", L"", UserCmd_TMsg3, L""),
	    CreateUserCommand(L"/t4", L"", UserCmd_TMsg4, L""),
	    CreateUserCommand(L"/t5", L"", UserCmd_TMsg5, L""),
	    CreateUserCommand(L"/t6", L"", UserCmd_TMsg6, L""),
	    CreateUserCommand(L"/t7", L"", UserCmd_TMsg7, L""),
	    CreateUserCommand(L"/t8", L"", UserCmd_TMsg8, L""),
	    CreateUserCommand(L"/t9", L"", UserCmd_TMsg9, L""),
	    CreateUserCommand(L"/target", L"<message>", UserCmd_SendToLastTarget, L"Send a message to previous/current target."),
	    CreateUserCommand(L"/t", L"<message>", UserCmd_SendToLastTarget, L"Shortcut for /target."),
	    CreateUserCommand(L"/reply", L"<message>", UserCmd_ReplyToLastPMSender, L"Send a message to the last person to PM you."),
	    CreateUserCommand(L"/r", L"<message>", UserCmd_ReplyToLastPMSender, L"Shortcut for /reply."),
	    CreateUserCommand(L"/privatemsg$", L"<clientid> <message>", UserCmd_PrivateMsgID, L"Send private message to the specified client id."),
	    CreateUserCommand(L"/pm$", L"<clientid> <message>", UserCmd_PrivateMsgID, L"Shortcut for /privatemsg$."),
	    CreateUserCommand(L"/privatemsg", L"<charname> <message>", UserCmd_PrivateMsg, L"Send private message to the specified character name."),
	    CreateUserCommand(L"/pm", L"<charname> <message>", UserCmd_PrivateMsg, L"Shortcut for /privatemsg."),
	    CreateUserCommand(L"/factionmsg", L"<tag> <message>", UserCmd_FactionMsg, L"Send a message to the specified faction tag."),
	    CreateUserCommand(L"/fm", L"<tag> <message>", UserCmd_FactionMsg, L"Shortcut for /factionmsg."),
	    CreateUserCommand(L"/factioninvite", L"<name>", UserCmd_FactionInvite, L"Send a group invite to online members of the specified tag."),
	    CreateUserCommand(L"/fi", L"<name>", UserCmd_FactionInvite, L"Shortcut for /factioninvite."),
	    CreateUserCommand(L"/lastpm", L"", UserCmd_ShowLastPMSender, L"Shows the send of the last PM received."),
	    CreateUserCommand(L"/set chattime", L"<on|off>", UserCmd_SetChatTime, L"Enable time stamps on chat."),
	    CreateUserCommand(L"/me", L"<message>", UserCmd_Me, L"Prints your name plus other text to system. Eg, /me thinks would say Bob thinks in system chat."),
	    CreateUserCommand(L"/do", L"<message>", UserCmd_Do, L"Prints red text to system chat."),
	    CreateUserCommand(L"/time", L"", UserCmd_Time, L"Prints the current server time."),
	}};

} // namespace Plugins::Message

using namespace Plugins::Message;
REFL_AUTO(type(Config), field(GreetingBannerLines), field(SpecialBannerLines), field(StandardBannerLines), field(SpecialBannerTimeout),
    field(StandardBannerTimeout), field(CustomHelp), field(SuppressMistypedCommands), field(EnableSetMessage), field(EnableMe), field(EnableDo),
    field(DisconnectSwearingInSpaceMsg), field(DisconnectSwearingInSpaceRange), field(SwearWords))

DefaultDllMainSettings(LoadSettings)

    // Functions to hook
    extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	pi->name("Message");
	pi->shortName("message");
	pi->mayUnload(true);
	pi->commands(commands);
	pi->returnCode(&global->returncode);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
	pi->emplaceHook(HookedCall::IServerImpl__SubmitChat, &SubmitChat);
	pi->emplaceHook(HookedCall::IChat__SendChat, &SendChat);
	pi->emplaceHook(HookedCall::FLHook__LoadSettings, &LoadSettings, HookStep::After);
	pi->emplaceHook(HookedCall::FLHook__TimerNPCAndF1Check, &OneSecondTimer);
	pi->emplaceHook(HookedCall::IServerImpl__SetTarget, &SetTarget);
	pi->emplaceHook(HookedCall::IServerImpl__DisConnect, &DisConnect);
	pi->emplaceHook(HookedCall::FLHook__ClearClientInfo, &ClearClientInfo);
	pi->emplaceHook(HookedCall::IServerImpl__PlayerLaunch, &PlayerLaunch);
}