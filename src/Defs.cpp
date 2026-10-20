#include "Defs.h"

namespace Defs
{
	namespace
	{
		RE::FormID ParseId(const json& a_value)
		{
			if (a_value.is_number_unsigned()) {
				return a_value.get<RE::FormID>();
			}
			return static_cast<RE::FormID>(std::stoul(a_value.get<std::string>(), nullptr, 16));
		}

		RE::BGSBaseAlias* FindAlias(RE::TESQuest* a_quest, int a_id)
		{
			RE::BSReadLockGuard guard{ a_quest->aliasAccessLock };
			for (const auto alias : a_quest->aliases) {
				if (alias && static_cast<int>(alias->aliasID) == a_id) {
					return alias;
				}
			}
			return nullptr;
		}

		// straight lines between the points (sorted by x), flat beyond the first and the last
		float Map(const std::vector<std::pair<float, float>>& a_points, float a_x)
		{
			if (a_x <= a_points.front().first) {
				return a_points.front().second;
			}
			for (std::size_t i = 1; i < a_points.size(); ++i) {
				const auto& [x0, y0] = a_points[i - 1];
				const auto& [x1, y1] = a_points[i];
				if (a_x <= x1) {
					return x1 > x0 ? std::lerp(y0, y1, (a_x - x0) / (x1 - x0)) : y1;
				}
			}
			return a_points.back().second;
		}
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

	template RE::TESGlobal*     LookupForm<RE::TESGlobal>(const json&, const std::string&);
	template RE::TESQuest*      LookupForm<RE::TESQuest>(const json&, const std::string&);
	template RE::SpellItem*     LookupForm<RE::SpellItem>(const json&, const std::string&);
	template RE::EffectSetting* LookupForm<RE::EffectSetting>(const json&, const std::string&);

	RE::BSTSmartPointer<RE::BSScript::Object> ScriptObject(RE::TESQuest* a_quest, const std::string& a_class, int a_alias)
	{
		RE::BSTSmartPointer<RE::BSScript::Object> object;
		const auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		const auto policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
		if (!a_quest || !policy) {
			return object;
		}
		RE::VMHandle handle = policy->EmptyHandle();
		if (a_alias < 0) {
			handle = policy->GetHandleForObject(RE::TESQuest::FORMTYPE, a_quest);
		} else if (const auto alias = FindAlias(a_quest, a_alias)) {
			handle = policy->GetHandleForObject(RE::BGSRefAlias::VMTYPEID, alias);
		}
		if (handle != policy->EmptyHandle()) {
			vm->FindBoundObject(handle, a_class.c_str(), object);
		}
		return object;
	}

	bool PlayerEffectActive(const RE::EffectSetting* a_effect)
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto target = player ? player->AsMagicTarget() : nullptr;
		const auto list = target ? target->GetActiveEffectList() : nullptr;
		if (!a_effect || !list) {
			return false;
		}
		using Flag = RE::ActiveEffect::Flag;
		return std::ranges::any_of(*list, [&](const RE::ActiveEffect* a_active) {
			return a_active && a_active->GetBaseObject() == a_effect && !a_active->flags.any(Flag::kInactive, Flag::kDispelled) &&
			       a_active->conditionStatus.get() != RE::ActiveEffect::ConditionStatus::kFalse;
		});
	}

