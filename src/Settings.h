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
};

namespace Config
{
	Settings&  Get();
	std::mutex& Lock();

	void Load();
	void Save();
	void Reset();  // everything back to the defaults

	// layouts the settings page offers as buttons; keep colors and behaviour
	enum class Preset
	{
		kHalfCircle,
		kVertical,
		kHorizontal,
	};
	void Apply(Preset a_preset);
}
