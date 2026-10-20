#include "Stress.h"

#include "Defs.h"
#include "Meter.h"

// Stress mods described by json files (Data/SKSE/Plugins/TemperatureGauge/Stress/*.json):
//
// {
//   "name": "Stress and Fear",
//   "plugin": "Stress and Fear.esp",           forms are local IDs in this plugin unless a value names its own "plugin"
//   "replaces": "...",                         optional, definitions (by "name") left out while this one is loaded
//   "priority": 0,                             with several mods running, the highest priority is shown
//   "stress": { ... },                         a meter (see Meter.h): 0 = calm .. max
//   "fearsEnabled": { "global": "0x8A6" },     the fears are on (left out: always); a list = all of them
//   "fears": [ { "spell": "0x817", "effect": "0x816", "name": "..." }, ... ]
//                                              a phobia: the ability the player gets, its magic effect that works while
//                                              the player faces what they fear; "name" (a text or a stage name, see
//                                              Defs.h) is left out for the spell's own name ("Fear of Wolves")
//   "hide": { ... }                            the mod's own widgets, like Needs/*.json
// }
namespace Stress
{
	namespace
	{
		using namespace Defs;

		constexpr auto kFolder = "Data/SKSE/Plugins/TemperatureGauge/Stress";

		struct Fear
		{
			RE::SpellItem*      spell{ nullptr };
			RE::EffectSetting*  effect{ nullptr };
			std::string         name;
		};

		struct Source
		{
			std::string                name;
			int                        priority{ 0 };
			std::vector<std::string>   replaces;
			std::optional<Meter::Def>  stress;
			std::vector<Value>         fearsEnabled;
			std::vector<Fear>          fears;
			Hidden                     hidden;
		};

		std::vector<Source>      sources;  // highest priority first; filled once, then only read
		std::mutex               lock;
		StressReading            current;
		std::string              currentName;
		std::vector<std::string> iconPaths;

		// what the log said last (phobias gained or lost, fears starting and ending)
		std::vector<std::string> loggedPhobias;
		std::vector<std::string> loggedAfraid;

		void LogChanges(const std::vector<std::string>& a_now, std::vector<std::string>& a_logged, const char* a_what)
		{
			if (a_now == a_logged) {
				return;
			}
			std::string list;
			for (const auto& name : a_now) {
				list += (list.empty() ? "" : ", ") + name;
			}
			logs::info("{}: {}", a_what, list.empty() ? "none"s : list);
			a_logged = a_now;
		}

		std::optional<Source> Load(const std::filesystem::path& a_file)
		{
			std::ifstream file{ a_file };
			const auto    j = json::parse(file);
			Source        s;
			s.name = j.value("name", a_file.stem().string());
			const auto plugin = j.at("plugin").get<std::string>();
			if (!RE::TESDataHandler::GetSingleton()->LookupModByName(plugin)) {
				logs::info("{} stress: {} isn't loaded, skipped", s.name, plugin);
				return std::nullopt;
			}
			logs::info("{} stress ({})", s.name, a_file.filename().string());
			s.priority = j.value("priority", 0);
			s.replaces = ParseReplaces(j);
			if (j.contains("stress")) {
				auto stress = Meter::Load(j["stress"], plugin);
				if (stress.value.Valid()) {
					logs::info("  stress: {}", stress.Describe());
					s.stress = std::move(stress);
				} else {
					logs::warn("  stress: no value, left out");
				}
			}
			s.fearsEnabled = ParseConditions(j, "fearsEnabled", plugin);
			for (const auto& f : j.value("fears", json::array())) {
				Fear fear;
				const auto fearPlugin = f.value("plugin", plugin);
				fear.spell = LookupForm<RE::SpellItem>(f.at("spell"), fearPlugin);
				if (f.contains("effect")) {
					fear.effect = LookupForm<RE::EffectSetting>(f["effect"], fearPlugin);
				}
				if (f.contains("name")) {
					fear.name = StageName(f["name"], fearPlugin);
				} else if (fear.spell) {
					fear.name = fear.spell->GetFullName();
				}
				if (fear.spell) {
					s.fears.push_back(std::move(fear));
				}
			}
			if (!s.fears.empty()) {
				logs::info("  {} fears", s.fears.size());
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
		if (ec && ec != std::errc::no_such_file_or_directory) {
			logs::error("Can't read {}: {}", kFolder, ec.message());
		}
		DropReplaced(sources);
		std::ranges::stable_sort(sources, std::greater{}, &Source::priority);
		logs::info("{} stress mod(s) supported", sources.size());
	}

	bool Available()
	{
		return !sources.empty();
	}

	void Sample()
	{
		StressReading            reading;
		std::string              name;
		std::vector<std::string> paths;
		bool                     found = false;
		for (auto& source : sources) {
			source.hidden.Collect(source.name, paths);
			if (found) {
				continue;
			}
			if (source.stress && source.stress->On()) {
				reading.stress = Meter::Read(*source.stress);
				found = true;
			}
			if (!source.fears.empty() && AllHold(source.fearsEnabled)) {
				reading.fears = true;
				found = true;
				for (const auto& fear : source.fears) {
					if (!PlayerHasSpell(fear.spell)) {
						continue;
					}
					reading.phobias.push_back(fear.name);
					if (PlayerEffectActive(fear.effect)) {
						reading.afraid.push_back(fear.name);
					}
				}
			}
			if (found) {
				name = source.name;
			}
		}
		LogChanges(reading.phobias, loggedPhobias, "Phobias");
		LogChanges(reading.afraid, loggedAfraid, "Afraid now");
		std::scoped_lock l(lock);
		current = std::move(reading);
		currentName = std::move(name);
		iconPaths = std::move(paths);
	}

	StressReading Current()
	{
		std::scoped_lock l(lock);
		return current;
	}

	std::string SourceName()
	{
		std::scoped_lock l(lock);
		return currentName;
	}

	std::vector<std::string> IconPaths()
	{
		std::scoped_lock l(lock);
		return iconPaths;
	}
}
