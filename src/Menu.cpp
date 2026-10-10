#include "Framework.h"
#include "Gauge.h"
#include "Lang.h"
#include "Settings.h"
#include "Temperature.h"
#include "UI.h"

namespace UI
{
	using namespace ImGuiMCP;
	using Lang::F;
	using Lang::T;

	namespace
	{
		constexpr auto kSection = "Temperature Gauge";

		Preview preview;

		bool IsFrameworkInstalled()
		{
			return SKSEMenuFramework::IsInstalled() && GetMenuFrameworkModule() != nullptr;
		}

		void __stdcall OnFrameworkEvent(SKSEMenuFramework::Model::EventType a_type)
		{
			if (a_type == SKSEMenuFramework::Model::EventType::kCloseMenu) {
				preview.enabled = false;
				std::scoped_lock lock(Config::Lock());
				Config::Save();
			}
		}

		// translated label with a fixed ImGui id (the id must not change with the language or repeat on the page)
		std::string Id(const char* a_english, const char* a_id)
		{
			return std::format("{}###{}", T(a_english), a_id);
		}

		std::string Id(const char* a_english)
		{
			return Id(a_english, a_english);
		}

		void Hint(const char* a_english)
		{
			SameLine();
			TextDisabled("(?)");
			if (IsItemHovered()) {
				SetTooltip("%s", T(a_english));
			}
		}

