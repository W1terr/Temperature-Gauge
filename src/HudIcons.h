#pragma once

// The game's HUD movie: hides the survival mods' own temperature icons and tells how visible the HUD is.
namespace HudIcons
{
	// Queues a UI task: shows / hides the icons (Temperature::IconPaths) and reads the HUD's opacity. Call a few times
	// a second.
	void Update(bool a_hide);

	// 0..1: the HUD movie's own opacity (Immersive HUD and similar fade it), 0 while the HUD menu isn't open
	float Alpha();
}
