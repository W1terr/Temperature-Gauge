#pragma once

// Pieces of the json definitions (Sources/*.json, Needs/*.json) that name game data: values read from globals or
// Papyrus scripts, stage names from spells, SkyUI widgets. Read them on the main thread.
namespace Defs
{
	// A number, { "global": "<id>" }, { "script": { "quest": "<id>", "script": "<class>", "variable": "<name>" } }
	// (auto properties: "::<name>_var") or { "questRunning": "<id>" } (1 while the quest runs); any of them can add
	// "plugin". As a condition it holds when > 0, or >= "atLeast" if given (Frostfall's switches are 1 = off, 2 = on).
	struct Value
	{
		std::optional<float> constant;
		RE::TESGlobal*       global{ nullptr };
		RE::TESQuest*        quest{ nullptr };
		bool                 running{ false };  // quest: whether it runs, not a script variable
		std::string          script;
		RE::BSFixedString    variable;
		std::optional<float> atLeast;

		bool Valid() const { return constant || global || quest; }

		std::optional<float> Get() const;
		bool                 Holds() const;
	};

	Value ParseValue(const json& a_json, const std::string& a_plugin);

	// "enabled": one value or a list, all must hold (left out: always on)
	std::vector<Value> ParseConditions(const json& a_parent, const char* a_key, const std::string& a_plugin);
	bool               AllHold(const std::vector<Value>& a_conditions);

	// A text, or { "spell": "<id>", "fallback": "..." }: the spell's name in the game's language with the part up to
	// the last '-' or ':' cut off (" Cold - Chilly", "Hunger: Peckish" -> "Chilly", "Peckish")
	std::string StageName(const json& a_json, const std::string& a_plugin);

	template <class T>
	T* LookupForm(const json& a_id, const std::string& a_plugin);

	RE::BSTSmartPointer<RE::BSScript::Object> ScriptObject(RE::TESQuest* a_quest, const std::string& a_class);

	// A SkyUI widget (SKI_WidgetBase script on a quest): its HUD path once SkyUI loaded it
	struct Widget
	{
		RE::TESQuest* quest{ nullptr };
		std::string   script;
		std::string   root;  // "_root.WidgetContainer.<id>.widget"; read again every time (SkyUI may move it after a load)

		bool Valid() const { return quest != nullptr; }
		void Refresh(const std::string& a_owner);
	};

	// "skyuiWidget": { "quest": "<id>", "script": "<class>" }
	Widget ParseWidget(const json& a_json, const std::string& a_plugin);
}
