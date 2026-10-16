#include "Needs.h"

#include "Defs.h"

// Survival mods' needs described by json files (Data/SKSE/Plugins/TemperatureGauge/Needs/*.json):
//
// {
//   "name": "SunHelm Survival",
//   "plugin": "SunHelmSurvival.esp",          forms are local IDs in this plugin unless a value names its own "plugin"
//   "replaces": "Survival Mode",              optional, definitions (by "name") left out while this one is loaded
//   "priority": 10,                           with several mods running a need, the highest priority is shown
//   "enabled": { "global": "0x02EB63" },      the mod runs (left out: always); a list = all of them
//   "hunger": {                               also "thirst" and "fatigue"; a need left out isn't in the mod
//     "enabled": { "questRunning": "0x008EE8" },   this need runs (left out: always)
//     "value": { "global": "0x00EAAE" },      0 = fed / quenched / rested .. "max" = the worst
//     "max": 360,                             or a value; left out: 1.25 x the last stage
//     "stages": [40, 80, 140, 240, 360],      value where stage 1, 2, ... begins (numbers or values, e.g. globals)
//     "stageNames": [ { "spell": "0x001824", "fallback": "Well Fed" }, ... ],   stage 0 .. N
//     "comfortableStages": 2,                 stages 0 .. n-1 have no penalties
//     "dangerStage": 4,                       from this stage on the gauge pulses (default: the second to last)
//     "bonusStages": 1,                       stages 0 .. n-1 are a bonus (Well Fed), shown in the bonus color
//     "bonus": { "when": { ... }, "name": { "spell": "...", "fallback": "Well Fed" } }
//                                             or a bonus outside the stages (Survival Mode's Well Fed / Well Rested)
//   },
//   "hide": { "hudPaths": [ "_root..." ], "skyuiWidget": { "quest": "0x12BDC1", "script": "_SHWidgetScript" } }
// }
//
// Values, conditions and stage names: see Defs.h.
namespace Needs
{
	namespace
	{
		using namespace Defs;

		constexpr auto                      kFolder = "Data/SKSE/Plugins/TemperatureGauge/Needs";
		constexpr std::array<const char*, kNeedCount> kKeys{ "hunger", "thirst", "fatigue" };

		struct NeedDef
		{
			std::vector<Value>       enabled;
			Value                    value;
			Value                    max;
			std::vector<Value>       stages;      // start of stage 1 .. N
			std::vector<std::string> stageNames;  // stage 0 .. N
			int                      comfortableStages{ 1 };
			int                      dangerStage{ 1 };
			int                      bonusStages{ 0 };
			Value                    bonusWhen;
			std::string              bonusName;

			bool On() const { return AllHold(enabled); }

			float Max(const std::vector<float>& a_stages) const
			{
				const auto v = max.Get();
				return v && *v > 0.0f ? *v : (a_stages.empty() ? 1000.0f : a_stages.back() * 1.25f);
			}
		};

		struct Source
		{
			std::string                                name;
			int                                        priority{ 0 };
			std::vector<std::string>                   replaces;
			std::vector<Value>                         enabled;
			std::array<std::optional<NeedDef>, kNeedCount> needs;
			std::vector<std::string>                   hudPaths;
			Widget                                     widget;

			bool On() const { return AllHold(enabled); }
		};

		std::vector<Source> sources;  // highest priority first; filled once, then only read
		std::mutex          lock;
		NeedReadings                        current;
		std::array<std::string, kNeedCount> currentNames;
		std::vector<std::string>            iconPaths;

		NeedDef LoadNeed(const json& a_json, const std::string& a_plugin)
		{
			NeedDef n;
			n.enabled = ParseConditions(a_json, "enabled", a_plugin);
			n.value = ParseValue(a_json.at("value"), a_plugin);
			if (a_json.contains("max")) {
				n.max = ParseValue(a_json["max"], a_plugin);
			}
			n.stages = ParseStages(a_json, a_plugin);
			for (const auto& name : a_json.value("stageNames", json::array())) {
				n.stageNames.push_back(StageName(name, a_plugin));
			}
			n.stageNames.resize(n.stages.size() + 1);
			n.comfortableStages = a_json.value("comfortableStages", 1);
			n.dangerStage = a_json.value("dangerStage", std::max(static_cast<int>(n.stages.size()) - 1, 1));
			n.bonusStages = a_json.value("bonusStages", 0);
			if (a_json.contains("bonus")) {
				const auto& b = a_json["bonus"];
				n.bonusWhen = ParseValue(b.at("when"), a_plugin);
				n.bonusName = b.contains("name") ? StageName(b["name"], a_plugin) : std::string{};
			}
			return n;
		}

