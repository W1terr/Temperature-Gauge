#include "Settings.h"

#include "Lang.h"

namespace Config
{
	namespace
	{
		// settings file versions: 2 = 1.1, the Crimson Desert look (1.0 files are ignored); 3 = 1.2, markers Arrow / Line;
		// 4 = 1.5, the needs arcs as one line in orange / blue / brown
		constexpr int kVersion = 4;

		Settings   settings;
		std::mutex lock;

		std::filesystem::path FilePath()
		{
			const auto dir = logs::log_directory();
			return dir ? *dir / "TemperatureGauge.json" : std::filesystem::path{ "Data/SKSE/Plugins/TemperatureGauge.json" };
		}

		// one table for loading and saving: json key, pointer to the member
		template <class Fn>
		void Fields(Settings& a_s, Fn&& a_fn)
		{
			a_fn("enabled", a_s.enabled);
			a_fn("hideVanillaIcon", a_s.hideVanillaIcon);
			a_fn("visibility", a_s.visibility);
			a_fn("hideDelay", a_s.hideDelay);
			a_fn("followHud", a_s.followHud);
			a_fn("opacity", a_s.opacity);
			a_fn("posX", a_s.posX);
			a_fn("posY", a_s.posY);
			a_fn("scale", a_s.scale);
			a_fn("length", a_s.length);
			a_fn("thickness", a_s.thickness);
			a_fn("curve", a_s.curve);
			a_fn("rotation", a_s.rotation);
			a_fn("mirror", a_s.mirror);
			a_fn("flip", a_s.flip);
			a_fn("roundEnds", a_s.roundEnds);
			a_fn("fadeEnds", a_s.fadeEnds);
			a_fn("outline", a_s.outline);
			a_fn("coldZone", a_s.coldZone);
			a_fn("hotZone", a_s.hotZone);
			a_fn("blendZones", a_s.blendZones);
			a_fn("zoneLines", a_s.zoneLines);
			a_fn("marker", a_s.marker);
			a_fn("markerInside", a_s.markerInside);
			a_fn("markerSize", a_s.markerSize);
			a_fn("showTarget", a_s.showTarget);
			a_fn("showTrend", a_s.showTrend);
			a_fn("pulseDanger", a_s.pulseDanger);
			a_fn("showLabel", a_s.showLabel);
			a_fn("labelSize", a_s.labelSize);
			a_fn("smoothing", a_s.smoothing);
			a_fn("coldEnd", a_s.coldEnd);
			a_fn("coldEdge", a_s.coldEdge);
			a_fn("comfortable", a_s.comfortable);
			a_fn("hotEdge", a_s.hotEdge);
			a_fn("hotEnd", a_s.hotEnd);
			a_fn("outlineColor", a_s.outlineColor);
			a_fn("markerColor", a_s.markerColor);
			a_fn("targetColor", a_s.targetColor);
			a_fn("labelColor", a_s.labelColor);

			auto& n = a_s.needs;
			a_fn("needsEnabled", n.enabled);
			a_fn("needsVisibility", n.visibility);
			a_fn("needsHideDelay", n.hideDelay);
			a_fn("needsShow", n.show);
			a_fn("needsStyle", n.style);
			a_fn("needsColumn", n.column);
			a_fn("needsPosX", n.posX);
			a_fn("needsPosY", n.posY);
			a_fn("needsSize", n.size);
			a_fn("needsSpacing", n.spacing);
			a_fn("needsThickness", n.thickness);
			a_fn("needsBarLength", n.barLength);
			a_fn("needsBarCurve", n.barCurve);
			a_fn("needsArcRadius", n.arcRadius);
			a_fn("needsArcStart", n.arcStart);
			a_fn("needsArcSpan", n.arcSpan);
			a_fn("needsArcGap", n.arcGap);
			a_fn("needsArcReverse", n.arcReverse);
			a_fn("needsArcFlip", n.arcFlip);
			a_fn("needsMarker", n.marker);
			a_fn("needsColorByNeed", n.colorByNeed);
			a_fn("needsFadeEnds", n.fadeEnds);
			a_fn("needColors", n.needColors);
			a_fn("needsIcons", n.icons);
			a_fn("needsTicks", n.ticks);
			a_fn("needsPulseDanger", n.pulseDanger);
			a_fn("needsShowLabel", n.showLabel);
			a_fn("needsLabelSize", n.labelSize);
			a_fn("needFine", n.fine);
			a_fn("needWarning", n.warning);
			a_fn("needDanger", n.danger);
			a_fn("needBonus", n.bonus);
			a_fn("needTrack", n.track);
			a_fn("needIcon", n.icon);

			auto& st = a_s.stress;
			a_fn("stressEnabled", st.enabled);
			a_fn("stressVisibility", st.visibility);
			a_fn("stressHideDelay", st.hideDelay);
			a_fn("stressStyle", st.style);
			a_fn("stressPosX", st.posX);
			a_fn("stressPosY", st.posY);
			a_fn("stressSize", st.size);
			a_fn("stressThickness", st.thickness);
			a_fn("stressBarLength", st.barLength);
			a_fn("stressBarCurve", st.barCurve);
			a_fn("stressArcRadius", st.arcRadius);
			a_fn("stressArcStart", st.arcStart);
			a_fn("stressArcSpan", st.arcSpan);
			a_fn("stressFlip", st.arcFlip);
			a_fn("stressMarker", st.marker);
			a_fn("stressOneColor", st.colorByNeed);
			a_fn("stressFadeEnds", st.fadeEnds);
			a_fn("stressColor", st.needColors[0]);
			a_fn("stressIcon", st.icons);
			a_fn("stressTicks", st.ticks);
			a_fn("stressPulseDanger", st.pulseDanger);
			a_fn("stressShowLabel", st.showLabel);
			a_fn("stressLabelSize", st.labelSize);
			a_fn("stressFine", st.fine);
			a_fn("stressWarning", st.warning);
			a_fn("stressDanger", st.danger);
			a_fn("stressTrack", st.track);
			a_fn("stressIconColor", st.icon);

			auto& f = a_s.fear;
			a_fn("fearEnabled", f.enabled);
			a_fn("fearPosX", f.posX);
			a_fn("fearPosY", f.posY);
			a_fn("fearSize", f.size);
			a_fn("fearIcon", f.icon);
			a_fn("fearShowName", f.showName);
			a_fn("fearLabelSize", f.labelSize);
			a_fn("fearPulse", f.pulse);
			a_fn("fearShowPhobias", f.showPhobias);
			a_fn("fearColor", f.color);
		}

