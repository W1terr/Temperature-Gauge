#include "Settings.h"

#include "Lang.h"

namespace Config
{
	namespace
	{
		// settings file versions: 2 = 1.1, the Crimson Desert look (1.0 files are ignored); 3 = 1.2, markers Arrow / Line
		constexpr int kVersion = 3;

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
			if (version < kVersion) {
				// 1.1 had Crimson Desert (badge), Arrow and figure, Arrow, Line
				settings.marker = j.value("marker", 0) == 3 ? MarkerStyle::kLine : MarkerStyle::kArrow;
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
