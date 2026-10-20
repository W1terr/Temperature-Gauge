#pragma once

#include "Defs.h"
#include "Needs.h"

// One meter of a json definition, shared by the needs (Needs/*.json) and stress (Stress/*.json): a value from 0 (fed,
// rested, calm) to "max" (the worst) and the survival mod's stages on the way.
//
//   "enabled": { "questRunning": "0x008EE8" },   this meter runs (left out: always); a list = all of them
//   "value": { "global": "0x00EAAE" },           0 = fed / quenched / rested / calm .. "max" = the worst
//   "max": 360,                                  or a value; left out: 1.25 x the last stage
//   "stages": [40, 80, 140, 240, 360],           value where stage 1, 2, ... begins (numbers or values, e.g. globals)
//   "stageNames": [ { "spell": "0x001824", "fallback": "Well Fed" }, ... ],   stage 0 .. N
//   "comfortableStages": 2,                      stages 0 .. n-1 have no penalties
//   "dangerStage": 4,                            from this stage on the gauge pulses (default: the second to last)
//   "bonusStages": 1,                            stages 0 .. n-1 are a bonus (Well Fed), shown in the bonus color
//   "bonus": { "when": { ... }, "name": { "spell": "...", "fallback": "Well Fed" } }
//                                                or a bonus outside the stages (Survival Mode's Well Fed / Well
//                                                Rested); a list of them: the first that holds
//   "fillsUp": true,                             a ring fills up as the value grows (inebriation) instead of emptying
//   "detail": { "value": { ... }, "format": "{:.0f} kg" }   a number shown before the stage name
//   "hide": { ... }                              the mod's own widget for this meter (like a definition's "hide"),
//                                                hidden only while this meter's gauge is shown
//
// Values, conditions and stage names: see Defs.h.
namespace Meter
{
	struct Bonus
	{
		Defs::Value when;
		std::string name;
	};

	struct Def
	{
		std::vector<Defs::Value> enabled;
		Defs::Value              value;
		Defs::Value              max;
		std::vector<Defs::Value> stages;      // start of stage 1 .. N
		std::vector<std::string> stageNames;  // stage 0 .. N
		int                      comfortableStages{ 1 };
		int                      dangerStage{ 1 };
		int                      bonusStages{ 0 };
		std::vector<Bonus>       bonuses;
		bool                     fillsUp{ false };
		Defs::Value              detailValue;
		std::string              detailFormat;
		Defs::Hidden             hidden;

		bool On() const { return Defs::AllHold(enabled); }

		// "Satisfied, Peckish, ..." for the log
		std::string Describe() const;
	};

	Def Load(const json& a_json, const std::string& a_plugin);

	NeedReading Read(const Def& a_def);
}
