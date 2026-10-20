#pragma once

namespace UI
{
	// Adds the HUD element and the settings page to SKSE Menu Framework; false if it isn't installed
	bool Register();

	// After a language change the sidebar pages get their new names here, on the next HUD frame (not while the
	// framework draws its pages)
	void UpdatePageNames();

	// whether the framework's settings window is open (the gauge is previewed then)
	bool SettingsOpen();

	// The preview on the settings page (not saved)
	struct Preview
	{
		bool  enabled{ false };  // false: the live reading
		float body{ 0.5f };
		float target{ -0.5f };
		int   trend{ -1 };
		bool  sweep{ false };    // move the marker up and down by itself

		bool  needs{ false };      // the needs gauges show made up values
		bool  needsSweep{ true };  // ... going round
		bool  stress{ false };     // the stress gauge and the fear indicator show made up values
		bool  stressSweep{ true };
	};
	Preview& GetPreview();
}