	bool PlayerHasSpell(const RE::SpellItem* a_spell)
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !a_spell) {
			return false;
		}
		const auto spell = const_cast<RE::SpellItem*>(a_spell);
		if (player->HasSpell(spell)) {
			return true;
		}
		const auto& added = player->GetActorRuntimeData().addedSpells;
		if (std::ranges::find(added, spell) != added.end()) {
			return true;
		}
		if (const auto base = player->GetActorBase(); base && base->actorEffects) {
			const auto data = base->actorEffects;
			for (std::uint32_t i = 0; data->spells && i < data->numSpells; ++i) {
				if (data->spells[i] == spell) {
					return true;
				}
			}
		}
		const auto target = player->AsMagicTarget();
		const auto list = target ? target->GetActiveEffectList() : nullptr;
		return list && std::ranges::any_of(*list, [&](const RE::ActiveEffect* a_active) {
			return a_active && a_active->spell == static_cast<const RE::MagicItem*>(a_spell);
		});
	}

	std::optional<float> Value::Raw() const
	{
		if (constant) {
			return constant;
		}
		if (global) {
			return global->value;
		}
		if (running) {
			return quest->IsRunning() ? 1.0f : 0.0f;
		}
		if (spell) {
			return PlayerHasSpell(spell) ? 1.0f : 0.0f;
		}
		if (effect) {
			return PlayerEffectActive(effect) ? 1.0f : 0.0f;
		}
		if (!anyOf.empty()) {
			return std::ranges::any_of(anyOf, [](const Value& a_value) { return a_value.Holds(); }) ? 1.0f : 0.0f;
		}
		if (playerWeight) {
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto base = player ? player->GetActorBase() : nullptr;
			return base ? std::optional{ base->weight } : std::nullopt;
		}
		if (const auto object = ScriptObject(quest, script, alias)) {
			if (const auto v = object->GetVariable(variable)) {
				if (v->IsInt()) {
					return static_cast<float>(v->GetSInt());
				}
				if (v->IsFloat()) {
					return v->GetFloat();
				}
				if (v->IsBool()) {
					return v->GetBool() ? 1.0f : 0.0f;
				}
			}
		}
		return std::nullopt;
	}

	std::optional<float> Value::Get() const
	{
		auto v = Raw();
		if (v && !map.empty()) {
			*v = Map(map, *v);
		}
		return v ? v : fallback;
	}

	bool Value::Holds() const
	{
		const auto v = Get();
		if (!v) {
			return false;
		}
		if (atLeast || below) {
			return (!atLeast || *v >= *atLeast) && (!below || *v < *below);
		}
		return *v > 0.0f;
	}

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
		if (a_json.contains("below")) {
			value.below = a_json["below"].get<float>();
		}
		if (a_json.contains("default")) {
			value.fallback = a_json["default"].get<float>();
		}
		for (const auto& point : a_json.value("map", json::array())) {
			value.map.emplace_back(point.at(0).get<float>(), point.at(1).get<float>());
		}
		std::ranges::sort(value.map);
		if (a_json.contains("global")) {
			value.global = LookupForm<RE::TESGlobal>(a_json["global"], plugin);
		} else if (a_json.contains("questRunning")) {
			value.quest = LookupForm<RE::TESQuest>(a_json["questRunning"], plugin);
			value.running = value.quest != nullptr;
		} else if (a_json.contains("playerHasSpell")) {
			value.spell = LookupForm<RE::SpellItem>(a_json["playerHasSpell"], plugin);
		} else if (a_json.contains("effectActive")) {
			value.effect = LookupForm<RE::EffectSetting>(a_json["effectActive"], plugin);
		} else if (a_json.contains("anyOf")) {
			for (const auto& v : a_json["anyOf"]) {
				if (auto parsed = ParseValue(v, plugin); parsed.Valid()) {
					value.anyOf.push_back(std::move(parsed));
				}
			}
		} else if (a_json.contains("playerWeight")) {
			value.playerWeight = a_json["playerWeight"].get<bool>();
		} else if (a_json.contains("script")) {
			const auto& s = a_json["script"];
			value.quest = LookupForm<RE::TESQuest>(s.at("quest"), s.value("plugin", plugin));
			value.alias = s.value("alias", -1);
			value.script = s.at("script").get<std::string>();
			value.variable = s.at("variable").get<std::string>();
		}
		return value;
	}

	std::vector<Value> ParseStages(const json& a_parent, const std::string& a_plugin)
	{
		std::vector<Value> stages;
		for (const auto& v : a_parent.value("stages", json::array())) {
			stages.push_back(ParseValue(v, a_plugin));
		}
		return stages;
	}

	std::vector<float> GetStages(const std::vector<Value>& a_stages)
	{
		std::vector<float> stages;
		stages.reserve(a_stages.size());
		for (const auto& v : a_stages) {
			stages.push_back(v.Get().value_or(stages.empty() ? 0.0f : stages.back()));
		}
		return stages;
	}

	std::vector<std::string> ParseReplaces(const json& a_json)
	{
		std::vector<std::string> names;
		if (a_json.contains("replaces")) {
			const auto& r = a_json["replaces"];
			for (const auto& name : r.is_array() ? r : json::array({ r })) {
				names.push_back(name.get<std::string>());
			}
		}
		return names;
	}

	std::vector<Value> ParseConditions(const json& a_parent, const char* a_key, const std::string& a_plugin)
	{
		std::vector<Value> conditions;
		if (a_parent.contains(a_key)) {
			const auto& e = a_parent[a_key];
			for (const auto& v : e.is_array() ? e : json::array({ e })) {
				conditions.push_back(ParseValue(v, a_plugin));
			}
		}
		return conditions;
	}

	bool AllHold(const std::vector<Value>& a_conditions)
	{
		return std::ranges::all_of(a_conditions, [](const Value& a_value) { return a_value.Holds(); });
	}

	std::string StageName(const json& a_json, const std::string& a_plugin)
	{
		if (a_json.is_string()) {
			return a_json.get<std::string>();
		}
		const auto  fallback = a_json.value("fallback", ""s);
		const auto  plugin = a_json.value("plugin", a_plugin);
		std::string name;
		if (a_json.contains("spell")) {
			if (const auto spell = LookupForm<RE::SpellItem>(a_json["spell"], plugin)) {
				name = spell->GetFullName();
			}
		} else if (a_json.contains("effect")) {
			if (const auto effect = LookupForm<RE::EffectSetting>(a_json["effect"], plugin)) {
				name = effect->GetFullName();
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

	void Widget::Refresh(const std::string& a_owner)
	{
		// SKI_WidgetBase keeps its HUD path ("_root.WidgetContainer.<id>.widget") once SkyUI loaded it
		if (const auto object = ScriptObject(quest, script)) {
			static const RE::BSFixedString rootName{ "_widgetRoot" };
			const auto v = object->GetVariable(rootName);
			if (v && v->IsString() && !v->GetString().empty() && v->GetString() != root) {
				root = v->GetString();
				logs::info("{}: its widget is {}", a_owner, root);
			}
		}
	}

	std::vector<Widget> ParseWidgets(const json& a_json, const std::string& a_plugin)
	{
		std::vector<Widget> widgets;
		for (const auto& w : a_json.is_array() ? a_json : json::array({ a_json })) {
			Widget widget;
			widget.quest = LookupForm<RE::TESQuest>(w.at("quest"), w.value("plugin", a_plugin));
			widget.script = w.at("script").get<std::string>();
			widgets.push_back(std::move(widget));
		}
		return widgets;
	}

	void Hidden::Parse(const json& a_definition, const std::string& a_plugin)
	{
		if (!a_definition.contains("hide")) {
			return;
		}
		const auto& h = a_definition["hide"];
		hudPaths = h.value("hudPaths", std::vector<std::string>{});
		if (h.contains("skyuiWidget")) {
			widgets = ParseWidgets(h["skyuiWidget"], a_plugin);
		}
	}

	void Hidden::Collect(const std::string& a_owner, std::vector<std::string>& a_out)
	{
		a_out.insert(a_out.end(), hudPaths.begin(), hudPaths.end());
		for (auto& widget : widgets) {
			if (widget.Valid()) {
				widget.Refresh(a_owner);
			}
			if (!widget.root.empty()) {
				a_out.push_back(widget.root);
			}
		}
	}
}
