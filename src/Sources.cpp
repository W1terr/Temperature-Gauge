#include "Temperature.h"

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
// A value is a number, { "global": "<id>" } or { "script": { "quest": "<id>", "script": "<class>", "variable": "<name>" } }
// (auto properties: "::<name>_var"); any of them can add "plugin". As a condition (enabled, nearHeat) it holds when
// > 0, or >= "atLeast" if given (Frostfall's switches are 1 = off, 2 = on).
namespace Temperature
{
	namespace
	{
		constexpr auto kFolder = "Data/SKSE/Plugins/TemperatureGauge/Sources";

		RE::FormID ParseId(const json& a_value)
		{
			if (a_value.is_number_unsigned()) {
				return a_value.get<RE::FormID>();
			}
			return static_cast<RE::FormID>(std::stoul(a_value.get<std::string>(), nullptr, 16));
		}

		template <class T>
		T* LookupForm(const json& a_id, const std::string& a_plugin)
		{
			const auto form = RE::TESDataHandler::GetSingleton()->LookupForm<T>(ParseId(a_id) & 0xFFFFFF, a_plugin);
			if (!form) {
				logs::warn("  {} {} not found in {}", T::FORMTYPE, a_id.dump(), a_plugin);
			}
			return form;
		}

		RE::BSTSmartPointer<RE::BSScript::Object> ScriptObject(RE::TESQuest* a_quest, const std::string& a_class)
		{
			RE::BSTSmartPointer<RE::BSScript::Object> object;
			const auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			const auto policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
			if (!a_quest || !policy) {
				return object;
			}
			const auto handle = policy->GetHandleForObject(RE::TESQuest::FORMTYPE, a_quest);
			if (handle != policy->EmptyHandle()) {
				vm->FindBoundObject(handle, a_class.c_str(), object);
			}
			return object;
		}

		struct Value
		{
			std::optional<float> constant;
			RE::TESGlobal*       global{ nullptr };
			RE::TESQuest*        quest{ nullptr };
			std::string          script;
			RE::BSFixedString    variable;
			std::optional<float> atLeast;  // as a condition

			bool Valid() const { return constant || global || quest; }

			// game thread
			std::optional<float> Get() const
			{
				if (constant) {
					return constant;
				}
				if (global) {
					return global->value;
				}
				if (const auto object = ScriptObject(quest, script)) {
					if (const auto v = object->GetVariable(variable)) {
						if (v->IsInt()) {
							return static_cast<float>(v->GetSInt());
						}
						if (v->IsFloat()) {
							return v->GetFloat();
						}
					}
				}
				return std::nullopt;
			}

			bool Holds() const
			{
				const auto v = Get();
				return v && (atLeast ? *v >= *atLeast : *v > 0.0f);
			}
		};

		Value ParseValue(const json& a_json, const std::string& a_plugin)
		{
			Value value;
			if (a_json.is_number()) {
				value.constant = a_json.get<float>();
				return value;
			}
			const auto plugin = a_json.value("plugin", a_plugin);
			if (a_json.contains("atLeast")) {
				value.atLeast = a_json["atLeast"].get<float>();
			}
			if (a_json.contains("global")) {
				value.global = LookupForm<RE::TESGlobal>(a_json["global"], plugin);
			} else if (a_json.contains("script")) {
				const auto& s = a_json["script"];
				value.quest = LookupForm<RE::TESQuest>(s.at("quest"), s.value("plugin", plugin));
				value.script = s.at("script").get<std::string>();
				value.variable = s.at("variable").get<std::string>();
			}
			return value;
		}

		// " Cold - Chilly" / "Cold: Chilly" -> "Chilly" (the stage spells' names, in the game's language)
		std::string StageName(const json& a_json, const std::string& a_plugin)
		{
			if (a_json.is_string()) {
				return a_json.get<std::string>();
			}
			const auto  fallback = a_json.value("fallback", ""s);
			std::string name;
			if (a_json.contains("spell")) {
				if (const auto spell = LookupForm<RE::SpellItem>(a_json["spell"], a_json.value("plugin", a_plugin))) {
					name = spell->GetFullName();
				}
			}
			if (const auto cut = name.find_last_of("-:"); cut != std::string::npos) {
				name = name.substr(cut + 1);
			}
			const auto first = name.find_first_not_of(' ');
			const auto last = name.find_last_not_of(' ');
			name = first == std::string::npos ? std::string{} : name.substr(first, last - first + 1);
			return name.empty() ? fallback : name;
		}

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
			RE::TESQuest*            widgetQuest{ nullptr };
			std::string              widgetScript;
			std::string              widgetRoot;  // read from the SkyUI widget script (SkyUI may move it after a load)

			bool On() const
			{
				return std::ranges::all_of(enabled, [](const Value& a_value) { return a_value.Holds(); });
			}

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
			if (j.contains("enabled")) {
				const auto& e = j["enabled"];
				for (const auto& v : e.is_array() ? e : json::array({ e })) {
					s.enabled.push_back(ParseValue(v, plugin));
				}
			}
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
					const auto& w = h["skyuiWidget"];
					s.widgetQuest = LookupForm<RE::TESQuest>(w.at("quest"), w.value("plugin", plugin));
					s.widgetScript = w.at("script").get<std::string>();
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
			if (source.widgetQuest) {
				// SKI_WidgetBase keeps its HUD path ("_root.WidgetContainer.<id>.widget") once SkyUI loaded it
				if (const auto object = ScriptObject(source.widgetQuest, source.widgetScript)) {
					static const RE::BSFixedString rootName{ "_widgetRoot" };
					const auto v = object->GetVariable(rootName);
					if (v && v->IsString() && !v->GetString().empty() && v->GetString() != source.widgetRoot) {
						source.widgetRoot = v->GetString();
						logs::info("{}: its widget is {}", source.name, source.widgetRoot);
					}
				}
			}
			paths.insert(paths.end(), source.hudPaths.begin(), source.hudPaths.end());
			if (!source.widgetRoot.empty()) {
				paths.push_back(source.widgetRoot);
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
