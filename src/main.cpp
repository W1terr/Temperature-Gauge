#include "IHud.h"
#include "Menus.h"
#include "Needs.h"
#include "Settings.h"
#include "SmfHuds.h"
#include "Stress.h"
#include "Temperature.h"
#include "UI.h"

namespace
{
	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type == SKSE::MessagingInterface::kPostLoad) {
			SmfHuds::Install();  // plugins that loaded after this one (they register their HUD at kPostLoad, after us)
			return;
		}
		if (a_msg->type != SKSE::MessagingInterface::kDataLoaded) {
			return;
		}
		{
			std::scoped_lock lock(Config::Lock());
			Config::Load();
		}
		Temperature::Init();
		Needs::Init();
		Stress::Init();
		Menus::Register();
		IHud::Register();
		if (!UI::Register()) {
			RE::DebugMessageBox("Temperature Gauge needs SKSE Menu Framework (version 3 or newer). The gauge is disabled.");
		}
	}
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	logs::info("Temperature Gauge loading");
	SmfHuds::Install();  // plugins loaded before this one register their HUD at kPostLoad, before we'd hear of it
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
