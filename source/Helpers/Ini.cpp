#include "Global.hpp"

namespace Hk::Ini
{
	struct FlhookPlayerData
	{
		std::string charfilename;
		std::map<std::wstring, std::wstring> lines;
	};

	std::map<uint, FlhookPlayerData> clients;

	std::string GetAccountDir(ClientId client)
	{
		static auto GetFLName = (_GetFLName)((char*)server + 0x66370);
		char dirname[1024];
		GetFLName(dirname, Players[client].account->accId);
		return dirname;
	}

	std::string GetCharfilename(const std::wstring& charname)
	{
		static auto GetFLName = (_GetFLName)((char*)server + 0x66370);
		char filename[1024];
		GetFLName(filename, charname.c_str());
		return filename;
	}

	static PlayerData* currPlayer;

	int __stdcall UpdateFile(char* filename, wchar_t* savetime, int b)
	{
		// Call the original save charfile function
		int retv = 0;
		__asm {
			pushad
			mov ecx, [currPlayer]
			push b
			push savetime
			push filename
			mov eax, 0x6d4ccd0
			call eax
			mov retv, eax
			popad
			}

		// Readd the flhook section.
		if (retv)
		{
			ClientId client = currPlayer->onlineId;

			std::string path = CoreGlobals::c()->accPath + GetAccountDir(client) + "\\" + filename;

			const bool encryptFiles = !FLHookConfig::c()->general.disableCharfileEncryption;

			const auto writeFlhookSection = [client](std::string& str) {
				str += "\n[flhook]\n";
				for (const auto& [key, value] : clients[client].lines)
				{
					str += wstos(std::format(L"{} = {}\n", key, value));
				}
			};

			std::fstream saveFile;
			std::string data;
			if (encryptFiles)
			{
				saveFile.open(path, std::ios::ate | std::ios::in | std::ios::out | std::ios::binary);

				// Copy old version that we plan to rewrite
				auto size = static_cast<size_t>(saveFile.tellg());
				std::string buffer(size, ' ');
				saveFile.seekg(0);
				saveFile.read(&buffer[0], size);

				// Reset the file pointer so we can start overwriting
				saveFile.seekg(0);
				buffer = FlcDecode(buffer);
				writeFlhookSection(buffer);
				data = FlcEncode(buffer);
			}
			else
			{
				saveFile.open(path, std::ios::app | std::ios::binary);
				writeFlhookSection(data);
			}

			saveFile.write(data.c_str(), data.size());
			saveFile.close();
		}

		return retv;
	}

	__declspec(naked) void UpdateFileNaked()
	{
		__asm {
			mov currPlayer, ecx
			jmp UpdateFile
			}
	}

	void CharacterClearClientInfo(ClientId client) { clients.erase(client); }

	void CharacterSelect(const CHARACTER_ID charId, ClientId client)
	{
		const std::string path = CoreGlobals::c()->accPath + GetAccountDir(client) + "\\" + charId.charFilename;

		clients[client].charfilename = charId.charFilename;
		clients[client].lines.clear();

		// Read the flhook section so that we can rewrite after the save so that it isn't lost
		INI_Reader ini;
		if (ini.open(path.c_str(), false))
		{
			while (ini.read_header())
			{
				if (ini.is_header("flhook"))
				{
					std::wstring tag;
					while (ini.read_value())
					{
						clients[client].lines[stows(ini.get_name_ptr())] = stows(ini.get_value_string());
					}
				}
			}
			ini.close();
		}
	}

	static bool patched = false;

	void CharacterInit()
	{
		clients.clear();
		if (patched)
			return;

		PatchCallAddr((char*)server, 0x6c547, (char*)UpdateFileNaked);
		PatchCallAddr((char*)server, 0x6c9cd, (char*)UpdateFileNaked);

		patched = true;
	}

	void CharacterShutdown()
	{
		if (!patched)
			return;

		const BYTE patch[] = {0xE8, 0x84, 0x07, 0x00, 0x00};
		WriteProcMem((char*)server + 0x6c547, patch, 5);

		const BYTE patch2[] = {0xE8, 0xFE, 0x2, 0x00, 0x00};
		WriteProcMem((char*)server + 0x6c9cd, patch2, 5);

		patched = false;
	}

