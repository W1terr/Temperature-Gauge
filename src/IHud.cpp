#include "IHud.h"

namespace IHud
{
	namespace
	{
		constexpr std::size_t kGroups = 3;
		// menu names and movies (Immersive HUD's MCM shows the file names); tools/widget_swf.py writes the movies
		constexpr std::array<const char*, kGroups> kMenus{ "TemperatureGauge", "TemperatureGaugeNeeds", "TemperatureGaugeStress" };
		constexpr std::array<const char*, kGroups> kMovies{ "TemperatureGauge/TemperatureGauge", "TemperatureGauge/TemperatureGaugeNeeds",
			"TemperatureGauge/TemperatureGaugeStress" };

		bool                                    installed{ false };
		std::array<std::atomic<float>, kGroups> alphas{ 1.0f, 1.0f, 1.0f };
		std::array<std::atomic<bool>, kGroups>  open{};  // the menu is open (else its alpha is stale)

		template <Group G>
		class WidgetMenu final : public RE::IMenu
		{
		public:
			static constexpr auto kIndex = static_cast<std::size_t>(G);

			WidgetMenu()
			{
				using Flag = RE::UI_MENU_FLAGS;
				menuFlags.set(Flag::kAlwaysOpen, Flag::kRequiresUpdate, Flag::kAllowSaving);
				depthPriority = 0;
				inputContext = Context::kNone;
				alphas[kIndex] = 1.0f;  // a new movie is fully visible until Immersive HUD fades it
				if (!RE::BSScaleformManager::GetSingleton()->LoadMovie(this, uiMovie, kMovies[kIndex])) {
					logs::error("Immersive HUD: couldn't load Interface/{}.swf", kMovies[kIndex]);
				}
			}

			~WidgetMenu() override { open[kIndex] = false; }

			static RE::IMenu* Create() { return new WidgetMenu(); }

			RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override
			{
				if (a_message.type == RE::UI_MESSAGE_TYPE::kShow) {
					open[kIndex] = true;
				} else if (a_message.type == RE::UI_MESSAGE_TYPE::kHide || a_message.type == RE::UI_MESSAGE_TYPE::kForceHide) {
					open[kIndex] = false;
				}
				return IMenu::ProcessMessage(a_message);
			}

			void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override
			{
				IMenu::AdvanceMovie(a_interval, a_currentTime);
				RE::GFxValue root;
				if (!uiMovie || !uiMovie->GetVariable(&root, "_root") || !root.IsDisplayObject()) {
					return;
				}
				RE::GFxValue::DisplayInfo info;
				if (root.GetDisplayInfo(&info)) {
					alphas[kIndex] = info.GetVisible() ? static_cast<float>(std::clamp(info.GetAlpha() / 100.0, 0.0, 1.0)) : 0.0f;
					open[kIndex] = true;
				}
			}
		};
	}

	void Register()
	{
		installed = REX::W32::GetModuleHandleW(L"ImmersiveHUD.dll") != nullptr;
		if (!installed) {
			return;
		}
		const auto ui = RE::UI::GetSingleton();
		ui->Register(kMenus[0], WidgetMenu<Group::kTemperature>::Create);
		ui->Register(kMenus[1], WidgetMenu<Group::kNeeds>::Create);
		ui->Register(kMenus[2], WidgetMenu<Group::kStress>::Create);
		logs::info("Immersive HUD SKSE found: the gauges follow its menus {}, {} and {}", kMenus[0], kMenus[1], kMenus[2]);
	}

	void KeepOpen()
	{
		const auto ui = RE::UI::GetSingleton();
		if (!installed || !ui || !ui->IsMenuOpen(RE::HUDMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
			return;
		}
		const auto queue = RE::UIMessageQueue::GetSingleton();
		for (std::size_t i = 0; i < kGroups; ++i) {
			if (!ui->IsMenuOpen(kMenus[i])) {
				if (open[i].exchange(false)) {
					logs::info("Immersive HUD: {} was closed (a loading screen?), opening it again", kMenus[i]);
				}
				queue->AddMessage(kMenus[i], RE::UI_MESSAGE_TYPE::kShow, nullptr);
			}
		}
	}

	float Alpha(Group a_group)
	{
		const auto i = static_cast<std::size_t>(a_group);
		return installed && open[i].load(std::memory_order_relaxed) ? alphas[i].load(std::memory_order_relaxed) : 1.0f;
	}
}
