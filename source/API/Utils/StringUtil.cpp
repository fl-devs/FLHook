#include "PCH.hpp"

bool IsValidHex(std::wstring_view input)
{
    constexpr std::wstring_view characters = L"1234567890ABCDEFabcdf";
    return input.find_first_not_of(characters) == std::wstring::npos;
}

uint StringUtils::MultiplyUIntBySuffix(std::wstring_view valueString)
{
    const uint value = Cast<uint>(valueString);
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

std::wstring StringUtils::XmlText(std::wstring_view text)
{
    std::wstring ret;
    for (uint i = 0; i < text.length(); i++)
    {
        if (text[i] == '<')
        {
            ret.append(L"&#60;");
        }
        else if (text[i] == '>')
        {
            ret.append(L"&#62;");
        }
        else if (text[i] == '&')
        {
            ret.append(L"&#38;");
        }
        else
        {
            ret.append(1, text[i]);
        }
    }

    return ret;
}

std::wstring StringUtils::stows(const std::string& text)
{
    const int size = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, nullptr, 0);
    const auto wideText = new wchar_t[size];
    MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, wideText, size);
    std::wstring ret = wideText;
    delete[] wideText;
    return ret;
}

std::string StringUtils::wstos(const std::wstring& text)
{
    const uint len = text.length() + 1;
    const auto buf = new char[len];
    WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, buf, len, nullptr, nullptr);
    std::string ret = buf;
    delete[] buf;
    return ret;
}

std::wstring StringUtils::ToHex(std::wstring_view input)
{
    std::wostringstream output;

    for (const auto& c : input)
    {
        const auto val = static_cast<long>(c);
        output << std::uppercase << std::setfill(L'0') << std::setw(4) << std::hex << val;
    }
    return output.str();
}