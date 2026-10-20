#pragma once

enum class Need : int
{
	kHunger,
	kThirst,
	kFatigue,
	kInebriation,  // Realistic Needs and Diseases
	kWeight,       // ... the body's weight (muscular .. skinny)
};
inline constexpr std::size_t kNeedCount = 5;

// What a stage on a need's gauge is like
enum class NeedZone : std::uint8_t
{
	kBonus,    // Well Fed, Well Rested...
	kFine,     // no penalties
	kPenalty,
	kDanger,   // the worst stages
};
inline constexpr int kMaxNeedSegments = 16;

// One need as the gauges show it
struct NeedReading
{
	float       fill{ 1.0f };        // 1 = fed / quenched / rested .. 0 = the worst stage; every stage gets the same share
	int         segments{ 1 };       // stages on the gauge (tick marks between them)
	std::array<NeedZone, kMaxNeedSegments> zones{};  // per segment, from the full end (segment 0) to the empty end
	bool        bonus{ false };      // Well Fed, Well Rested...
	bool        comfortable{ true }; // no penalties
	bool        danger{ false };     // the worst stages
	std::string stage;               // the survival mod's name for the current stage
	std::string detail;              // shown before the stage name ("100 kg"), may be empty
	bool        fillsUp{ false };    // a ring fills up as it gets worse (inebriation) instead of emptying
	float       value{ 0.0f };       // what the mod has, 0 .. max (for the settings page)
	float       max{ 1.0f };
};

using NeedReadings = std::array<std::optional<NeedReading>, kNeedCount>;

// Hunger, thirst and fatigue (and inebriation and weight) of the survival mods, described by json files in
// Data/SKSE/Plugins/TemperatureGauge/Needs (Survival Mode comes with the mod, SunHelm with its patch). Each need comes
// from the highest priority mod that has it running.
namespace Needs
{
	void Init();  // after the data is loaded

	// Runs on the main thread a few times a second
	void Sample();

	// The latest readings, nullopt for needs no supported mod tracks right now
	NeedReadings Current();

	// For the settings page: the mod each need is read from (empty: none), every loaded definition
	std::array<std::string, kNeedCount> SourceNames();
	std::string                         LoadedSources();

	// HUD clips of the survival mods' own need icons (to hide): the mods' and those of the needs shown
	std::vector<std::string> IconPaths(const std::array<bool, kNeedCount>& a_shown);
}
