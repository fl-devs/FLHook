#include "PCH.hpp"

#include "API/FLServer/Client.hpp"
#include "API/Utils/IniUtils.hpp"
#include "API/Utils/FileUtils.hpp"

namespace Hk
{
    struct FlhookPlayerData
    {
            std::wstring charfilename;
            std::map<std::wstring, std::wstring> lines;
    };

    std::map<uint, FlhookPlayerData> clients;

    std::wstring GetAccountDir(ClientId client)
    {
        static auto GetFLName = (_GetFLName)((char*)server + 0x66370);
        char dirname[1024];
        GetFLName(dirname, Players[client].account->accId);
        return StringUtils::stows(dirname);
    }

    std::wstring GetCharfilename(const std::wstring& charname)
    {
        static auto GetFLName = (_GetFLName)((char*)server + 0x66370);
        char filename[1024];
        GetFLName(filename, charname.c_str());
        return StringUtils::stows(filename);
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

            std::wstring path = std::format(L"{}{}\\{}", CoreGlobals::c()->accPath, GetAccountDir(client), StringUtils::stows(std::string(filename)));

            const bool encryptFiles = !FLHookConfig::c()->general.disableCharfileEncryption;

            const auto writeFlhookSection = [client](std::wstring& str)
            {
                str += L"\n[flhook]\n";
                for (const auto& [key, value] : clients[client].lines)
                {
                    str += std::format(L"{} = {}\n", key, value);
                }
            };

            std::wfstream saveFile;
            std::wstring data;
            if (encryptFiles)
            {
                saveFile.open(path, std::ios::ate | std::ios::in | std::ios::out | std::ios::binary);

                // Copy old version that we plan to rewrite
                auto size = static_cast<size_t>(saveFile.tellg());
                std::wstring buffer(size, ' ');
                saveFile.seekg(0);
                saveFile.read(buffer.data(), size);

                // Reset the file pointer so we can start overwriting
                saveFile.seekg(0);
                buffer = FlCodec::Decode(buffer);
                writeFlhookSection(buffer);
                data = FlCodec::Encode(buffer);
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

    void IniUtils::CharacterClearClientInfo(ClientId client) { clients.erase(client); }

    void IniUtils::CharacterSelect(const CHARACTER_ID charId, ClientId client) const
    {
        const auto fileName = StringUtils::stows(std::string(charId.charFilename));
        const std::wstring path = std::format(L"{}{}\\{}", CoreGlobals::c()->accPath, GetAccountDir(client), fileName);

        clients[client].charfilename = fileName;
        clients[client].lines.clear();

        // Read the flhook section so that we can rewrite after the save so that it isn't lost
        INI_Reader ini;
        if (ini.open(StringUtils::wstos(path).c_str(), false))
        {
            while (ini.read_header())
            {
                if (ini.is_header("flhook"))
                {
                    std::wstring tag;
                    while (ini.read_value())
                    {
                        clients[client].lines[StringUtils::stows(ini.get_name_ptr())] = StringUtils::stows(ini.get_value_string());
                    }
                }
            }
            ini.close();
        }
    }

    static bool patched = false;

    IniUtils::IniUtils()
    {
        clients.clear();
        if (patched)
        {
            return;
        }

        MemUtils::PatchCallAddr((char*)server, 0x6c547, (char*)UpdateFileNaked);
        MemUtils::PatchCallAddr((char*)server, 0x6c9cd, (char*)UpdateFileNaked);

        patched = true;
    }

    IniUtils::~IniUtils()
    {
        if (!patched)
        {
            return;
        }

        const BYTE patch[] = { 0xE8, 0x84, 0x07, 0x00, 0x00 };
        MemUtils::WriteProcMem((char*)server + 0x6c547, patch, 5);

        const BYTE patch2[] = { 0xE8, 0xFE, 0x2, 0x00, 0x00 };
        MemUtils::WriteProcMem((char*)server + 0x6c9cd, patch2, 5);

        patched = false;
    }

    std::wstring IniUtils::GetIniValue(const std::wstring& data, const std::wstring& section, const std::wstring& key)
    {
        std::wstringstream wss(data);
        inipp::Ini<wchar_t> ini;

        ini.parse(wss);
        const auto iniSection = ini.sections.find(section);
        if (iniSection == ini.sections.end())
        {
            return L"";
        }

        const auto kv = iniSection->second.find(key);
        return kv == iniSection->second.end() ? L"" : kv->second;
    }

    cpp::result<std::wstring, Error> IniUtils::GetFromFile(const std::wstring& filePath, const std::wstring& section, const std::wstring& key)
    {
        std::wstring ret;

        if (!std::filesystem::exists(filePath))
        {
            return { cpp::fail(Error::FileNotFound) };
        }

        std::wstring buffer;

        std::wifstream file(filePath);
        file.seekg(0, std::ios::end);
        buffer.resize(file.tellg());
        file.seekg(0);
        file.read(buffer.data(), buffer.size());

        return GetIniValue(filePath, section, key);
    }

    cpp::result<std::wstring, Error> IniUtils::GetFromPlayerFile(const std::variant<uint, std::wstring_view>& player, const std::wstring& key) const
    {
        std::wstring ret;

        const auto characterName =
            player.index() == 0 ? Client::GetCharacterNameByID(std::get<uint>(player)).Unwrap() : std::wstring(std::get<std::wstring_view>(player));

        const auto characterData = FileUtils::ReadCharacterFile(characterName);
        if (characterData.has_error())
        {
            return { cpp::fail(characterData.error()) };
        }

        return GetIniValue(characterData.value(), L"Player", key);
    }

    Action<void> IniUtils::WriteToPlayerFile(const std::variant<uint, std::wstring_view>& player, const std::wstring& key, const std::wstring& value) const
    {
        const auto characterName =
            player.index() == 0 ? Client::GetCharacterNameByID(std::get<uint>(player)).Unwrap() : std::wstring(std::get<std::wstring_view>(player));

        auto characterData = FileUtils::ReadCharacterFile(characterName);
        if (characterData.has_error())
        {
            return { cpp::fail(characterData.error()) };
        }

        std::wstringstream wss(characterData.value());
        inipp::Ini<wchar_t> ini;

        ini.parse(wss);
        ini.sections[L"Player"][key] = value;

        std::wstringstream output;
        ini.generate(output);

        return FileUtils::WriteCharacterFile(characterName, output.str());
    }

    std::wstring IniUtils::GetCharacterIni(ClientId client, const std::wstring& name)
    {
        if (!clients.contains(client))
        {
            return L"";
        }

        if (!clients[client].charfilename.length())
        {
            return L"";
        }

        if (!clients[client].lines.contains(name))
        {
            return L"";
        }

        auto line = clients[client].lines[name];
        return line;
    }

    void IniUtils::SetCharacterIni(ClientId client, const std::wstring& name, std::wstring value) const { clients[client].lines[name] = std::move(value); }

 
} // namespace Hk