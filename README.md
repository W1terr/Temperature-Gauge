# Temperature Gauge

A Crimson Desert style temperature gauge for Skyrim survival mods, drawn with SKSE Menu Framework and set up on its
pages. It replaces the survival mod's own temperature icon (Survival Mode's compass thermometer, SunHelm's cold widget,
Frostfall's exposure meter, Frostbite's meter).

- Three zones like Crimson Desert: cold (blue shades), comfortable (green), hot (red shades), with thin lines between
  them and fading ends. Straight or bent into an arc of any angle, at any place, size and rotation; zone sizes and all
  colors can be changed.
- A white notched arrowhead (Crimson Desert) or a line marks your body temperature; chevrons show warming up /
  cooling down; the marker pulses when freezing. Optional: where your temperature is heading, the stage name.
- Show always, or only when not comfortable; fades with the HUD (Immersive HUD, and Immersive HUD SKSE with an entry
  for each group of gauges in its menu); hidden in menus.
- Optional hunger, thirst and fatigue gauges (Needs page): arcs on the temperature gauge's circle (one click puts them
  on the other side of a minimap), bars drawn like the temperature gauge, or rings with an apple, a water drop and a
  moon; a color per need or by stage, the temperature gauge's line or arrow marker, stage marks, pulsing at the worst
  stages, stage names. Survival Mode (also with Survival Mode Improved - SKSE or Conner's Survival Mode) has hunger and
  fatigue, SunHelm and Realistic Needs and Diseases all three, RND also inebriation and weight (a bottle and a
  dumbbell); their own need icons are hidden while the gauges are on. Frostfall and Frostbite have no needs of their
  own; next to them the gauges read the needs mod you run with them.
- With Stress and Fear - A Dynamic Sanity System: a Stress and Fear page with a stress gauge (ring, bar or arc, its
  stages from calm to critical combat stress) and a fear indicator (an eye and the phobia's name while you fight what
  you fear).
- Menus in all the languages Skyrim ships with (English, French, German, Italian, Spanish, Polish, Russian, Japanese,
  Chinese), chosen on the Languages page.

## Requirements

- SKSE, Address Library for SKSE Plugins
- SKSE Menu Framework (3 or newer)
- A supported survival mod: Survival Mode (Creation Club, built in), Survival Mode Improved - SKSE, Conner's Survival
  Mode, SunHelm Survival, Frostfall, Frostbite or Realistic Needs and Diseases (patches in the installer)

## Credits

- Inspired by Crimson Desert's temperature gauge (Pearl Abyss)
- CommonLibSSE-NG (MIT), SKSE Menu Framework API (LGPL-2.1), SKSE team, Address Library
