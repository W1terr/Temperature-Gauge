#pragma once

#include "Needs.h"

// Stress and fears (phobias) as the stress gauge and the fear indicator show them
struct StressReading
{
	std::optional<NeedReading> stress;   // the mod's stress (0 = calm; fill 1 = calm .. 0 = the worst), nullopt while off
	std::vector<std::string>   afraid;   // phobias working right now ("Fear of Wolves")
	std::vector<std::string>   phobias;  // every phobia the player has
	bool                       fears{ false };  // the mod's fears are on
};

// Stress mods described by json files in Data/SKSE/Plugins/TemperatureGauge/Stress (Stress and Fear comes with the
// mod). The Stress and Fear settings page is there only while one of them is loaded.
namespace Stress
{
	void Init();  // after the data is loaded

	// a supported stress mod is loaded
	bool Available();

	// Runs on the main thread a few times a second
	void Sample();

	StressReading Current();

	// For the settings page: the mod read now (empty: none)
	std::string SourceName();

	// HUD clips of the mod's own stress widgets (to hide while our gauge is on)
	std::vector<std::string> IconPaths();
}
