#include "Needs.h"

#include "Defs.h"
#include "Meter.h"

// Survival mods' needs described by json files (Data/SKSE/Plugins/TemperatureGauge/Needs/*.json):
//
// {
//   "name": "SunHelm Survival",
//   "plugin": "SunHelmSurvival.esp",          forms are local IDs in this plugin unless a value names its own "plugin"
//   "replaces": "Survival Mode",              optional, definitions (by "name") left out while this one is loaded
//   "priority": 10,                           with several mods running a need, the highest priority is shown
//   "enabled": { "global": "0x02EB63" },      the mod runs (left out: always); a list = all of them
//   "hunger": { ... },                        also "thirst", "fatigue", "inebriation", "weight", "vitality", "dirt" and
//                                             "blood", a meter each (see Meter.h); a need left out isn't in the mod
//   "hide": { "hudPaths": [ "_root..." ], "skyuiWidget": { "quest": "0x12BDC1", "script": "_SHWidgetScript" } }
//                                             the mod's own need icons (skyuiWidget may be a list); "smfHud": its SKSE
//                                             Menu Framework HUD (see SmfHuds.h), hidden while a need of it is shown
// }
//
// Values, conditions and stage names: see Defs.h.
namespace Needs
{
	namespace
	{
		using namespace Defs;

		constexpr auto                                kFolder = "Data/SKSE/Plugins/TemperatureGauge/Needs";
		constexpr std::array<const char*, kNeedCount> kKeys{ "hunger", "thirst", "fatigue", "inebriation", "weight", "vitality", "dirt",
			"blood" };

		struct Source
		{
			std::string                                     name;
			int                                             priority{ 0 };
			std::vector<std::string>                        replaces;
			std::vector<Value>                              enabled;
			std::array<std::optional<Meter::Def>, kNeedCount> needs;
			Hidden                                          hidden;

			bool On() const { return AllHold(enabled); }
		};

		std::vector<Source>                 sources;  // highest priority first; filled once, then only read
		std::mutex                          lock;
		NeedReadings                        current;
		std::array<std::string, kNeedCount>              currentNames;
		std::vector<std::string>                         iconPaths;      // the mods' own (always hidden)
		std::array<std::vector<std::string>, kNeedCount> needIconPaths;  // each need's (hidden while it's shown)
		std::array<std::vector<std::string>, kNeedCount> needHuds;       // the SMF HUDs of the mod each need is read from
		std::string                                      logged;         // the needs read, as the log last said
		std::vector<std::array<int, kNeedCount>>         loggedOn;       // per source and need: last logged on (1) / off (0)

		std::optional<Source> Load(const std::filesystem::path& a_file)
		{
			std::ifstream file{ a_file };
			const auto    j = json::parse(file);
			Source        s;
			s.name = j.value("name", a_file.stem().string());
			const auto plugin = j.at("plugin").get<std::string>();
			if (!PluginLoaded(j, plugin, s.name + " needs")) {
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
				auto need = Meter::Load(j[kKeys[i]], plugin);
				if (!need.value.Valid()) {
					logs::warn("  {}: no value, left out", kKeys[i]);
					continue;
				}
				logs::info("  {}: {}", kKeys[i], need.Describe());
				s.needs[i] = std::move(need);
			}
			s.hidden.Parse(j, plugin);
			return s;
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
		NeedReadings                                     readings;
		std::array<std::string, kNeedCount>              names;
		std::vector<std::string>                         paths;
		std::array<std::vector<std::string>, kNeedCount> needPaths;
		std::array<std::vector<std::string>, kNeedCount> huds;
		std::array<int, kNeedCount>                      unknown;
		unknown.fill(-1);
		loggedOn.resize(sources.size(), unknown);
		for (std::size_t k = 0; k < sources.size(); ++k) {
			auto&      source = sources[k];
			const bool sourceOn = source.On();
			source.hidden.Collect(source.name, paths);
			for (std::size_t i = 0; i < kNeedCount; ++i) {
				if (!source.needs[i]) {
					continue;
				}
				const bool on = sourceOn && source.needs[i]->On();
				if (loggedOn[k][i] != static_cast<int>(on)) {
					logs::info("{} {}: {}", source.name, kKeys[i], on ? "running" : sourceOn ? "off (its \"enabled\" doesn't hold)" : "off (the mod isn't running)");
					loggedOn[k][i] = on;
				}
				if (on && !readings[i]) {
					readings[i] = Meter::Read(*source.needs[i]);
					names[i] = source.name;
					// the mod's own widget for this need, hidden only while our gauge shows it
					source.needs[i]->hidden.Collect(source.name, needPaths[i]);
					huds[i] = source.hidden.smfHuds;
				}
			}
		}
		// which mod each need comes from, in the log when it changes
		std::string now;
		for (std::size_t i = 0; i < kNeedCount; ++i) {
			if (readings[i]) {
				now += std::format("{}{} ({})", now.empty() ? "" : ", ", kKeys[i], names[i]);
			}
		}
		if (now != logged) {
			logs::info("Needs read now: {}", now.empty() ? "none"s : now);
			logged = now;
		}
		std::scoped_lock l(lock);
		current = std::move(readings);
		currentNames = std::move(names);
		iconPaths = std::move(paths);
		needIconPaths = std::move(needPaths);
		needHuds = std::move(huds);
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

	std::vector<std::string> IconPaths(const std::array<bool, kNeedCount>& a_shown)
	{
		std::scoped_lock l(lock);
		auto paths = iconPaths;
		for (std::size_t i = 0; i < kNeedCount; ++i) {
			if (a_shown[i]) {
				paths.insert(paths.end(), needIconPaths[i].begin(), needIconPaths[i].end());
			}
		}
		return paths;
	}

	std::vector<std::string> SmfHuds(const std::array<bool, kNeedCount>& a_shown)
	{
		std::scoped_lock         l(lock);
		std::vector<std::string> huds;
		for (std::size_t i = 0; i < kNeedCount; ++i) {
			if (a_shown[i]) {
				huds.insert(huds.end(), needHuds[i].begin(), needHuds[i].end());
			}
		}
		return huds;
	}
}
