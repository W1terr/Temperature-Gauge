#include "Menus.h"

namespace Menus
{
	namespace
	{
		constexpr std::array kNames{ RE::MainMenu::MENU_NAME, RE::LoadingMenu::MENU_NAME, RE::FaderMenu::MENU_NAME,
			RE::RaceSexMenu::MENU_NAME, RE::DialogueMenu::MENU_NAME, RE::MapMenu::MENU_NAME, RE::BookMenu::MENU_NAME,
			RE::LockpickingMenu::MENU_NAME, RE::Console::MENU_NAME, RE::FavoritesMenu::MENU_NAME, RE::SleepWaitMenu::MENU_NAME,
			RE::StatsMenu::MENU_NAME, RE::LevelUpMenu::MENU_NAME,
			// these pause the game, but not with Skyrim Souls RE (the game runs on behind them)
			RE::TweenMenu::MENU_NAME, RE::InventoryMenu::MENU_NAME, RE::MagicMenu::MENU_NAME, RE::ContainerMenu::MENU_NAME,
			RE::BarterMenu::MENU_NAME, RE::GiftMenu::MENU_NAME, RE::CraftingMenu::MENU_NAME, RE::JournalMenu::MENU_NAME,
			RE::TrainingMenu::MENU_NAME, RE::TutorialMenu::MENU_NAME, RE::MessageBoxMenu::MENU_NAME };

		std::array<std::atomic<bool>, kNames.size()> open{};

		class Watcher final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event) {
					const std::string_view name = a_event->menuName;
					for (std::size_t i = 0; i < kNames.size(); ++i) {
						if (name == kNames[i]) {
							open[i] = a_event->opening;
							break;
						}
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		Watcher watcher;
	}

	void Register()
	{
		const auto ui = RE::UI::GetSingleton();
		if (!ui) {
			return;
		}
		for (std::size_t i = 0; i < kNames.size(); ++i) {
			open[i] = ui->IsMenuOpen(kNames[i]);  // the main menu may be up already
		}
		ui->AddEventSink<RE::MenuOpenCloseEvent>(&watcher);
	}

	bool HudCovered()
	{
		return std::ranges::any_of(open, [](const std::atomic<bool>& a_open) { return a_open.load(std::memory_order_relaxed); });
	}
}
