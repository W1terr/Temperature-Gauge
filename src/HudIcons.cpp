#include "HudIcons.h"

// The survival mods' own temperature icons live in the HUD movie:
// - Survival Mode: HUDMovieBaseInstance.TemperatureMeter_mc (vanilla and Edge UI hudmenu.swf). The engine only moves
//   it between its frames (Neutral / Fire / Warm / Cold / Freezing), but Immersive HUD SKSE makes it visible again
//   every frame, so it's also shrunk to nothing (_xscale / _yscale 0; Immersive HUD only sets visibility and alpha).
// - SkyUI widgets (SunHelm's cold widget and need icons, Frostfall's exposure meter): _root.WidgetContainer.<id>.widget;
//   the mod may show it again, so it's hidden every time.
namespace HudIcons
{
	namespace
	{
		// a clip we hid and the size it had
		struct Hidden
		{
			std::string path;
			double      xscale{ 100.0 };
			double      yscale{ 100.0 };
		};

		std::atomic<float>       alpha{ 1.0f };
		std::atomic<bool>        pending{ false };
		std::vector<Hidden>      hidden;   // UI thread only: clips we hid
		std::vector<std::string> missing;  // UI thread only: logged once

		float NumberOf(RE::GFxMovieView* a_movie, const char* a_path)
		{
			RE::GFxValue value;
			if (a_movie->GetVariable(&value, a_path) && value.IsNumber()) {
				return static_cast<float>(value.GetNumber());
			}
			return 100.0f;
		}

		bool IsClip(RE::GFxMovieView* a_movie, const std::string& a_path)
		{
			RE::GFxValue clip;
			return a_movie->GetVariable(&clip, a_path.c_str()) && clip.IsDisplayObject();
		}

		// Sets "<path>.<property>" on the movie like Papyrus UI.SetBool / SetNumber do (GFxValue::DisplayInfo didn't
		// hide SunHelm's widget); a_was: what it was before, if it had to change
		template <class T>
		bool Set(RE::GFxMovieView* a_movie, const std::string& a_path, const char* a_property, T a_value, double* a_was = nullptr)
		{
			const auto   property = a_path + a_property;
			RE::GFxValue now;
			if (a_movie->GetVariable(&now, property.c_str())) {
				if constexpr (std::is_same_v<T, bool>) {
					if (now.IsBool() && now.GetBool() == a_value) {
						return false;
					}
				} else {
					if (!now.IsNumber() || now.GetNumber() == a_value) {
						return false;
					}
					if (a_was) {
						*a_was = now.GetNumber();
					}
				}
			}
			a_movie->SetVariable(property.c_str(), RE::GFxValue{ a_value });
			return true;
		}

		void Hide(RE::GFxMovieView* a_movie, Hidden& a_clip)
		{
			Set(a_movie, a_clip.path, "._visible", false);
			// a size someone else gave it meanwhile is the one to give back
			Set(a_movie, a_clip.path, "._xscale", 0.0, &a_clip.xscale);
			Set(a_movie, a_clip.path, "._yscale", 0.0, &a_clip.yscale);
		}

		void Show(RE::GFxMovieView* a_movie, const Hidden& a_clip)
		{
			Set(a_movie, a_clip.path, "._xscale", a_clip.xscale);
			Set(a_movie, a_clip.path, "._yscale", a_clip.yscale);
			Set(a_movie, a_clip.path, "._visible", true);
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
			std::erase_if(hidden, [&](const Hidden& a_clip) {
				if (std::ranges::find(a_hide, a_clip.path) != a_hide.end()) {
					return false;
				}
				if (IsClip(movie, a_clip.path)) {
					Show(movie, a_clip);
				}
				logs::info("Showed the icon {} again", a_clip.path);
				return true;
			});
			for (const auto& path : a_hide) {
				if (!IsClip(movie, path)) {
					if (std::ranges::find(missing, path) == missing.end()) {
						missing.push_back(path);
						logs::info("No icon at {} (yet)", path);
					}
					continue;
				}
				auto it = std::ranges::find(hidden, path, &Hidden::path);
				if (it == hidden.end()) {
					it = hidden.insert(hidden.end(), Hidden{ path });
					logs::info("Hid the icon {}", path);
				}
				Hide(movie, *it);
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
