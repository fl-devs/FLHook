#pragma once

inline HWND GetFLServerHwnd()
{
	const auto* unkThis = (void*)0x00426C58;
	return *(HWND*)(*((DWORD*)unkThis + 8) + 32);
}

inline void SwapBytes(void* ptr, uint len)
{
	if (len % 4)
		return;

	for (uint i = 0; i < len; i += 4)
	{
		char* ptr1 = static_cast<char*>(ptr) + i;
		unsigned long temp;
		memcpy(&temp, ptr1, 4);
		const auto ptr2 = (char*)&temp;
		memcpy(ptr1, ptr2 + 3, 1);
		memcpy(ptr1 + 1, ptr2 + 2, 1);
		memcpy(ptr1 + 2, ptr2 + 1, 1);
		memcpy(ptr1 + 3, ptr2, 1);
	}
}

inline void WriteProcMem(void* address, const void* mem, int size)
{
	const HANDLE hProc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
	DWORD old;
	VirtualProtectEx(hProc, address, size, PAGE_EXECUTE_READWRITE, &old);
	WriteProcessMemory(hProc, address, mem, size, nullptr);
	CloseHandle(hProc);
}

inline void ReadProcMem(void* address, void* mem, int size)
{
	const HANDLE hProc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
	DWORD old;
	VirtualProtectEx(hProc, address, size, PAGE_EXECUTE_READWRITE, &old);
	ReadProcessMemory(hProc, address, mem, size, nullptr);
	CloseHandle(hProc);
}

inline int ToInt(const std::wstring& str)
{
	return wcstol(str.c_str(), nullptr, 10);
}

inline int64 ToInt64(const std::wstring& str)
{
	return str.empty() ? 0 : wcstoll(str.c_str(), nullptr, 10);
}

inline uint ToUInt(const std::wstring& str)
{
	if (str.find(L"-") != std::wstring::npos)
	{
		return 0;
	}
	return wcstoul(str.c_str(), nullptr, 10);
}

//! Converts numeric value with a metric suffix to the full value, eg 10k translates to 10000
inline uint MultiplyUIntBySuffix(const std::wstring& valueString)
{
	const uint value = wcstoul(valueString.c_str(), nullptr, 10);
	const auto lastChar = valueString.back();
	if (lastChar == *L"k" || lastChar == *L"K")
	{
		return value * 1000;
	}
	if (lastChar == *L"m" || lastChar == *L"M")
	{
		return value * 1000000;
	}
	return value;
}

inline std::chrono::sys_time<std::chrono::seconds> UnixToSysTime(int64 time)
{
	return std::chrono::sys_time<std::chrono::seconds>{std::chrono::seconds{time}};
}

inline std::wstring XMLText(const std::wstring& text)
{
	std::wstring Ret;
	for (uint i = 0; (i < text.length()); i++)
	{
		if (text[i] == '<')
			Ret.append(L"&#60;");
		else if (text[i] == '>')
			Ret.append(L"&#62;");
		else if (text[i] == '&')
			Ret.append(L"&#38;");
		else
			Ret.append(1, text[i]);
	}

	return Ret;
}

/**
Remove leading and trailing spaces from the std::string  ~FlakCommon by Motah.
*/
template<typename Str>
Str Trim(const Str& stringInput)
	requires StringRestriction<Str>
{
	if (stringInput.empty())
		return stringInput;

	using Char = typename Str::value_type;
	constexpr auto trimmable = []() constexpr {
		if constexpr (std::is_same_v<Char, char>)
			return " \t\n\r";
		else if constexpr (std::is_same_v<Char, wchar_t>)
			return L" \t\n\r";
	}();

	auto start = stringInput.find_first_not_of(trimmable);
	auto end = stringInput.find_last_not_of(trimmable);

	if (start == end)
		return stringInput;

	return stringInput.substr(start, end - start + 1);
}

template<typename TString>
TString ExpandEnvironmentVariables(const TString& input)
{
	std::string accumulator = "";
	std::string output = "";
	bool percentFound = false;

	for (uint i = 0; i < input.length(); i++)
	{
		const auto ch = input[i];
		if (ch == '%')
		{
			if (percentFound || (input[i + 1] != '%'))
			{
				percentFound = !percentFound;
				if (percentFound)
					accumulator.clear();
				else
				{
					auto var = std::getenv(accumulator.c_str());
					accumulator = var ? var : accumulator;
					output += accumulator;
				}
			}
			else
			{
				i++; // Extra percentage sign, escape it.
			}
		}
		else
		{
			if (percentFound)
				accumulator += ch;
			else
				output += ch;
		}
	}

	TString ret = Trim(output);
	return ret;
}

