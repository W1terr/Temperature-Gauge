#pragma once

struct Settings;

namespace UI
{
	// The hunger, thirst and fatigue gauges, drawn from the HUD element every frame (a_gameplay: the HUD is up and no
	// menu covers it; a_hud: the HUD's opacity to follow)
	void DrawNeeds(const Settings& a_settings, float a_dt, float a_time, bool a_preview, bool a_gameplay, float a_hud);

	// The stress gauge (drawn like one of the needs gauges) and the fear indicator, same arguments
	void DrawStress(const Settings& a_settings, float a_dt, float a_time, bool a_preview, bool a_gameplay, float a_hud);

	// Arcs: puts them on the circle the bent temperature gauge lies on, on the other side of it, sharing the same
	// angle between them. False (nothing changed) while the temperature gauge is straight. Render thread.
	bool PlaceNeedsOnTemperatureCircle(Settings& a_settings);
}
