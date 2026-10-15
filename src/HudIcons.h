#pragma once

// The game's HUD movie: hides the survival mods' own icons (temperature, needs) and tells how visible the HUD is.
namespace HudIcons
{
	// Queues a UI task: hides these HUD clips, shows again the ones hidden earlier that aren't in the list any more, and
	// reads the HUD's opacity. Call a few times a second.
	void Update(std::vector<std::string> a_hide);

	// 0..1: the HUD movie's own opacity (Immersive HUD and similar fade it), 0 while the HUD menu isn't open
	float Alpha();
}