template<typename TStr, typename TChar>
TStr GetParam(const TStr& line, TChar splitChar, uint pos)
	requires StringRestriction<TStr>
{
	uint i;
	uint j;

	TStr result;
	for (i = 0, j = 0; (i <= pos) && (j < line.length()); j++)
	{
		if (line[j] == splitChar)
		{
			while (((j + 1) < line.length()) && (line[j + 1] == splitChar))
				j++; // skip "whitechar"

			i++;
			continue;
		}

		if (i == pos)
			result += line[j];
	}
	return result;
}

template<typename TString, typename TChar>
TString GetParamToEnd(const TString& line, TChar splitChar, uint pos)
	requires StringRestriction<TString>
{
	for (uint i = 0, j = 0; (i <= pos) && (j < line.length()); j++)
	{
		if (line[j] == splitChar)
		{
			while (((j + 1) < line.length()) && (line[j + 1] == splitChar))
				j++; // skip "whitechar"
			i++;
			continue;
		}
		if (i == pos)
		{
			return line.substr(j);
		}
	}

	return TString();
}

template<typename TString>
auto Split(const TString& input, const TString& splitCharacter)
	requires StringRestriction<TString>
{
	auto inputCopy = input;
	size_t pos = 0;
	std::vector<TString> tokens;
	while ((pos = inputCopy.find(splitCharacter)) != TString::npos)
	{
		TString token = inputCopy.substr(0, pos);
		tokens.emplace_back(token);
		inputCopy.erase(0, pos + splitCharacter.length());
	}

	if (!inputCopy.empty() && inputCopy.size() != input.size())
	{
		tokens.emplace_back(inputCopy);
	}
	return tokens;
}

template<typename TString, typename TChar>
auto Split(const TString& input, const TChar& splitCharacter)
	requires StringRestriction<TString>
{
	return Split(input, TString(1, splitCharacter));
}

template<typename TString, typename TTStr, typename TTTStr>
TString ReplaceStr(const TString& source, const TTStr& searchForRaw, const TTTStr& replaceWithRaw)
	requires StringRestriction<TString>
{
	const TString searchFor = searchForRaw;
	const TString replaceWith = replaceWithRaw;

	uint lPos, sPos = 0;

	TString result = source;
	while ((lPos = static_cast<uint>(result.find(searchFor, sPos))) != UINT_MAX)
	{
		result.replace(lPos, searchFor.length(), replaceWith);
		sPos = lPos + replaceWith.length();
	}

	return result;
}

template<typename T>
std::wstring ToMoneyStr(T cash)
{
	std::wstringstream ss;
	ss.imbue(std::locale(""));
	ss << std::fixed << cash;
	return ss.str();
}

inline float ToFloat(const std::wstring& string)
{
	return wcstof(string.c_str(), nullptr);
}

inline FARPROC PatchCallAddr(char* mod, DWORD installAddress, const char* hookFunction)
{
	DWORD relAddr;
	ReadProcMem(mod + installAddress + 1, &relAddr, 4);

	const DWORD offset = (DWORD)hookFunction - (DWORD)(mod + installAddress + 5);
	WriteProcMem(mod + installAddress + 1, &offset, 4);

	return (FARPROC)(mod + relAddr + installAddress + 5);
}

inline std::wstring ToLower(std::wstring string)
{
	std::transform(string.begin(), string.end(), string.begin(), towlower);
	return string;
}

inline std::string ToLower(std::string string)
{
	std::transform(string.begin(), string.end(), string.begin(), tolower);
	return string;
}

inline std::wstring ViewToWString(const std::wstring& wstring)
{
	return {wstring.begin(), wstring.end()};
}

inline std::string ViewToString(const std::string_view& stringView)
{
	return {stringView.begin(), stringView.end()};
}

inline std::wstring stows(const std::string& text)
{
	const int size = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, nullptr, 0);
	const auto wideText = new wchar_t[size];
	MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, wideText, size);
	std::wstring ret = wideText;
	delete[] wideText;
	return ret;
}

inline std::string wstos(const std::wstring& text)
{
	const uint len = text.length() + 1;
	const auto buf = new char[len];
	WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, buf, len, nullptr, nullptr);
	std::string ret = buf;
	delete[] buf;
	return ret;
}

template<typename TStr>
auto strswa(TStr str)
	requires StringRestriction<TStr>
{
	if constexpr (std::is_same_v<TStr, std::string>)
	{
		return stows(str);
	}
	else
	{
		return wstos(str);
	}
}
