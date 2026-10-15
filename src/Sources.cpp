#include "Temperature.h"

#include "Defs.h"

// Survival mods described by json files (Data/SKSE/Plugins/TemperatureGauge/Sources/*.json):
//
// {
//   "name": "SunHelm Survival",
//   "plugin": "SunHelmSurvival.esp",          forms are local IDs in this plugin unless a value names its own "plugin"
//   "priority": 10,                           with several mods running, the highest priority is shown
//   "enabled": { "global": "0x79441D" },      the mod's cold system is on (left out: always on); a list = all of them
//   "cold": { "global": "0x6A13C5" },         the cold need: 0 = warm .. "max" = deadly
//   "max": { "global": "0xA08085" },          or a number
//   "stages": [25, 150, 250, 450, 700],       cold where stage 1, 2, ... begins
//   "stageNames": [ { "spell": "0x6E81DA", "fallback": "Warm" }, "Comfortable", ... ],   stage 0 .. N
//   "comfortableStages": 2,                   stages 0 .. n-1 have no penalties (the green zone)
//   "dangerStage": 4,                         from this stage on the marker pulses (default: the second to last)
//   "positions": [ ... ],                     optional, gauge scale at cold 0, each stage start and max
//   "temperatureLevel": { "global": "..." },  the vanilla icon's state: 0 neutral, 1 near heat, 2 warming up,
//                                             3 getting colder, 4 freezing area
//   "nearHeat": { "global": "..." },          without "temperatureLevel": the warming up / cooling down chevrons come
//   "trend": { "steadyWithin": 1, "fastFrom": 40 }   from the target vs the cold (cold units; two chevrons from
//                                             "fastFrom" on when getting colder, near heat when warming up)
//   "target": { "value": { "global": "..." }, "deadlyFrom": 600 }      the cold the area brings you to
//          or { "level": { "script": { ... } }, "levelStages": [1, 4, 7, 10, 13] }
//                                             a cold level; reaching levelStages[i] lets the cold go up to stage i + 1
//   "hide": { "hudPaths": [ "_root..." ], "skyuiWidget": { "quest": "0x84F92D", "script": "_SHColdWidgetScript" } }
// }
//
// Values, conditions (enabled, nearHeat), stage names and the widget: see Defs.h.
namespace Temperature
{
	namespace
	{
		using namespace Defs;

		constexpr auto kFolder = "Data/SKSE/Plugins/TemperatureGauge/Sources";

		struct Source
		{
			std::string              name;
			int                      priority{ 0 };
			std::vector<Value>       enabled;  // all of them hold = on
			Value                    cold;
			Value                    max;
			Value                    temperatureLevel;
			std::vector<float>       stages;     // start of stage 1 .. N
			std::vector<std::string> stageNames; // stage 0 .. N
			int                      comfortableStages{ 2 };
			int                      dangerStage{ 4 };
			std::vector<float>       positions;  // gauge scale at cold 0, each stage start, max

			Value                    nearHeat;
			float                    steadyWithin{ 0.0f };
			float                    fastFrom{ 0.0f };

			Value                    targetValue;
			std::optional<float>     deadlyFrom;
			Value                    targetLevel;
			std::vector<int>         levelStages;

			std::vector<std::string> hudPaths;
			Widget                   widget;

			bool On() const { return AllHold(enabled); }

			float Max() const
			{
				const auto v = max.Get();
				return v && *v > 0.0f ? *v : (stages.empty() ? 1000.0f : stages.back() * 1.25f);
			}

			// cold -> gauge scale, linear between cold 0, each stage start and max
			float Position(float a_cold, float a_max) const
			{
				float from = 0.0f;
				for (std::size_t i = 0; i <= stages.size(); ++i) {
					const float to = i < stages.size() ? stages[i] : a_max;
					if (a_cold <= to) {
						const float t = std::clamp((a_cold - from) / std::max(to - from, 1e-3f), 0.0f, 1.0f);
						return std::lerp(positions[i], positions[i + 1], t);
					}
					from = to;
				}
				return positions.back();
			}
		};

