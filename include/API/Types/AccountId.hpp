#pragma once

class AccountId final
{
        CAccount* value = nullptr;

    public:
        explicit AccountId(ClientId client);
        explicit AccountId(std::wstring_view characterName);
        explicit AccountId(CAccount* acc) : value(acc) {}
        explicit AccountId() = default;

        bool operator==(const CAccount* next) const { return value == next; }
        explicit operator bool() const;

        [[nodiscard]]
        CAccount* GetValue() const;

        Action<std::wstring, Error> GetDirectoryName() const;

        bool IsAdmin() const;

        Action<void, Error> UnBan() const;
        Action<void, Error> Ban(uint tempBanDays = 0) const;
        Action<void, Error> LockAccountAccess(bool kick) const;
        Action<void, Error> UnlockAccountAccess() const;
        Action<void, Error> Logout() const;
        Action<void, Error> CreateCharacter(std::wstring_view name) const;
        Action<void, Error> DeleteCharacter(std::wstring_view name) const;

        Action<void, Error> AddRoles(const std::vector<std::wstring_view>& roles);
        Action<void, Error> RemoveRoles(const std::vector<std::wstring_view>& roles, bool clear);
        Action<void, Error> SetRoles(const std::vector<std::wstring_view>& roles);
};
