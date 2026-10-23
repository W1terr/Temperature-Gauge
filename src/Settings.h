#pragma once

using RGBA = std::array<float, 4>;

enum class Visibility : int
{
	kAlways,
	kNotComfortable,  // hidden while warm / comfortable and not getting colder
	kNever,
};

enum class MarkerStyle : int
{
	kArrow,  // Crimson Desert's notched arrowhead
	kLine,   // a notch across the gauge
};

enum class NeedStyle : int
{
	kRing,  // a ring around the icon that empties
	kBar,   // the icon and a bar beside it
	kArc,   // pieces of a circle one after another, e.g. around a minimap opposite the temperature gauge
};

enum class NeedMarker : int
{
	kTemperature,  // whatever the temperature gauge uses
	kArrow,
	kLine,
};

// The hunger, thirst and fatigue gauges: one per need the survival mod tracks, in a row or a column, or as arcs on a
// circle. Same units as the temperature gauge; the position is the middle of the group (arcs: the circle's center).
// Bars and arcs are drawn like the temperature gauge: a band, the marker where the need is (the full end is fed /
// quenched / rested).
struct NeedsSettings
{
	bool       enabled{ false };
	Visibility visibility{ Visibility::kAlways };  // kNotComfortable: each need shows while it has penalties
	float      hideDelay{ 4.0f };
	std::array<bool, 6> show{ true, true, true, true, true, true };  // hunger, thirst, fatigue, inebriation, weight, vitality

	NeedStyle style{ NeedStyle::kRing };
	bool      column{ false };  // stacked instead of side by side
	float     posX{ 0.81f };
	float     posY{ 0.09f };
	float     size{ 34.0f };    // ring diameter / icon size
	float     spacing{ 10.0f };
	float     thickness{ 4.0f };
	float     barLength{ 80.0f };
	float     barCurve{ 0.0f };  // degrees the bar bends through; below 0 the other way
	float     arcRadius{ 100.0f };
	float     arcStart{ 20.0f };  // clockwise degrees from 12 o'clock where the first arc begins
	float     arcSpan{ 60.0f };   // degrees each arc covers, below 0 counterclockwise
	float     arcGap{ 0.0f };     // 0: one line, the needs end to end
	bool      arcReverse{ false };  // the needs in the other order
	bool      arcFlip{ false };     // the full end at the arc's end instead of its start
	bool      icons{ true };
	bool      ticks{ true };    // marks between the stages
	NeedMarker marker{ NeedMarker::kLine };
	bool      colorByNeed{ true };  // bars / arcs: one color per need; off: by stage like the temperature gauge's zones
	bool      fadeEnds{ false };
	bool      pulseDanger{ true };
	bool      showLabel{ false };
	float     labelSize{ 13.0f };

	RGBA fine{ 0.92f, 0.94f, 0.90f, 1.0f };
	RGBA warning{ 0.96f, 0.72f, 0.25f, 1.0f };
	RGBA danger{ 0.88f, 0.17f, 0.12f, 1.0f };
	RGBA bonus{ 0.98f, 0.84f, 0.38f, 1.0f };
	RGBA track{ 0.0f, 0.0f, 0.0f, 0.45f };
	RGBA icon{ 0.95f, 0.96f, 0.93f, 1.0f };
	std::array<RGBA, 6> needColors{ { { 0.97f, 0.55f, 0.13f, 1.0f },     // hunger: orange
		{ 0.13f, 0.40f, 0.96f, 1.0f },                                 // thirst: blue
		{ 0.56f, 0.36f, 0.19f, 1.0f },                                 // fatigue: brown
		{ 0.92f, 0.76f, 0.22f, 1.0f },                                 // inebriation: mead gold
		{ 0.78f, 0.32f, 0.36f, 1.0f },                                 // weight: muscle red
		{ 0.42f, 0.78f, 0.36f, 1.0f } } };                             // vitality: leaf green
};

// The stress gauge (Stress and Fear page) looks like one of the needs gauges and has the same settings; arcFlip
// (the calm end at the other end) makes the ring fill up with stress and puts the bar's calm end on the left. Only
// needColors[0] is used. kNotComfortable: shown while there is any stress.
inline NeedsSettings StressDefaults()
{
	NeedsSettings n;
	n.enabled = true;
	n.visibility = Visibility::kNotComfortable;
	n.hideDelay = 3.0f;
	n.posX = 0.875f;
	n.arcFlip = true;
	n.colorByNeed = false;
	n.arcStart = 200.0f;
	n.needColors[0] = { 0.62f, 0.38f, 0.92f, 1.0f };  // violet
	return n;
}

