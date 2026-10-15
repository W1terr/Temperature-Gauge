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

	template RE::TESGlobal* LookupForm<RE::TESGlobal>(const json&, const std::string&);
	template RE::TESQuest*  LookupForm<RE::TESQuest>(const json&, const std::string&);
	template RE::SpellItem* LookupForm<RE::SpellItem>(const json&, const std::string&);

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

	std::optional<float> Value::Get() const
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
		if (const auto object = ScriptObject(quest, script)) {
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

	bool Value::Holds() const
	{
		const auto v = Get();
		return v && (atLeast ? *v >= *atLeast : *v > 0.0f);
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
		if (a_json.contains("global")) {
			value.global = LookupForm<RE::TESGlobal>(a_json["global"], plugin);
		} else if (a_json.contains("questRunning")) {
			value.quest = LookupForm<RE::TESQuest>(a_json["questRunning"], plugin);
			value.running = true;
		} else if (a_json.contains("script")) {
			const auto& s = a_json["script"];
			value.quest = LookupForm<RE::TESQuest>(s.at("quest"), s.value("plugin", plugin));
			value.script = s.at("script").get<std::string>();
			value.variable = s.at("variable").get<std::string>();
		}
		return value;
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

	Widget ParseWidget(const json& a_json, const std::string& a_plugin)
	{
		Widget widget;
		widget.quest = LookupForm<RE::TESQuest>(a_json.at("quest"), a_json.value("plugin", a_plugin));
		widget.script = a_json.at("script").get<std::string>();
		return widget;
	}
}
