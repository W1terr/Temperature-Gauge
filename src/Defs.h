#pragma once

// Pieces of the json definitions (Sources/*.json, Needs/*.json, Stress/*.json) that name game data: values read from
// globals or Papyrus scripts, stage names from spells, SkyUI widgets. Read them on the main thread.
namespace Defs
{
	// A number, { "global": "<id>" }, { "script": { "quest": "<id>", "script": "<class>", "variable": "<name>" } }
	// (auto properties: "::<name>_var"; add "alias": <alias id> for a script on one of the quest's aliases),
	// { "questRunning": "<id>" } (1 while the quest runs), { "playerHasSpell": "<id>" } (1 while the player has the
	// spell), { "effectActive": "<magic effect id>" } (1 while one of the player's effects of it works, its conditions
	// met), { "anyOf": [ values ] } (1 while any of them holds) or { "playerWeight": true } (the character's body
	// weight, 0 .. 100); any of them can add "plugin". "map": [[x, y], ...]
	// turns what was read into another number, straight lines between the points (Frostbite's body temperature 1 =
	// warm .. 0 = freezing as cold 0 .. 100: [[0, 100], [1, 0]]); "default": a number for while it can't be read (a
	// script that hasn't started). As a condition it holds when > 0, or >= "atLeast" / < "below" if given (Frostfall's
	// switches are 1 = off, 2 = on).
	struct Value
	{
		std::optional<float>                 constant;
		RE::TESGlobal*                       global{ nullptr };
		RE::TESQuest*                        quest{ nullptr };
		bool                                 running{ false };  // quest: whether it runs, not a script variable
		int                                  alias{ -1 };       // script: on this alias of the quest
		RE::SpellItem*                       spell{ nullptr };
		RE::EffectSetting*                   effect{ nullptr };
		std::vector<Value>                   anyOf;
		bool                                 playerWeight{ false };
		std::string                          script;
		RE::BSFixedString                    variable;
		std::vector<std::pair<float, float>> map;
		std::optional<float>                 fallback;
		std::optional<float>                 atLeast;
		std::optional<float>                 below;

		bool Valid() const { return constant || global || quest || spell || effect || !anyOf.empty() || playerWeight; }

		std::optional<float> Get() const;
		bool                 Holds() const;

	private:
		std::optional<float> Raw() const;
	};

	Value ParseValue(const json& a_json, const std::string& a_plugin);

	// whether one of the player's active effects of this magic effect works (not switched off by its conditions)
	bool PlayerEffectActive(const RE::EffectSetting* a_effect);

	// whether the player has the spell: the game's own check, the spells added to the character (Papyrus AddSpell),
	// the character's base spells, or an effect of it on the player
	bool PlayerHasSpell(const RE::SpellItem* a_spell);

	// "stages": a list of values (numbers or globals), read again every sample; one that can't be read repeats the one
	// before it
	std::vector<Value> ParseStages(const json& a_parent, const std::string& a_plugin);
	std::vector<float> GetStages(const std::vector<Value>& a_stages);

	// whether a definition is for the loaded plugin: it's loaded and, with "light": true / false, is (isn't) flagged
	// ESL (Frostfall 2026 and Last Seed 2026 have ESL builds with renumbered forms); logs why not as "<a_what>: ..."
	bool PluginLoaded(const json& a_definition, const std::string& a_plugin, const std::string& a_what);

	// "replaces": a definition's "name" or a list of them, left out while this one is loaded (Survival Mode Improved
	// takes over Survival Mode's globals)
	std::vector<std::string> ParseReplaces(const json& a_json);

	template <class T>
	void DropReplaced(std::vector<T>& a_sources)
	{
		std::vector<std::string> replaced;
		for (const auto& source : a_sources) {
			replaced.insert(replaced.end(), source.replaces.begin(), source.replaces.end());
		}
		std::erase_if(a_sources, [&](const T& a_source) {
			if (std::ranges::find(replaced, a_source.name) == replaced.end()) {
				return false;
			}
			logs::info("{}: replaced by another definition, left out", a_source.name);
			return true;
		});
	}

	// "enabled": one value or a list, all must hold (left out: always on)
	std::vector<Value> ParseConditions(const json& a_parent, const char* a_key, const std::string& a_plugin);
	bool               AllHold(const std::vector<Value>& a_conditions);

	// A text, or { "spell": "<id>", "fallback": "..." } / { "effect": "<magic effect id>", ... }: the spell's (magic
	// effect's) name in the game's language with the part up to the last '-' or ':' cut off (" Cold - Chilly",
	// "Hunger: Peckish" -> "Chilly", "Peckish")
	std::string StageName(const json& a_json, const std::string& a_plugin);

	template <class T>
	T* LookupForm(const json& a_id, const std::string& a_plugin);

	// a_alias >= 0: the script on that alias of the quest
	RE::BSTSmartPointer<RE::BSScript::Object> ScriptObject(RE::TESQuest* a_quest, const std::string& a_class, int a_alias = -1);

	// A SkyUI widget (SKI_WidgetBase script on a quest): its HUD path once SkyUI loaded it
	struct Widget
	{
		RE::TESQuest* quest{ nullptr };
		std::string   script;
		std::string   root;  // "_root.WidgetContainer.<id>.widget"; read again every time (SkyUI may move it after a load)

		bool Valid() const { return quest != nullptr; }
		void Refresh(const std::string& a_owner);
	};

	// "skyuiWidget": { "quest": "<id>", "script": "<class>" } or a list of them
	std::vector<Widget> ParseWidgets(const json& a_json, const std::string& a_plugin);

	// "hide": { "hudPaths": [...], "skyuiWidget": ..., "smfHud": ... } of a definition: the HUD clips of its own icons
	// and the SKSE plugins whose SKSE Menu Framework HUD to hide (see SmfHuds.h)
	struct Hidden
	{
		std::vector<std::string> hudPaths;
		std::vector<Widget>      widgets;
		std::vector<std::string> smfHuds;  // DLL names

		void Parse(const json& a_definition, const std::string& a_plugin);

		// refreshes the widgets' paths and adds every path to a_out
		void Collect(const std::string& a_owner, std::vector<std::string>& a_out);
	};
}