		std::vector<Source> sources;  // highest priority first; filled once, then only read
		std::mutex          lock;
		std::optional<Reading>   current;
		std::string              currentName;
		std::vector<std::string> iconPaths;

		// even bands: the comfortable stages share 0.75 .. 0 (the warmest stays below the hot zone), the others -1 .. 0
		std::vector<float> DefaultPositions(std::size_t a_stages, int a_comfortable)
		{
			const int n = static_cast<int>(a_stages);
			if (n == 0) {
				return { 0.75f, -1.0f };
			}
			const int          c = std::clamp(a_comfortable, 1, n);
			std::vector<float> pos;
			for (int i = 0; i <= n + 1; ++i) {
				pos.push_back(i <= c ? 0.75f * (1.0f - static_cast<float>(i) / c) : -static_cast<float>(i - c) / (n + 1 - c));
			}
			return pos;
		}

		std::optional<Source> Load(const std::filesystem::path& a_file)
		{
			std::ifstream file{ a_file };
			const auto    j = json::parse(file);
			Source        s;
			s.name = j.value("name", a_file.stem().string());
			const auto plugin = j.at("plugin").get<std::string>();
			if (!RE::TESDataHandler::GetSingleton()->LookupModByName(plugin)) {
				logs::info("{}: {} isn't loaded, skipped", s.name, plugin);
				return std::nullopt;
			}
			logs::info("{} ({})", s.name, a_file.filename().string());
			s.priority = j.value("priority", 0);
			s.enabled = ParseConditions(j, "enabled", plugin);
			s.cold = ParseValue(j.at("cold"), plugin);
			if (!s.cold.Valid()) {
				logs::warn("  no cold value, skipped");
				return std::nullopt;
			}
			if (j.contains("max")) {
				s.max = ParseValue(j["max"], plugin);
			}
			if (j.contains("temperatureLevel")) {
				s.temperatureLevel = ParseValue(j["temperatureLevel"], plugin);
			}
			if (j.contains("nearHeat")) {
				s.nearHeat = ParseValue(j["nearHeat"], plugin);
			}
			s.stages = j.value("stages", std::vector<float>{});
			for (const auto& name : j.value("stageNames", json::array())) {
				s.stageNames.push_back(StageName(name, plugin));
			}
			s.stageNames.resize(s.stages.size() + 1);
			s.comfortableStages = j.value("comfortableStages", 2);
			s.dangerStage = j.value("dangerStage", std::max(static_cast<int>(s.stages.size()) - 1, 1));
			const auto trend = j.value("trend", json::object());
			s.steadyWithin = trend.value("steadyWithin", 0.0f);
			s.fastFrom = trend.value("fastFrom", 0.0f);
			s.positions = j.value("positions", std::vector<float>{});
			if (s.positions.size() != s.stages.size() + 2) {
				s.positions = DefaultPositions(s.stages.size(), s.comfortableStages);
			}
			if (j.contains("target")) {
				const auto& t = j["target"];
				if (t.contains("value")) {
					s.targetValue = ParseValue(t["value"], plugin);
					if (t.contains("deadlyFrom")) {
						s.deadlyFrom = t["deadlyFrom"].get<float>();
					}
				} else if (t.contains("level")) {
					s.targetLevel = ParseValue(t["level"], plugin);
					s.levelStages = t.value("levelStages", std::vector<int>{});
				}
			}
			if (j.contains("hide")) {
				const auto& h = j["hide"];
				s.hudPaths = h.value("hudPaths", std::vector<std::string>{});
				if (h.contains("skyuiWidget")) {
					s.widget = ParseWidget(h["skyuiWidget"], plugin);
				}
			}
			std::string names;
			for (const auto& n : s.stageNames) {
				names += (names.empty() ? "" : ", ") + n;
			}
			logs::info("  stages: {}", names);
			return s;
		}