		bool Color(const char* a_english, const char* a_id, RGBA& a_color)
		{
			return ColorEdit4(Id(a_english, a_id).c_str(), a_color.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
		}

		template <class E, std::size_t N>
		void EnumCombo(const char* a_english, E& a_value, const std::array<const char*, N>& a_names)
		{
			std::array<const char*, N> names;
			std::ranges::transform(a_names, names.begin(), [](const char* a_name) { return T(a_name); });
			int index = static_cast<int>(a_value);
			if (Combo(Id(a_english).c_str(), &index, names.data(), static_cast<int>(N))) {
				a_value = static_cast<E>(index);
			}
		}

		void RenderStatus()
		{
			const auto name = Temperature::SourceName();
			const auto loaded = Temperature::LoadedSources();
			const char* reading = !name.empty()  ? name.c_str() :
			                      loaded.empty() ? T("none (no supported survival mod is loaded)") :
			                                       T("none (the survival mod's cold is turned off)");
			Text("%s", F("Reading from: {}", reading).c_str());
			TextDisabled("%s", F("Supported here: {}", loaded.empty() ? T("none") : loaded.c_str()).c_str());
			if (const auto r = Temperature::Current()) {
				TextDisabled("%s", r->target ? F("Now: {}, gauge position {:.2f}, heading to {:.2f}", r->stage, r->body, *r->target).c_str() :
				                               F("Now: {}, gauge position {:.2f}", r->stage, r->body).c_str());
			}
		}

		void __stdcall RenderSettings()
		{
			std::scoped_lock lock(Config::Lock());
			auto&            s = Config::Get();
			const auto       seconds = T("%.1f s");
			const auto       pixels = T("%.0f px");

			RenderStatus();
			TextWrapped("%s", T("The gauge is previewed while this menu is open. Sizes are pixels at 1080p and scale with your resolution."));

			SeparatorText(T("General"));
			Checkbox(Id("Show the gauge").c_str(), &s.enabled);
			Checkbox(Id("Hide the vanilla temperature icon").c_str(), &s.hideVanillaIcon);
			Hint("Survival Mode's thermometer next to the compass, SunHelm's cold widget");
			static constexpr std::array visibilityNames{ "Always", "Only when not comfortable", "Never" };
			SetNextItemWidth(260.0f);
			EnumCombo("Show", s.visibility, visibilityNames);
			if (s.visibility == Visibility::kNotComfortable) {
				SliderFloat(Id("Hide after").c_str(), &s.hideDelay, 0.0f, 30.0f, seconds);
				Hint("How long you have to be warm or comfortable (and not getting colder) before the gauge fades out");
			}
			Checkbox(Id("Fade with the HUD").c_str(), &s.followHud);
			Hint("Follows the HUD's opacity, e.g. Immersive HUD hiding it");
			SliderFloat(Id("Opacity").c_str(), &s.opacity, 0.05f, 1.0f, "%.2f");
			SliderFloat(Id("Marker smoothing").c_str(), &s.smoothing, 0.0f, 3.0f, seconds);
			Hint("Survival mods change your temperature in steps; the marker glides to the new value over this time");

			SeparatorText(T("Layout"));
			if (Button(Id("Half circle").c_str())) {
				Config::Apply(Config::Preset::kHalfCircle);
			}
			SameLine();
			if (Button(Id("Straight, vertical").c_str())) {
				Config::Apply(Config::Preset::kVertical);
			}
			SameLine();
			if (Button(Id("Straight, under the compass").c_str())) {
				Config::Apply(Config::Preset::kHorizontal);
			}
			SliderFloat(Id("Position X").c_str(), &s.posX, 0.0f, 1.0f, "%.3f");
			SliderFloat(Id("Position Y").c_str(), &s.posY, 0.0f, 1.0f, "%.3f");
			SliderFloat(Id("Scale").c_str(), &s.scale, 0.25f, 4.0f, "%.2f");
			SliderFloat(Id("Length").c_str(), &s.length, 20.0f, 600.0f, pixels);
			SliderFloat(Id("Thickness").c_str(), &s.thickness, 1.0f, 30.0f, T("%.1f px"));
			SliderFloat(Id("Curve").c_str(), &s.curve, 0.0f, 300.0f, "%.0f\xC2\xB0");
			Hint("0 = a straight bar, 180 = half a circle");
			SliderFloat(Id("Rotation").c_str(), &s.rotation, -180.0f, 180.0f, "%.0f\xC2\xB0");
			Hint("0 = standing up with the hot end on top, 90 = lying with the hot end on the right");
			Checkbox(Id("Bend the other way").c_str(), &s.mirror);
			SameLine();
			Checkbox(Id("Hot end at the other end").c_str(), &s.flip);
			SameLine();
			Checkbox(Id("Fade the ends").c_str(), &s.fadeEnds);
			SameLine();
			Checkbox(Id("Round ends").c_str(), &s.roundEnds);
			SliderFloat(Id("Outline").c_str(), &s.outline, 0.0f, 6.0f, T("%.1f px"));

			SeparatorText(T("Zones"));
			SliderFloat(Id("Cold zone").c_str(), &s.coldZone, 0.05f, 0.9f, "%.2f");
			Hint("Part of the gauge's length that is blue (cold end to comfortable)");
			s.hotZone = std::min(s.hotZone, 0.95f - s.coldZone);
			SliderFloat(Id("Hot zone").c_str(), &s.hotZone, 0.0f, 0.95f - s.coldZone, "%.2f");
			Hint("Part that is red. The green comfortable zone gets the rest. Survival Mode and SunHelm have no overheating, so only other survival mods reach it");
			Checkbox(Id("Lines between the zones").c_str(), &s.zoneLines);
			SameLine();
			Checkbox(Id("Blend the zones").c_str(), &s.blendZones);

			SeparatorText(T("Marker"));
			static constexpr std::array markerNames{ "Arrow", "Line" };
			SetNextItemWidth(260.0f);
			EnumCombo("Marker", s.marker, markerNames);
			Checkbox(Id("Arrow on the inner side").c_str(), &s.markerInside);
			Hint("The side the curve bends towards; off puts the arrow on the outer side");
			SliderFloat(Id("Marker size").c_str(), &s.markerSize, 0.25f, 3.0f, "%.2f");
			Checkbox(Id("Show where the temperature is heading").c_str(), &s.showTarget);
			Hint("A hollow pointer on the other side: the cold the area, the time of day and the weather bring you to (or a heat source warming you up)");
			Checkbox(Id("Show warming up / cooling down").c_str(), &s.showTrend);
			Hint("Chevrons beside the marker; two when near a fire or in a freezing area");
			Checkbox(Id("Pulse when freezing").c_str(), &s.pulseDanger);
			Checkbox(Id("Show the stage name").c_str(), &s.showLabel);
			if (s.showLabel) {
				SliderFloat(Id("Text size").c_str(), &s.labelSize, 8.0f, 40.0f, pixels);
			}

			SeparatorText(T("Colors"));
			Color("Cold end", "colorColdEnd", s.coldEnd);
			SameLine();
			Color("Cold", "colorCold", s.coldEdge);
			SameLine();
			Color("Comfortable", "colorComfortable", s.comfortable);
			SameLine();
			Color("Hot", "colorHot", s.hotEdge);
			SameLine();
			Color("Hot end", "colorHotEnd", s.hotEnd);
			Color("Outline", "colorOutline", s.outlineColor);
			SameLine();
			Color("Arrow", "colorArrow", s.markerColor);
			SameLine();
			Color("Heading to", "colorTarget", s.targetColor);
			SameLine();
			Color("Text", "colorText", s.labelColor);

			SeparatorText(T("Preview"));
			Checkbox(Id("Try a temperature").c_str(), &preview.enabled);
			if (preview.enabled) {
				Checkbox(Id("Sweep").c_str(), &preview.sweep);
				if (!preview.sweep) {
					SliderFloat(Id("Temperature").c_str(), &preview.body, -1.0f, 2.0f, "%.2f");
					Hint("-1 .. 0 cold zone, 0 .. 1 comfortable, 1 .. 2 hot zone");
				}
				SliderFloat(Id("Heading to", "previewTarget").c_str(), &preview.target, -1.0f, 2.0f, "%.2f");
				SliderInt(Id("Trend").c_str(), &preview.trend, -2, 2);
				Hint("-2 cooling fast, -1 cooling, 0 steady, 1 warming, 2 near a fire");
			}

			Spacing();
			Separator();
			if (Button(Id("Reset everything to the defaults").c_str())) {
				Config::Reset();
			}
			SameLine();
			TextDisabled("%s", T("Settings are saved when the menu closes."));
		}

		void __stdcall RenderLanguages();

		// The sidebar pages: English name (the translation key), page, name it has in the framework now
		struct SidebarPage
		{
			const char*                              english;
			SKSEMenuFramework::Model::RenderFunction render;
			std::string                              shown;
		};
		std::array pages{
			SidebarPage{ "Settings", RenderSettings, {} },
			SidebarPage{ "Languages", RenderLanguages, {} },
		};
		std::atomic<bool> pageNamesOutdated{ false };

		// exported by SKSE Menu Framework 3.8+, not in its header we use: renames a section or page at runtime
		using RenameSectionFunction = bool (*)(const char* a_path, const char* a_newName);

		RenameSectionFunction RenameSection()
		{
			static const auto func = SKSEMenuFramework::Model::Internal::GetFunction<RenameSectionFunction>("RenameSection");
			return func;
		}

		void __stdcall RenderLanguages()
		{
			std::scoped_lock lock(Config::Lock());
			const auto       languages = Lang::All();
			const auto&      current = Lang::Current();

			SeparatorText(T("Language"));
			for (const auto& language : languages) {
				if (RadioButton(language.name, &language == &current)) {
					Lang::Set(language.id);
					pageNamesOutdated = true;
				}
			}
			Spacing();
			if (!RenameSection()) {
				TextWrapped("%s", T("The names of the pages on the left change after you restart the game."));
			}
			TextWrapped("%s", T("The survival stage names (Chilly, Freezing...) come from the survival mod and follow the game's language."));

			// without the right glyphs the framework draws "?" for every letter, so this is also shown in English
			if (const auto glyphs = Lang::Current().glyphs) {
				constexpr auto kGlyphHint = "If you see ? instead of letters: in SKSE Menu Framework open Options > Open Settings and set Character Glyphs to {}.";
				Spacing();
				TextWrapped("%s", F(kGlyphHint, glyphs).c_str());
				TextDisabled("%s", std::vformat(kGlyphHint, std::make_format_args(glyphs)).c_str());
			}
		}
	}

