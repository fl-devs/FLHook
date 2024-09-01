#include "PCH.hpp"
#include "Wardrobe.hpp"

#include "API/FLHook/ClientList.hpp"

namespace Plugins
{
    WardrobePlugin::WardrobePlugin(const PluginInfo& info) : Plugin(info) {}

	Task WardrobePlugin::UserCmdShowWardrobe(ClientId client, const std::wstring_view param)
	{
		if (StringUtils::ToLower(param) == L"head")
		{
			(void)client.Message(L"Heads:");
			std::wstring heads;
			for (const auto& [name, id] : config.heads)
				heads += (StringUtils::stows(name) + L" | ");
			(void)client.Message(heads);
		}
		else if (StringUtils::ToLower(param) == L"body")
		{
			(void)client.Message(L"Bodies:");
			std::wstring bodies;
			for (const auto& [name, id] : config.bodies)
				bodies += (StringUtils::stows(name) + L" | ");
			(void)client.Message(bodies);
		}

        co_return TaskStatus::Finished;
	}

	Task WardrobePlugin::UserCmdChangeCostume(ClientId client, const std::wstring_view type, const std::wstring_view costume)
	{
		if (type.empty() || costume.empty())
		{
			(void)client.Message(L"ERR Invalid parameters");
			co_return TaskStatus::Finished;
		}

		if (StringUtils::ToLower(type) == L"head")
		{
			if (!config.heads.contains(StringUtils::wstos(costume)))
			{
				(void)client.Message(L"ERR Head not found. Use \"/warehouse show head\" to get available heads.");
				co_return TaskStatus::Finished;
			}
			client.GetData().playerData->baseCostume.head = CreateID(config.heads[StringUtils::wstos(costume)].c_str());
		}
		else if (StringUtils::ToLower(type) == L"body")
		{
			if (!config.bodies.contains(StringUtils::wstos(costume)))
			{
				(void)client.Message(L"ERR Body not found. Use \"/warehouse show body\" to get available bodies.");
				co_return TaskStatus::Finished;
			}
			client.GetData().playerData->baseCostume.body = CreateID(config.bodies[StringUtils::wstos(costume)].c_str());
		}
		else
		{
			(void)client.Message(L"ERR Invalid parameters");
			co_return TaskStatus::Finished;
		}

		// Saving the characters forces an anti-cheat checks and fixes
		// up a multitude of other problems.
        (void)client.SaveChar();
        (void)client.Kick(L"Updating character, please wait 10 seconds before reconnecting");

        co_return TaskStatus::Finished;
	}

	Task WardrobePlugin::UserCmdHandle(ClientId client, const std::wstring_view command, const std::wstring_view param, const std::wstring_view param2)
	{
		// Check character is in base
		if (!client.IsDocked())
		{
			(void)client.Message(L"ERR Not in base");
			co_return TaskStatus::Finished;
		}

		if (command == L"list")
		{
			UserCmdShowWardrobe(client, param);
		}
		else if (command == L"change")
		{
			UserCmdChangeCostume(client, param, param2);
		}
		else
		{
			(void)client.Message(L"Command usage:");
			(void)client.Message(L"/wardrobe list <head/body> - lists available bodies/heads");
			(void)client.Message(L"/wardrobe change <head/body> <name> - changes your head/body to the chosen model");
		}

        co_return TaskStatus::Finished;
	}

	void WardrobePlugin::OnLoadSettings()
	{
	    if (const auto conf = Json::Load<Config>("config/wardrobe.json"); !conf.has_value())
	    {
	        Json::Save(config, "config/wardrobe.json");
	    }
	    else
	    {
	        config = conf.value();
	    }
	}

} // namespace Plugins::Wardrobe

using namespace Plugins;

DefaultDllMain();

const PluginInfo Info(L"Wardrobe", L"wardrobe", PluginMajorVersion::V05, PluginMinorVersion::V00);
SetupPlugin(WardrobePlugin, Info);