		// the highest stage the area's cold level allows -> the most cold it can give
		float StageCeiling(const Source& a_source, float a_level, float a_max)
		{
			std::size_t stage = 0;
			while (stage < a_source.levelStages.size() && a_level >= a_source.levelStages[stage]) {
				++stage;
			}
			return stage >= a_source.stages.size() ? a_max : a_source.stages[stage] - 1.0f;
		}

		Reading Read(const Source& a_source)
		{
			const float max = a_source.Max();
			const float cold = std::clamp(a_source.cold.Get().value_or(0.0f), 0.0f, max);
			const int   level = static_cast<int>(a_source.temperatureLevel.Get().value_or(0.0f));

			const bool  byLevel = a_source.temperatureLevel.Valid();

			// the cold the body is heading to
			std::optional<float> target;
			if (byLevel && level == 1) {  // near a heat source: warming up all the way
				target = 0.0f;
			} else if (a_source.targetValue.Valid()) {
				if (auto v = a_source.targetValue.Get()) {
					if (a_source.deadlyFrom && *v >= *a_source.deadlyFrom) {
						*v = max;
					}
					target = std::clamp(*v, 0.0f, max);
				}
			} else if (a_source.targetLevel.Valid()) {
				if (const auto v = a_source.targetLevel.Get()) {
					target = StageCeiling(a_source, *v, max);
				}
			}

			Reading r;
			r.body = a_source.Position(cold, max);
			if (target) {
				r.target = a_source.Position(*target, max);
			}
			if (byLevel) {
				constexpr std::array kTrend{ 0, 2, 1, -1, -2 };
				r.trend = level >= 0 && level < 5 ? kTrend[level] : 0;
			} else if (target) {
				const float diff = *target - cold;  // > 0: getting colder
				if (diff > a_source.steadyWithin) {
					r.trend = a_source.fastFrom > 0.0f && diff >= a_source.fastFrom ? -2 : -1;
				} else if (diff < -a_source.steadyWithin) {
					r.trend = a_source.nearHeat.Valid() && a_source.nearHeat.Holds() ? 2 : 1;
				}
			}
			int stage = 0;
			while (stage < static_cast<int>(a_source.stages.size()) && cold >= a_source.stages[stage]) {
				++stage;
			}
			r.stage = a_source.stageNames[stage];
			r.comfortable = stage < a_source.comfortableStages;
			r.danger = stage >= a_source.dangerStage;
			return r;
		}
	}

	void Init()
	{
		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(kFolder, ec)) {
			if (entry.path().extension() != ".json") {
				continue;
			}
			try {
				if (auto source = Load(entry.path())) {
					sources.push_back(std::move(*source));
				}
			} catch (const std::exception& e) {
				logs::error("{}: {}", entry.path().filename().string(), e.what());
			}
		}
		if (ec) {
			logs::error("Can't read {}: {}", kFolder, ec.message());
		}
		std::ranges::stable_sort(sources, std::greater{}, &Source::priority);
		logs::info("{} survival mod(s) supported", sources.size());
	}

	void Sample()
	{
		std::optional<Reading>   reading;
		std::string              name;
		std::vector<std::string> paths;
		for (auto& source : sources) {
			if (source.widget.Valid()) {
				source.widget.Refresh(source.name);
			}
			paths.insert(paths.end(), source.hudPaths.begin(), source.hudPaths.end());
			if (!source.widget.root.empty()) {
				paths.push_back(source.widget.root);
			}
			if (!reading && source.On()) {
				reading = Read(source);
				name = source.name;
			}
		}
		std::scoped_lock l(lock);
		current = std::move(reading);
		currentName = std::move(name);
		iconPaths = std::move(paths);
	}

	std::optional<Reading> Current()
	{
		std::scoped_lock l(lock);
		return current;
	}

	std::string SourceName()
	{
		std::scoped_lock l(lock);
		return currentName;
	}

	std::string LoadedSources()
	{
		std::string names;
		for (const auto& source : sources) {
			names += (names.empty() ? "" : ", ") + source.name;
		}
		return names;
	}

	std::vector<std::string> IconPaths()
	{
		std::scoped_lock l(lock);
		return iconPaths;
	}
}