		std::optional<Source> Load(const std::filesystem::path& a_file)
		{
			std::ifstream file{ a_file };
			const auto    j = json::parse(file);
			Source        s;
			s.name = j.value("name", a_file.stem().string());
			const auto plugin = j.at("plugin").get<std::string>();
			if (!RE::TESDataHandler::GetSingleton()->LookupModByName(plugin)) {
				logs::info("{} needs: {} isn't loaded, skipped", s.name, plugin);
				return std::nullopt;
			}
			logs::info("{} needs ({})", s.name, a_file.filename().string());
			s.priority = j.value("priority", 0);
			s.replaces = ParseReplaces(j);
			s.enabled = ParseConditions(j, "enabled", plugin);
			for (std::size_t i = 0; i < kNeedCount; ++i) {
				if (!j.contains(kKeys[i])) {
					continue;
				}
				auto need = LoadNeed(j[kKeys[i]], plugin);
				if (!need.value.Valid()) {
					logs::warn("  {}: no value, left out", kKeys[i]);
					continue;
				}
				std::string names;
				for (const auto& n : need.stageNames) {
					names += (names.empty() ? "" : ", ") + n;
				}
				logs::info("  {}: {}{}", kKeys[i], names, need.bonusName.empty() ? "" : " (bonus: " + need.bonusName + ")");
				s.needs[i] = std::move(need);
			}
			if (j.contains("hide")) {
				const auto& h = j["hide"];
				s.hudPaths = h.value("hudPaths", std::vector<std::string>{});
				if (h.contains("skyuiWidget")) {
					s.widget = ParseWidget(h["skyuiWidget"], plugin);
				}
			}
			return s;
		}

		NeedReading Read(const NeedDef& a_need)
		{
			const auto  stages = GetStages(a_need.stages);
			const float max = a_need.Max(stages);
			const float value = std::clamp(a_need.value.Get().value_or(0.0f), 0.0f, max);

			// the gauge: one even share per stage, from 0 to max (a last stage that starts at max is only its end)
			std::vector<float> bounds{ 0.0f };
			for (const float stage : stages) {
				if (stage > bounds.back() && stage < max) {
					bounds.push_back(stage);
				}
			}
			bounds.push_back(max);
			const int segments = static_cast<int>(bounds.size()) - 1;
			float     used = static_cast<float>(segments);
			for (int i = 0; i < segments; ++i) {
				if (value < bounds[i + 1]) {
					used = i + (value - bounds[i]) / std::max(bounds[i + 1] - bounds[i], 1e-3f);
					break;
				}
			}

			int stage = 0;
			while (stage < static_cast<int>(stages.size()) && value >= stages[stage]) {
				++stage;
			}
			const bool bonusNow = a_need.bonusWhen.Valid() && a_need.bonusWhen.Holds();

			NeedReading r;
			r.segments = std::min(segments, kMaxNeedSegments);
			for (int j = 0; j < r.segments; ++j) {
				// the stage the segment starts in
				int k = 0;
				while (k < static_cast<int>(stages.size()) && bounds[j] >= stages[k]) {
					++k;
				}
				r.zones[j] = k < a_need.bonusStages ? NeedZone::kBonus :
				             k < a_need.comfortableStages ? NeedZone::kFine :
				             k >= a_need.dangerStage ? NeedZone::kDanger :
				                                       NeedZone::kPenalty;
			}
			r.fill = bonusNow ? 1.0f : std::clamp(1.0f - used / segments, 0.0f, 1.0f);
			r.bonus = bonusNow || stage < a_need.bonusStages;
			r.comfortable = bonusNow || stage < a_need.comfortableStages;
			r.danger = !bonusNow && stage >= a_need.dangerStage;
			r.stage = bonusNow && !a_need.bonusName.empty() ? a_need.bonusName : a_need.stageNames[stage];
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
		DropReplaced(sources);
		std::ranges::stable_sort(sources, std::greater{}, &Source::priority);
		logs::info("{} survival mod(s) with needs supported", sources.size());
	}

	void Sample()
	{
		NeedReadings                        readings;
		std::array<std::string, kNeedCount> names;
		std::vector<std::string>            paths;
		for (auto& source : sources) {
			if (source.widget.Valid()) {
				source.widget.Refresh(source.name);
			}
			paths.insert(paths.end(), source.hudPaths.begin(), source.hudPaths.end());
			if (!source.widget.root.empty()) {
				paths.push_back(source.widget.root);
			}
			if (!source.On()) {
				continue;
			}
			for (std::size_t i = 0; i < kNeedCount; ++i) {
				if (!readings[i] && source.needs[i] && source.needs[i]->On()) {
					readings[i] = Read(*source.needs[i]);
					names[i] = source.name;
				}
			}
		}
		std::scoped_lock l(lock);
		current = std::move(readings);
		currentNames = std::move(names);
		iconPaths = std::move(paths);
	}

	NeedReadings Current()
	{
		std::scoped_lock l(lock);
		return current;
	}

	std::array<std::string, kNeedCount> SourceNames()
	{
		std::scoped_lock l(lock);
		return currentNames;
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