	cpp::result<std::wstring, Error> GetFromPlayerFile(const std::variant<uint, std::wstring>& player, const std::wstring& Key)
	{
		std::wstring ret;
		const auto client = Client::ExtractClientID(player);
		const auto acc = Client::GetAccountByClientID(client);
		const auto dir = Client::GetAccountDirName(acc);

		auto file = Client::GetCharFileName(player);
		if (file.has_error())
		{
			return cpp::fail(file.error());
		}

		if (const std::string charFile = CoreGlobals::c()->accPath + wstos(dir) + "\\" + wstos(file.value()) + ".fl"; Client::IsEncoded(charFile))
		{
			const std::string charFileNew = charFile + ".ini";
			if (!FlcDecodeFile(charFile.c_str(), charFileNew.c_str()))
				return cpp::fail(Error::CouldNotDecodeCharFile);

			ret = stows(IniGetS(charFileNew, "Player", wstos(Key), ""));
			DeleteFile(charFileNew.c_str());
		}
		else
		{
			ret = stows(IniGetS(charFile, "Player", wstos(Key), ""));
		}

		return ret;
	}

	cpp::result<void, Error> WriteToPlayerFile(const std::variant<uint, std::wstring>& player, const std::wstring& Key, const std::wstring& Value)
	{
		const auto client = Client::ExtractClientID(player);
		const auto acc = Client::GetAccountByClientID(client);
		const auto dir = Client::GetAccountDirName(acc);

		auto file = Client::GetCharFileName(player);
		if (file.has_error())
		{
			return cpp::fail(file.error());
		}

		if (const std::string charFile = CoreGlobals::c()->accPath + wstos(dir) + "\\" + wstos(file.value()) + ".fl"; Client::IsEncoded(charFile))
		{
			const std::string charFileNew = charFile + ".ini";
			if (!FlcDecodeFile(charFile.c_str(), charFileNew.c_str()))
				return cpp::fail(Error::CouldNotDecodeCharFile);

			IniWrite(charFileNew, "Player", wstos(Key), wstos(Value));

			// keep decoded
			DeleteFile(charFile.c_str());
			MoveFile(charFileNew.c_str(), charFile.c_str());
		}
		else
		{
			IniWrite(charFile, "Player", wstos(Key), wstos(Value));
		}

		return {};
	}

	std::wstring GetCharacterIniString(ClientId client, const std::wstring& name)
	{
		if (!clients.contains(client))
			return L"";

		if (!clients[client].charfilename.length())
			return L"";

		if (!clients[client].lines.contains(name))
			return L"";

		auto line = clients[client].lines[name];
		return line;
	}

	void SetCharacterIni(ClientId client, const std::wstring& name, std::wstring value)
	{
		clients[client].lines[name] = std::move(value);
	}

	bool GetCharacterIniBool(ClientId client, const std::wstring& name)
	{
		const auto val = GetCharacterIniString(client, name);
		return val == L"true" || val == L"1";
	}

	int GetCharacterIniInt(ClientId client, const std::wstring& name)
	{
		const auto val = GetCharacterIniString(client, name);
		return wcstol(val.c_str(), nullptr, 10);
	}

	uint GetCharacterIniUint(ClientId client, const std::wstring& name)
	{
		const auto val = GetCharacterIniString(client, name);
		return wcstoul(val.c_str(), nullptr, 10);
	}

	float GetCharacterIniFloat(ClientId client, const std::wstring& name)
	{
		const auto val = GetCharacterIniString(client, name);
		return wcstof(val.c_str(), nullptr);
	}

	double GetCharacterIniDouble(ClientId client, const std::wstring& name)
	{
		const auto val = GetCharacterIniString(client, name);
		return wcstod(val.c_str(), nullptr);
	}

	int64 GetCharacterIniInt64(ClientId client, const std::wstring& name)
	{
		const auto val = GetCharacterIniString(client, name);
		return wcstoll(val.c_str(), nullptr, 10);
	}
} // namespace Hk::Ini
