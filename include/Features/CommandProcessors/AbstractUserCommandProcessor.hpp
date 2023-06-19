#pragma once

class AbstractUserCommandProcessor
{
    protected:
        uint client = 0;
        static void PrintOk(ClientId client) { PrintUserCmdText(client, L"OK"); }

    public:
        virtual ~AbstractUserCommandProcessor() = default;
        virtual bool ProcessCommand(ClientId client, std::wstring_view cmd, const std::vector<std::wstring>& paramVector) = 0;
};

template <class T>
concept IsUserCommandProcessor = std::is_base_of_v<AbstractUserCommandProcessor, T>;
