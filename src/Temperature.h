#pragma once

// What the gauge shows, on a scale that follows the gauge's three zones:
//   -1 .. 0  cold zone (-1 = the cold end, deadly cold; 0 = the cold edge of comfortable)
//    0 .. 1  comfortable zone
//    1 .. 2  hot zone (2 = the hot end)
struct Reading
{
	float                body{ 0.0f };
	std::optional<float> target;          // where the body temperature is heading (the area's temperature)
	int                  trend{ 0 };      // -2 cooling fast, -1 cooling, 0 steady, 1 warming, 2 near a heat source
	bool                 comfortable{ true };  // no penalties
	bool                 danger{ false };      // close to the deadly end
	std::string          stage;                // the survival mod's name for the current stage
};

// The survival mods the gauge can read. Each one is described by a json file in
// Data/SKSE/Plugins/TemperatureGauge/Sources (Survival Mode comes with the mod, others are patches in the installer).
namespace Temperature
{
	void Init();  // after the data is loaded

	// Runs on the main thread a few times a second (game globals, Papyrus script variables)
	void Sample();

	// The latest reading, nullopt while no supported survival mod is running
	std::optional<Reading> Current();

	// For the settings page: the mod read now, and every loaded definition (empty: none)
	std::string SourceName();
	std::string LoadedSources();

	// HUD clips of the survival mods' own temperature icons (to hide)
	std::vector<std::string> IconPaths();
}
