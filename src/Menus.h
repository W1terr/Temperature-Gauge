#pragma once

// Which game menus that cover the HUD are open. Kept from the game's menu open / close events, so the gauge (drawn on
// the render thread) never looks into the game's menu map.
namespace Menus
{
	void Register();  // after the data is loaded, on the main thread

	// a menu is open the gauge shouldn't be drawn over (dialogue, map, inventory, console...), also when it doesn't pause
	// the game (Skyrim Souls RE)
	bool HudCovered();
}