// The fear indicator: an eye and the phobia's name while a phobia works (the player faces what they fear)
struct FearSettings
{
	bool  enabled{ true };
	float posX{ 0.5f };
	float posY{ 0.16f };
	float size{ 30.0f };         // the eye's width
	bool  icon{ true };
	bool  showName{ true };      // "Fear of Wolves"
	float labelSize{ 16.0f };
	bool  pulse{ true };
	bool  showPhobias{ false };  // the phobias the player has, faint, while not afraid
	RGBA  color{ 0.66f, 0.36f, 0.95f, 1.0f };
};

// Everything the gauge can be set up with in SKSE Menu Framework. Sizes are pixels at 1080p and scale with the screen
// height (and the scale setting); positions are fractions of the screen.
//
// The gauge has three zones from the cold end to the hot end: cold (blue), comfortable (green), hot (red).
struct Settings
{
	bool       enabled{ true };
	bool       hideVanillaIcon{ true };
	Visibility visibility{ Visibility::kAlways };
	float      hideDelay{ 4.0f };  // seconds comfortable before "not comfortable" hides it
	bool       followHud{ true };  // fade with the HUD (Immersive HUD and the like)
	float      opacity{ 1.0f };

	// shape and place
	float posX{ 0.745f };
	float posY{ 0.09f };
	float scale{ 1.0f };
	float length{ 125.0f };
	float thickness{ 5.5f };
	float curve{ 55.0f };     // degrees the gauge bends through, 0 = straight
	float rotation{ -22.0f }; // degrees clockwise, 0 = upright with the hot end on top
	bool  mirror{ true };     // false: the curve's center to the right, true: to the left
	bool  flip{ false };      // hot end at the other end
	bool  roundEnds{ false };
	bool  fadeEnds{ true };   // the ends fade out
	float outline{ 1.0f };

	// zones: parts of the gauge's length (comfortable gets the rest)
	float coldZone{ 0.47f };
	float hotZone{ 0.27f };
	bool  blendZones{ false };  // soft color changes instead of hard edges
	bool  zoneLines{ true };    // thin dark lines between the zones

	// markers
	MarkerStyle marker{ MarkerStyle::kArrow };
	bool        markerInside{ true };  // the arrow on the inner side of the curve
	float       markerSize{ 1.0f };
	bool        showTarget{ false };    // where the body temperature is heading
	bool        showTrend{ true };      // chevrons while warming up / cooling down
	bool        pulseDanger{ true };
	bool        showLabel{ false };     // "Chilly", "Freezing"...
	float       labelSize{ 15.0f };
	float       smoothing{ 0.8f };      // seconds the marker takes to catch up

	// colors: the zones (cold end -> cold edge, comfortable, hot edge -> hot end) and the markers
	RGBA coldEnd{ 0.09f, 0.20f, 0.96f, 1.0f };
	RGBA coldEdge{ 0.14f, 0.56f, 0.96f, 1.0f };
	RGBA comfortable{ 0.45f, 0.81f, 0.44f, 1.0f };
	RGBA hotEdge{ 0.93f, 0.43f, 0.31f, 1.0f };
	RGBA hotEnd{ 0.72f, 0.06f, 0.06f, 1.0f };
	RGBA outlineColor{ 0.0f, 0.0f, 0.0f, 0.55f };
	RGBA markerColor{ 0.95f, 0.97f, 0.94f, 1.0f };
	RGBA targetColor{ 1.0f, 1.0f, 1.0f, 0.75f };
	RGBA labelColor{ 1.0f, 1.0f, 1.0f, 0.9f };

	// hunger, thirst and fatigue gauges (optional; while on, the survival mod's own need icons are hidden)
	NeedsSettings needs;

	// Stress and Fear page (only while a supported stress mod is loaded)
	NeedsSettings stress{ StressDefaults() };
	FearSettings  fear;
};

namespace Config
{
	Settings&  Get();
	std::mutex& Lock();

	void Load();
	void Save();
	void Reset();       // everything back to the defaults
	void ResetNeeds();  // the needs gauges' look back to the defaults (stays on / off)
	void ResetStress(); // the stress gauge and the fear indicator back to the defaults (stay on / off)

	// layouts the settings page offers as buttons; keep colors and behaviour
	enum class Preset
	{
		kHalfCircle,
		kVertical,
		kHorizontal,
	};
	void Apply(Preset a_preset);
}