	bool SettingsOpen()
	{
		return IsFrameworkInstalled() && SKSEMenuFramework::IsAnyBlockingWindowOpened();
	}

	Preview& GetPreview()
	{
		return preview;
	}

	void UpdatePageNames()
	{
		if (!pageNamesOutdated.exchange(false) || !RenameSection()) {
			return;
		}
		for (auto& page : pages) {
			const std::string wanted = T(page.english);
			if (wanted == page.shown) {
				continue;
			}
			if (RenameSection()(std::format("{}/{}", kSection, page.shown).c_str(), wanted.c_str())) {
				page.shown = wanted;
			} else {
				logs::warn("SKSE Menu Framework didn't rename the page \"{}\" to \"{}\"", page.shown, wanted);
			}
		}
	}

	bool Register()
	{
		if (!IsFrameworkInstalled()) {
			logs::critical("SKSE Menu Framework is not installed, the temperature gauge can't be drawn");
			return false;
		}
		logs::info("SKSE Menu Framework {} found", SKSEMenuFramework::GetMenuFrameworkVersion());
		SKSEMenuFramework::SetSection(kSection);
		for (auto& page : pages) {
			page.shown = T(page.english);
			SKSEMenuFramework::AddSectionItem(page.shown, page.render);
		}
		SKSEMenuFramework::AddHudElement(RenderHud);
		SKSEMenuFramework::AddEvent(OnFrameworkEvent, 0.0f);
		return true;
	}
}
