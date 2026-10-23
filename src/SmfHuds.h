#pragma once

// HUD elements other mods draw through SKSE Menu Framework (Frostfall 2026's and Last Seed 2026's bars), hidden while
// this mod's gauges stand in for them. Those SKSE plugins look up the framework's RegisterHudElement with
// GetProcAddress; a patch of that import in their DLL hands them a stand-in that registers a wrapper around their draw
// function. While hidden, the wrapper clips what they draw away (all but a "keep" rect, their start-up logo) whenever
// the game runs; menus (their inventory extras) and the framework's own window (their settings preview) stay as they
// are.
//
// In a definition: "hide": { "smfHud": { "dll": "Frostfall", "keep": [0.32, 0.23, 0.68, 0.43] } }, keep = left, top,
// right, bottom as fractions of the screen; may be a list.
namespace SmfHuds
{
	// Reads the "smfHud" entries of every definition file and patches the DLLs that are loaded. Call when this plugin
	// loads (before kPostLoad, when they register their HUD) and again at kPostLoad for DLLs that loaded after it.
	void Install();

	// The DLLs ("Frostfall") whose HUD is hidden from now on; the others show again
	void Hide(const std::vector<std::string>& a_dlls);
}