		void ClampMeter(NeedsSettings& n)
		{
			n.visibility = static_cast<Visibility>(std::clamp(static_cast<int>(n.visibility), 0, 1));
			n.style = static_cast<NeedStyle>(std::clamp(static_cast<int>(n.style), 0, 2));
			n.marker = static_cast<NeedMarker>(std::clamp(static_cast<int>(n.marker), 0, 2));
			n.arcRadius = std::clamp(n.arcRadius, 10.0f, 1000.0f);
			n.arcStart = std::clamp(n.arcStart, -360.0f, 360.0f);
			n.arcSpan = std::clamp(n.arcSpan, -360.0f, 360.0f);
			n.arcGap = std::clamp(n.arcGap, 0.0f, 90.0f);
			n.hideDelay = std::clamp(n.hideDelay, 0.0f, 60.0f);
			n.posX = std::clamp(n.posX, 0.0f, 1.0f);
			n.posY = std::clamp(n.posY, 0.0f, 1.0f);
			n.size = std::clamp(n.size, 8.0f, 200.0f);
			n.spacing = std::clamp(n.spacing, -20.0f, 200.0f);
			n.thickness = std::clamp(n.thickness, 1.0f, 30.0f);
			n.barLength = std::clamp(n.barLength, 10.0f, 600.0f);
			n.barCurve = std::clamp(n.barCurve, -180.0f, 180.0f);
			n.labelSize = std::clamp(n.labelSize, 6.0f, 60.0f);
		}

		template <class T>
		void Read(const json& a_json, const char* a_key, T& a_value)
		{
			const auto it = a_json.find(a_key);
			if (it == a_json.end()) {
				return;
			}
			try {
				if constexpr (std::is_enum_v<T>) {
					a_value = static_cast<T>(it->get<int>());
				} else if constexpr (requires { std::tuple_size<T>::value; typename T::value_type; }) {
					// lists that grew (3 needs before 2.2, now 8): what the file has, the defaults for the rest
					for (std::size_t i = 0; i < std::min(a_value.size(), it->size()); ++i) {
						a_value[i] = (*it)[i].template get<typename T::value_type>();
					}
				} else {
					a_value = it->get<T>();
				}
			} catch (const std::exception& e) {
				logs::warn("Setting {}: {}", a_key, e.what());
			}
		}

		std::string lastSaved;  // what the file has (nothing written while unchanged)

		std::string Serialize(Settings& a_s)
		{
			json j;
			j["version"] = kVersion;
			j["language"] = Lang::Current().id;  // outside the settings: a reset keeps the language
			Fields(a_s, [&](const char* a_key, auto& a_value) {
				if constexpr (std::is_enum_v<std::remove_cvref_t<decltype(a_value)>>) {
					j[a_key] = static_cast<int>(a_value);
				} else {
					j[a_key] = a_value;
				}
			});
			return j.dump(1, '\t');
		}

