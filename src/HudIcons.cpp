#include "HudIcons.h"

// The survival mods' own temperature icons live in the HUD movie:
// - Survival Mode: HUDMovieBaseInstance.TemperatureMeter_mc (vanilla and Edge UI hudmenu.swf). The engine only moves
//   it between its frames (Neutral / Fire / Warm / Cold / Freezing), so hiding the clip sticks.
// - SkyUI widgets (SunHelm's cold widget and need icons, Frostfall's exposure meter): _root.WidgetContainer.<id>.widget;
//   the mod may show it again, so it's hidden every time.
namespace HudIcons
{
	namespace
	{
		std::atomic<float>       alpha{ 1.0f };
		std::atomic<bool>        pending{ false };
		std::vector<std::string> hidden;  // UI thread only: paths we hid
		std::vector<std::string> missing; // UI thread only: logged once

		float NumberOf(RE::GFxMovieView* a_movie, const char* a_path)
		{
			RE::GFxValue value;
			if (a_movie->GetVariable(&value, a_path) && value.IsNumber()) {
				return static_cast<float>(value.GetNumber());
			}
			return 100.0f;
		}

		// false if the path isn't a clip in this HUD. Sets "<path>._visible" on the movie like Papyrus UI.SetBool does
		// (GFxValue::DisplayInfo didn't hide SunHelm's widget).
		bool SetVisible(RE::GFxMovieView* a_movie, const std::string& a_path, bool a_visible)
		{
			RE::GFxValue clip;
			if (!a_movie->GetVariable(&clip, a_path.c_str()) || !clip.IsDisplayObject()) {
				return false;
			}
			const auto   property = a_path + "._visible";
			RE::GFxValue visible;
			if (!a_movie->GetVariable(&visible, property.c_str()) || !visible.IsBool() || visible.GetBool() != a_visible) {
				a_movie->SetVariable(property.c_str(), RE::GFxValue{ a_visible });
			}
			return true;
		}

		void Run(const std::vector<std::string>& a_hide)
		{
			pending = false;
			const auto ui = RE::UI::GetSingleton();
			const auto hud = ui ? ui->GetMenu<RE::HUDMenu>() : nullptr;
			const auto movie = hud ? hud->uiMovie.get() : nullptr;
			if (!movie || !ui->IsMenuOpen(RE::HUDMenu::MENU_NAME)) {
				alpha = 0.0f;
				return;
			}
			// Immersive HUD fades _root; some HUD modes fade the base instance
			alpha = std::clamp(NumberOf(movie, "_root._alpha") / 100.0f, 0.0f, 1.0f) *
			        std::clamp(NumberOf(movie, "_root.HUDMovieBaseInstance._alpha") / 100.0f, 0.0f, 1.0f);

			// only give back what we took
			std::erase_if(hidden, [&](const std::string& a_path) {
				if (std::ranges::find(a_hide, a_path) != a_hide.end()) {
					return false;
				}
				SetVisible(movie, a_path, true);
				logs::info("Showed the icon {} again", a_path);
				return true;
			});
			for (const auto& path : a_hide) {
				if (SetVisible(movie, path, false)) {
					if (std::ranges::find(hidden, path) == hidden.end()) {
						hidden.push_back(path);
						logs::info("Hid the icon {}", path);
					}
				} else if (std::ranges::find(missing, path) == missing.end()) {
					missing.push_back(path);
					logs::info("No icon at {} (yet)", path);
				}
			}
		}
	}

	void Update(std::vector<std::string> a_hide)
	{
		if (pending.exchange(true)) {
			return;
		}
		SKSE::GetTaskInterface()->AddUITask([paths = std::move(a_hide)] { Run(paths); });
	}

	float Alpha()
	{
		return alpha;
	}
}
