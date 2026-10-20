#pragma once

// Immersive HUD SKSE (Fuzzlesz) has no API: it fades the _root of every open Scaleform menu that doesn't take input,
// with an MCM entry per movie (named after the file). Our gauges are drawn by SKSE Menu Framework, not by a menu, so
// for each gauge group an invisible menu with an empty movie is kept open (Interface/TemperatureGauge/<name>.swf) and
// the gauges follow the opacity Immersive HUD gives it. Without Immersive HUD nothing is registered.
namespace IHud
{
	enum class Group : std::uint32_t
	{
		kTemperature,
		kNeeds,
		kStress,
	};

	void Register();  // after the data is loaded (main thread)

	// Opens the menus while the HUD is up, again where they were closed (loading screens such as fast travel close
	// them); main thread, a few times a second
	void KeepOpen();

	// 0..1: what Immersive HUD wants the group to be (1 without it, or while its menu isn't open)
	float Alpha(Group a_group);
}
