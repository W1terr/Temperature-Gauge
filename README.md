# Temperature Gauge

A Crimson Desert style temperature gauge for Skyrim survival mods, drawn with SKSE Menu Framework and set up on its
pages. It replaces the survival mod's own temperature icon (Survival Mode's compass thermometer, SunHelm's cold widget).

- Three zones like Crimson Desert: cold (blue shades), comfortable (green), hot (red shades), with thin lines between
  them and fading ends. Straight or bent into an arc of any angle, at any place, size and rotation; zone sizes and all
  colors can be changed.
- A white notched arrowhead (Crimson Desert) or a line marks your body temperature; chevrons show warming up /
  cooling down; the marker pulses when freezing. Optional: where your temperature is heading, the stage name.
- Show always, or only when not comfortable; fades with the HUD (Immersive HUD); hidden in menus.
- Menus in all the languages Skyrim ships with (English, French, German, Italian, Spanish, Polish, Russian, Japanese,
  Chinese), chosen on the Languages page.

## Requirements

- SKSE, Address Library for SKSE Plugins
- SKSE Menu Framework (3 or newer)
- A supported survival mod: Survival Mode (Creation Club, built in) or SunHelm Survival (patch in the installer)

## Supported survival mods

Each survival mod is described by a json file in `SKSE/Plugins/TemperatureGauge/Sources` (format: top of
`src/Sources.cpp`): the cold value and its stages, the area's temperature, the icon to hide. Survival Mode's comes with
the mod (`res/core`), patches live in `res/patches/<name>` and become options in the FOMOD installer.

## Translations

`SKSE/Plugins/TemperatureGauge/lang/<language>.json` (`res/core/...`): the English text is the key. `tools/check_lang.py`
reports missing or unused texts and broken placeholders; `--keys` prints a template for a new language.

## Building

`build.bat` (xmake, MSVC, CommonLibSSE-NG in `lib/commonlibsse-ng`), then `python tools/package.py <build dir> <output dir>`
(FOMOD archive).

Settings are saved to `Documents/My Games/Skyrim Special Edition/SKSE/TemperatureGauge.json`.

## Credits

- Inspired by Crimson Desert's temperature gauge (Pearl Abyss)
- CommonLibSSE-NG (MIT), SKSE Menu Framework API (LGPL-2.1), SKSE team, Address Library