		void Clamp(Settings& a_s)
		{
			a_s.visibility = static_cast<Visibility>(std::clamp(static_cast<int>(a_s.visibility), 0, 2));
			a_s.marker = static_cast<MarkerStyle>(std::clamp(static_cast<int>(a_s.marker), 0, 1));
			a_s.coldZone = std::clamp(a_s.coldZone, 0.05f, 0.9f);
			a_s.hotZone = std::clamp(a_s.hotZone, 0.0f, 0.95f - a_s.coldZone);
			a_s.hideDelay = std::clamp(a_s.hideDelay, 0.0f, 60.0f);
			a_s.opacity = std::clamp(a_s.opacity, 0.0f, 1.0f);
			a_s.posX = std::clamp(a_s.posX, 0.0f, 1.0f);
			a_s.posY = std::clamp(a_s.posY, 0.0f, 1.0f);
			a_s.scale = std::clamp(a_s.scale, 0.25f, 4.0f);
			a_s.length = std::clamp(a_s.length, 20.0f, 1000.0f);
			a_s.thickness = std::clamp(a_s.thickness, 1.0f, 40.0f);
			a_s.curve = std::clamp(a_s.curve, 0.0f, 300.0f);
			a_s.rotation = std::clamp(a_s.rotation, -180.0f, 180.0f);
			a_s.outline = std::clamp(a_s.outline, 0.0f, 8.0f);
			a_s.markerSize = std::clamp(a_s.markerSize, 0.25f, 4.0f);
			a_s.labelSize = std::clamp(a_s.labelSize, 6.0f, 60.0f);
			a_s.smoothing = std::clamp(a_s.smoothing, 0.0f, 5.0f);

			ClampMeter(a_s.needs);
			ClampMeter(a_s.stress);

			auto& f = a_s.fear;
			f.posX = std::clamp(f.posX, 0.0f, 1.0f);
			f.posY = std::clamp(f.posY, 0.0f, 1.0f);
			f.size = std::clamp(f.size, 8.0f, 200.0f);
			f.labelSize = std::clamp(f.labelSize, 6.0f, 60.0f);
		}
	}

	Settings& Get()
	{
		return settings;
	}

	std::mutex& Lock()
	{
		return lock;
	}

	void Load()
	{
		const auto path = FilePath();
		std::ifstream file{ path };
		if (!file) {
			logs::info("No settings file at {}, using the defaults", path.string());
			return;
		}
		try {
			const auto j = json::parse(file);
			Lang::Set(j.value("language", "english"));
			const int version = j.value("version", 1);
			if (version < 2) {
				logs::info("{} is from version 1.0 (old look), using the new defaults", path.string());
				return;
			}
			Fields(settings, [&](const char* a_key, auto& a_value) { Read(j, a_key, a_value); });
			if (version < 3) {
				// 1.1 had Crimson Desert (badge), Arrow and figure, Arrow, Line
				settings.marker = j.value("marker", 0) == 3 ? MarkerStyle::kLine : MarkerStyle::kArrow;
			}
			if (version < 4) {
				// 1.5 test builds: arcs had gaps and yellow / blue / purple; now one line, orange / blue / brown
				const NeedsSettings defaults;
				settings.needs.arcGap = defaults.arcGap;
				settings.needs.needColors = defaults.needColors;
			}
			Clamp(settings);
			lastSaved = Serialize(settings);
			logs::info("Loaded settings from {}", path.string());
		} catch (const std::exception& e) {
			logs::error("Couldn't read {}: {}", path.string(), e.what());
		}
	}

	void Save()
	{
		const auto text = Serialize(settings);
		if (text == lastSaved) {
			return;  // the framework sends "menu closed" many times
		}
		const auto path = FilePath();
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
		std::ofstream file{ path };
		if (!file) {
			logs::error("Couldn't write {}", path.string());
			return;
		}
		file << text;
		lastSaved = text;
		logs::info("Saved settings to {}", path.string());
	}

	void Reset()
	{
		settings = Settings{};
	}

	void ResetNeeds()
	{
		const bool enabled = settings.needs.enabled;
		settings.needs = NeedsSettings{};
		settings.needs.enabled = enabled;
	}

	void ResetStress()
	{
		const bool stress = settings.stress.enabled;
		const bool fear = settings.fear.enabled;
		settings.stress = StressDefaults();
		settings.stress.enabled = stress;
		settings.fear = FearSettings{};
		settings.fear.enabled = fear;
	}

	void Apply(Preset a_preset)
	{
		const Settings defaults;
		auto&          s = settings;
		s.mirror = false;
		s.flip = false;
		switch (a_preset) {
		case Preset::kHalfCircle:
			s.curve = 180.0f;
			s.rotation = 0.0f;
			s.length = 150.0f;
			s.posX = defaults.posX;
			s.posY = 0.1f;
			s.markerInside = true;
			break;
		case Preset::kVertical:
			s.curve = 0.0f;
			s.rotation = 0.0f;
			s.length = defaults.length;
			s.posX = defaults.posX;
			s.posY = defaults.posY;
			s.markerInside = false;
			break;
		case Preset::kHorizontal:
			// under the compass, hot end on the right
			s.curve = 0.0f;
			s.rotation = 90.0f;
			s.length = 220.0f;
			s.posX = 0.5f;
			s.posY = 0.105f;
			s.markerInside = true;
			break;
		}
	}
}
