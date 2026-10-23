#include "Framework.h"
#include "Gauge.h"
#include "Lang.h"
#include "Needs.h"
#include "NeedsGauge.h"
#include "Settings.h"
#include "Stress.h"
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
		bool    arcHint{ false };  // "put them on the temperature gauge's circle" found it straight

		bool IsFrameworkInstalled()
		{
			return SKSEMenuFramework::IsInstalled() && GetMenuFrameworkModule() != nullptr;
		}

		void __stdcall OnFrameworkEvent(SKSEMenuFramework::Model::EventType a_type)
		{
			if (a_type == SKSEMenuFramework::Model::EventType::kCloseMenu) {
				preview.enabled = false;
				preview.needs = false;
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
				const char* stage = T(r->stage.c_str());
				TextDisabled("%s", r->target ? F("Now: {}, gauge position {:.2f}, heading to {:.2f}", stage, r->body, *r->target).c_str() :
				                               F("Now: {}, gauge position {:.2f}", stage, r->body).c_str());
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
			Hint("The survival mod's own temperature icon: Survival Mode's thermometer, SunHelm's cold widget, Frostfall's exposure meter, Frostbite's meter");
			static constexpr std::array visibilityNames{ "Always", "Only when not comfortable", "Never" };
			SetNextItemWidth(260.0f);
			EnumCombo("Show", s.visibility, visibilityNames);
			if (s.visibility == Visibility::kNotComfortable) {
				SliderFloat(Id("Hide after").c_str(), &s.hideDelay, 0.0f, 30.0f, seconds);
				Hint("How long you have to be warm or comfortable (and not getting colder) before the gauge fades out");
			}
			Checkbox(Id("Fade with the HUD").c_str(), &s.followHud);
			Hint("Follows the HUD's opacity, e.g. Immersive HUD hiding it. Immersive HUD SKSE has an entry for each group of gauges in its menu: TemperatureGauge, TemperatureGaugeNeeds, TemperatureGaugeStress");
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
			Hint("Part that is red. The green comfortable zone gets the rest. The supported survival mods have no overheating, so nothing reaches it yet");
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

		void RenderNeedsStatus()
		{
			const auto names = Needs::SourceNames();
			const auto loaded = Needs::LoadedSources();
			const auto readings = Needs::Current();
			static constexpr std::array<const char*, kNeedCount> kNeedNames{ "Hunger", "Thirst", "Fatigue", "Inebriation", "Weight", "Vitality" };
			for (std::size_t i = 0; i < kNeedCount; ++i) {
				if (readings[i]) {
					Text("%s", std::format("{}: {} ({}, {:.0f}%)", T(kNeedNames[i]), T(readings[i]->stage.c_str()), names[i], readings[i]->fill * 100.0f).c_str());
				} else if (i >= static_cast<std::size_t>(Need::kInebriation)) {
					continue;  // only some mods have them (Realistic Needs and Diseases, Last Seed); no line without them
				} else {
					TextDisabled("%s", std::format("{}: {}", T(kNeedNames[i]), T("not tracked by a running survival mod")).c_str());
				}
			}
			TextDisabled("%s", F("Supported here: {}", loaded.empty() ? T("none") : loaded.c_str()).c_str());
		}

		// Style, place and look of a group of meters (the needs gauges, or the stress gauge with a_stress)
		void MeterLayout(Settings& s, NeedsSettings& n, bool a_stress)
		{
			const auto pixels = T("%.0f px");
			static constexpr std::array needStyleNames{ "Rings", "Bars", "Arcs" };
			static constexpr std::array stressStyleNames{ "Ring", "Bar", "Arc" };
			SetNextItemWidth(260.0f);
			if (a_stress) {
				EnumCombo("Style", n.style, stressStyleNames);
			} else {
				EnumCombo("Style", n.style, needStyleNames);
			}
			if (n.style == NeedStyle::kArc) {
				if (!a_stress) {
					if (Button(Id("Put them on the temperature gauge's circle").c_str())) {
						if (!PlaceNeedsOnTemperatureCircle(s)) {
							arcHint = true;
						}
					}
					Hint("Same circle as the bent temperature gauge (e.g. around a minimap), on the other side of it");
					if (arcHint) {
						TextDisabled("%s", T("The temperature gauge is straight; give it a curve first."));
					}
				}
				SliderFloat(Id("Center X").c_str(), &n.posX, 0.0f, 1.0f, "%.3f");
				SliderFloat(Id("Center Y").c_str(), &n.posY, 0.0f, 1.0f, "%.3f");
				SliderFloat(Id("Radius").c_str(), &n.arcRadius, 10.0f, 600.0f, pixels);
				SliderFloat(Id("Start").c_str(), &n.arcStart, -180.0f, 360.0f, "%.0f\xC2\xB0");
				Hint(a_stress ? "Where the arc begins: degrees clockwise from 12 o'clock" : "Where the first arc begins: degrees clockwise from 12 o'clock");
				SliderFloat(Id("Arc length").c_str(), &n.arcSpan, -180.0f, 180.0f, "%.0f\xC2\xB0");
				Hint(a_stress ? "Degrees the arc covers; below 0 it goes counterclockwise" : "Degrees each arc covers; below 0 they go counterclockwise");
				if (!a_stress) {
					SliderFloat(Id("Gap").c_str(), &n.arcGap, 0.0f, 45.0f, "%.0f\xC2\xB0");
					Hint("0 = one line, the needs end to end");
					Checkbox(Id("Other order").c_str(), &n.arcReverse);
					SameLine();
					Checkbox(Id("Full end at the other end").c_str(), &n.arcFlip);
				}
				SliderFloat(Id("Thickness", "needsThickness").c_str(), &n.thickness, 1.0f, 20.0f, T("%.1f px"));
			} else {
				if (!a_stress) {
					Checkbox(Id("Stacked (a column)").c_str(), &n.column);
				}
				SliderFloat(Id("Position X", "needsPosX").c_str(), &n.posX, 0.0f, 1.0f, "%.3f");
				SliderFloat(Id("Position Y", "needsPosY").c_str(), &n.posY, 0.0f, 1.0f, "%.3f");
				SliderFloat(Id("Size", "needsSize").c_str(), &n.size, 8.0f, 120.0f, pixels);
				if (!a_stress) {
					SliderFloat(Id("Spacing").c_str(), &n.spacing, -10.0f, 100.0f, pixels);
				}
				SliderFloat(Id("Thickness", "needsThickness").c_str(), &n.thickness, 1.0f, 20.0f, T("%.1f px"));
			}
			if (n.style == NeedStyle::kBar) {
				SliderFloat(Id("Bar length").c_str(), &n.barLength, 10.0f, 400.0f, pixels);
				SliderFloat(Id("Curve", "needsCurve").c_str(), &n.barCurve, -180.0f, 180.0f, "%.0f\xC2\xB0");
				Hint("0 = a straight bar; below 0 it bends the other way");
				if (!a_stress) {
					Checkbox(Id("Full end at the other end").c_str(), &n.arcFlip);
				}
			}
			if (a_stress) {
				Checkbox(Id(n.style == NeedStyle::kRing ? "Fill up with stress" : "Calm end at the other end", "stressFlip").c_str(), &n.arcFlip);
				Hint(n.style == NeedStyle::kRing ? "Off: the ring is full while calm and empties as the stress grows" :
				                                   "Off: the calm end on the right (bars) or at the arc's start");
			}
			if (n.style != NeedStyle::kRing) {
				static constexpr std::array needMarkerNames{ "Like the temperature gauge", "Arrow", "Line" };
				SetNextItemWidth(260.0f);
				EnumCombo("Marker", n.marker, needMarkerNames);
				Checkbox(Id("Fade the ends", "needsFadeEnds").c_str(), &n.fadeEnds);
				SameLine();
			}
			if (n.style != NeedStyle::kArc) {  // arcs have no icons, their colors tell them apart
				Checkbox(Id(a_stress ? "Icon" : "Icons").c_str(), &n.icons);
				SameLine();
			}
			Checkbox(Id("Marks between the stages").c_str(), &n.ticks);
			SameLine();
			Checkbox(Id("Pulse at the worst stages").c_str(), &n.pulseDanger);
			Checkbox(Id("Show the stage name", "needsShowLabel").c_str(), &n.showLabel);
			if (n.showLabel) {
				SliderFloat(Id("Text size", "needsTextSize").c_str(), &n.labelSize, 8.0f, 40.0f, pixels);
			}
		}

		void MeterColors(NeedsSettings& n, bool a_stress)
		{
			const bool byNeed = n.style != NeedStyle::kRing && n.colorByNeed;
			if (n.style != NeedStyle::kRing) {
				Checkbox(Id(a_stress ? "One color" : "A color per need").c_str(), &n.colorByNeed);
				Hint(a_stress ? "Off: colored by stage like the temperature gauge's zones (its green and red shades, amber penalties)" :
				                "Off: colored by stage like the temperature gauge's zones (its green and red shades, amber penalties, gold bonus)");
			}
			if (byNeed) {
				if (a_stress) {
					Color("Stress", "stressColor", n.needColors[0]);
				} else {
					Color("Hunger", "needColorHunger", n.needColors[0]);
					SameLine();
					Color("Thirst", "needColorThirst", n.needColors[1]);
					SameLine();
					Color("Fatigue", "needColorFatigue", n.needColors[2]);
					SameLine();
					Color("Inebriation", "needColorInebriation", n.needColors[3]);
					SameLine();
					Color("Weight", "needColorWeight", n.needColors[4]);
					SameLine();
					Color("Vitality", "needColorVitality", n.needColors[5]);
				}
				SameLine();
			} else {
				if (n.style == NeedStyle::kRing) {
					Color(a_stress ? "Calm" : "Fine", "needFine", n.fine);
					SameLine();
				}
				Color("Penalties", "needWarning", n.warning);
				SameLine();
				if (n.style == NeedStyle::kRing) {
					Color("Worst stages", "needDanger", n.danger);
					SameLine();
				}
				if (!a_stress) {
					Color("Bonus", "needBonus", n.bonus);
					Hint("Well Fed, Well Rested and the like");
				}
			}
			if (n.style == NeedStyle::kRing) {
				Color("Background", "needTrack", n.track);
				SameLine();
			}
			if (n.style != NeedStyle::kArc) {
				Color(a_stress ? "Icon" : "Icons", "needIcon", n.icon);
			}
			if (n.style != NeedStyle::kRing) {
				TextDisabled("%s", T("The marker's color and size, the outline and the round ends come from the Settings page."));
			}
		}

		void __stdcall RenderNeeds()
		{
			std::scoped_lock lock(Config::Lock());
			auto&            s = Config::Get();
			auto&            n = s.needs;

			RenderNeedsStatus();
			TextWrapped("%s", T("Gauges for hunger, thirst and fatigue next to the temperature gauge. Survival Mode has hunger and fatigue, SunHelm all three; Frostfall has none of its own, so with Frostfall they come from the needs mod you run beside it."));

			SeparatorText(T("General"));
			Checkbox(Id("Show hunger, thirst and fatigue gauges").c_str(), &n.enabled);
			Hint("While on, the survival mod's own hunger, thirst and fatigue icons are hidden (SunHelm, Realistic Needs and Diseases)");
			if (!n.enabled) {
				return;
			}
			Checkbox(Id("Hunger").c_str(), &n.show[0]);
			SameLine();
			Checkbox(Id("Thirst").c_str(), &n.show[1]);
			SameLine();
			Checkbox(Id("Fatigue").c_str(), &n.show[2]);
			SameLine();
			Checkbox(Id("Inebriation").c_str(), &n.show[3]);
			SameLine();
			Checkbox(Id("Weight").c_str(), &n.show[4]);
			SameLine();
			Checkbox(Id("Vitality").c_str(), &n.show[5]);
			Hint("Inebriation: Realistic Needs and Diseases, Last Seed. Weight: Realistic Needs and Diseases. Vitality: Last Seed.");
			static constexpr std::array needsVisibilityNames{ "Always", "Only when not comfortable" };
			SetNextItemWidth(260.0f);
			EnumCombo("Show", n.visibility, needsVisibilityNames);
			if (n.visibility == Visibility::kNotComfortable) {
				SliderFloat(Id("Hide after", "needsHideAfter").c_str(), &n.hideDelay, 0.0f, 30.0f, T("%.1f s"));
				Hint("Each gauge fades out once its need has had no penalties for this long");
			}
			TextDisabled("%s", T("Opacity, fading with the HUD, smoothing, outline and scale are shared with the temperature gauge."));

			SeparatorText(T("Layout"));
			MeterLayout(s, n, false);

			SeparatorText(T("Colors"));
			MeterColors(n, false);

			SeparatorText(T("Preview"));
			Checkbox(Id("Try made up values").c_str(), &preview.needs);
			if (preview.needs) {
				SameLine();
				Checkbox(Id("Sweep", "needsSweep").c_str(), &preview.needsSweep);
			}

			Spacing();
			Separator();
			if (Button(Id("Reset the needs gauges to the defaults").c_str())) {
				Config::ResetNeeds();
			}
		}

		void RenderStressStatus()
		{
			const auto r = Stress::Current();
			const auto name = Stress::SourceName();
			if (r.stress) {
				Text("%s", F("Stress: {} ({:.0f} of {:.0f})", T(r.stress->stage.c_str()), r.stress->value, r.stress->max).c_str());
			} else {
				TextDisabled("%s", F("Stress: {}", T("off in the mod")).c_str());
			}
			if (!r.fears) {
				TextDisabled("%s", F("Phobias: {}", T("off in the mod")).c_str());
			} else {
				std::string phobias;
				for (const auto& phobia : r.phobias) {
					phobias += (phobias.empty() ? "" : ", ") + phobia;
				}
				Text("%s", F("Phobias: {}", phobias.empty() ? T("none") : phobias.c_str()).c_str());
				for (const auto& fear : r.afraid) {
					Text("%s", F("Afraid now: {}", fear).c_str());
				}
			}
			TextDisabled("%s", F("Reading from: {}", name.empty() ? T("none") : name.c_str()).c_str());
		}

		void __stdcall RenderStress()
		{
			std::scoped_lock lock(Config::Lock());
			auto&            s = Config::Get();
			auto&            n = s.stress;
			auto&            f = s.fear;
			const auto       pixels = T("%.0f px");

			RenderStressStatus();
			TextWrapped("%s", T("A gauge for Stress and Fear's stress (calm, minor .. critical combat stress) and an eye with the phobia's name while you face what you fear."));

			SeparatorText(T("Stress gauge"));
			Checkbox(Id("Show the stress gauge").c_str(), &n.enabled);
			if (n.enabled) {
				static constexpr std::array stressVisibilityNames{ "Always", "Only when stressed" };
				SetNextItemWidth(260.0f);
				EnumCombo("Show", n.visibility, stressVisibilityNames);
				if (n.visibility == Visibility::kNotComfortable) {
					SliderFloat(Id("Hide after", "needsHideAfter").c_str(), &n.hideDelay, 0.0f, 30.0f, T("%.1f s"));
					Hint("The gauge fades out once you have had no stress for this long");
				}
				MeterLayout(s, n, true);
				SeparatorText(T("Stress colors"));
				MeterColors(n, true);
			}

			SeparatorText(T("Fear"));
			Checkbox(Id("Show the fear indicator").c_str(), &f.enabled);
			Hint("While one of your phobias works: in a fight with what you fear");
			if (f.enabled) {
				SliderFloat(Id("Position X", "fearPosX").c_str(), &f.posX, 0.0f, 1.0f, "%.3f");
				SliderFloat(Id("Position Y", "fearPosY").c_str(), &f.posY, 0.0f, 1.0f, "%.3f");
				Checkbox(Id("Eye", "fearIcon").c_str(), &f.icon);
				SameLine();
				Checkbox(Id("Name of the fear").c_str(), &f.showName);
				SameLine();
				Checkbox(Id("Pulse", "fearPulse").c_str(), &f.pulse);
				if (f.icon) {
					SliderFloat(Id("Size", "fearSize").c_str(), &f.size, 8.0f, 120.0f, pixels);
				}
				if (f.showName) {
					SliderFloat(Id("Text size", "fearTextSize").c_str(), &f.labelSize, 8.0f, 40.0f, pixels);
				}
				Checkbox(Id("Show your phobias when not afraid").c_str(), &f.showPhobias);
				Hint("Faint, under the eye");
				Color("Eye", "fearColor", f.color);
			}
			TextDisabled("%s", T("Opacity, fading with the HUD, smoothing, outline and scale are shared with the temperature gauge."));

			SeparatorText(T("Preview"));
			Checkbox(Id("Try made up values", "stressPreview").c_str(), &preview.stress);
			if (preview.stress) {
				SameLine();
				Checkbox(Id("Sweep", "stressSweep").c_str(), &preview.stressSweep);
			}

			Spacing();
			Separator();
			if (Button(Id("Reset the stress gauge and the fear indicator to the defaults").c_str())) {
				Config::ResetStress();
			}
		}

		void __stdcall RenderLanguages();

		// The sidebar pages: English name (the translation key), page, name it has in the framework now
		struct SidebarPage
		{
			const char*                              english;
			SKSEMenuFramework::Model::RenderFunction render;
			std::string                              shown;
		};
		std::vector<SidebarPage> pages;  // filled once in Register (Stress and Fear only with a stress mod)
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
		pages = { { "Settings", RenderSettings, {} }, { "Needs", RenderNeeds, {} } };
		if (Stress::Available()) {
			pages.push_back({ "Stress and Fear", RenderStress, {} });
		}
		pages.push_back({ "Languages", RenderLanguages, {} });
		for (auto& page : pages) {
			page.shown = T(page.english);
			SKSEMenuFramework::AddSectionItem(page.shown, page.render);
		}
		SKSEMenuFramework::AddHudElement(RenderHud);
		SKSEMenuFramework::AddEvent(OnFrameworkEvent, 0.0f);
		return true;
	}
}
