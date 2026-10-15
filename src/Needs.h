#pragma once

enum class Need : int
{
	kHunger,
	kThirst,
	kFatigue,
};
inline constexpr std::size_t kNeedCount = 3;

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
};

using NeedReadings = std::array<std::optional<NeedReading>, kNeedCount>;

// Hunger, thirst and fatigue of the survival mods, described by json files in
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

	// HUD clips of the survival mods' own need icons (to hide)
	std::vector<std::string> IconPaths();
}
