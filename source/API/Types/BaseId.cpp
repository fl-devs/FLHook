#include "PCH.hpp"

#include "API/Types/BaseId.hpp"

#define ValidBaseCheck                            \
    if (!this->operator bool())                   \
    {                                             \
        return { cpp::fail(Error::InvalidBase) }; \
    }

BaseId::BaseId(const std::wstring_view name, const bool isWildCard)
{
    const auto baseName = StringUtils::ToLower(name);

    const Universe::IBase* baseInfo = Universe::GetFirstBase();
    if (!isWildCard)
    {
        while (baseInfo)
        {
            static std::array<char, 1024> baseNickname;
            std::fill_n(baseNickname, baseNickname.size(), '\0');
            pub::GetBaseNickname(baseNickname.data(), baseNickname.size(), baseInfo->baseId);

            if (const auto basename = FLHook::GetInfocardManager().GetInfocard(baseInfo->baseIdS);
                StringUtils::ToLower(StringUtils::stows(baseNickname.data())) == StringUtils::ToLower(baseName) ||
                StringUtils::ToLower(basename).find(StringUtils::ToLower(baseName)) != std::wstring::npos)
            {
                value = baseInfo->baseId;
                return;
            }
            baseInfo = Universe::GetNextBase();
        }

        return;
    }

    while (baseInfo)
    {
        if (const auto basename = FLHook::GetInfocardManager().GetInfocard(baseInfo->baseIdS);
            StringUtils::ToLower(basename).find(StringUtils::ToLower(baseName)) != std::wstring::npos)
        {
            value = baseInfo->baseId;
            return;
        }
        baseInfo = Universe::GetNextBase();
    };
}

BaseId::operator bool() const { return Universe::get_base(value) != nullptr; }

Action<ObjectId, Error> BaseId::GetSpaceId() const
{
    ValidBaseCheck;

    const auto base = Universe::get_base(value);
    if (!this->operator bool())
    {
        return { cpp::fail(Error::InvalidBase) };
    }

    return { ObjectId(base->spaceObjId) };
}

Action<RepId, Error> BaseId::GetAffiliation() const
{
    const auto objectId = GetSpaceId().Unwrap();
    if (!objectId)
    {
        return { cpp::fail(Error::InvalidBase) };
    }

    return { RepId(objectId, true) };
}

Action<std::wstring_view, Error> BaseId::GetName() const
{
    const auto base = Universe::get_base(value);
    if (!base)
    {
        return { cpp::fail(Error::InvalidBase) };
    }

    return { FLHook::GetInfocardManager().GetInfocard(base->baseIdS) };
}
Action<std::pair<std::wstring_view, std::wstring_view>, Error> BaseId::GetDescription()
{
    ValidBaseCheck;
    // TODO: Get internal description of base
}

Action<std::vector<uint>, Error> BaseId::GetItemsForSale() const
{
    ValidBaseCheck;

    std::array<byte, 2> nop = { 0x90, 0x90 };
    std::array<byte, 2> jnz = { 0x75, 0x1D };
    MemUtils::WriteProcMem(FLHook::Offset(FLHook::BinaryType::Server, AddressList::GetCommodities), nop.data(), 2); // patch, else we only get commodities

    static std::array<uint, 1024> arr;
    std::fill_n(arr, arr.size(), 0);

    int size = 256;
    pub::Market::GetCommoditiesForSale(value, arr.data(), &size);
    MemUtils::WriteProcMem(FLHook::Offset(FLHook::BinaryType::Server, AddressList::GetCommodities), jnz.data(), 2);

    return { std::vector(arr.begin(), arr.begin() + size) };
}