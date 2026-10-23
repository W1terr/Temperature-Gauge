r"""Builds the FOMOD archive "Temperature Gauge <version>.zip". The version is the target's set_version in xmake.lua (a
trailing ".0" is left out of the name).

  00 Core      SKSE\Plugins\TemperatureGauge.dll + res\core (Survival Mode's definitions, translations)
  10 Patches   one option per folder in res\patches (survival mod definitions); the option's name is the folder name,
               its description and the plugin that makes it "Recommended" come from the json inside

usage: python package.py [build dir] [output dir]"""

import json
import re
import sys
import zipfile
from pathlib import Path
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build"
OUTPUT = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "dist"
PATCHES = ROOT / "res" / "patches"


def version(target: str) -> str:
    text = (ROOT / "xmake.lua").read_text(encoding="utf-8")
    match = re.search(r'target\("' + re.escape(target) + r'"\)\s*\n\s*set_version\("([^"]+)"\)', text)
    full = match.group(1) if match else "0.0.0"
    return re.sub(r"\.0$", "", full)


def patch_info(folder: Path) -> tuple[str, str]:
    """description and plugin of a patch folder, from its temperature definition (Sources), else any definition; not
    from the ESL build's ("light": true, no description)"""
    def regular(paths):
        return [p for p in sorted(paths) if not json.loads(p.read_text(encoding="utf-8")).get("light", False)]
    source = next(iter(regular(folder.rglob("Sources/*.json")) or regular(folder.rglob("*.json"))))
    data = json.loads(source.read_text(encoding="utf-8"))
    return data.get("description", f"Support for {folder.name}."), data["plugin"]


def patch_xml(folder: Path) -> str:
    description, plugin = patch_info(folder)
    return f"""
                        <plugin name="{escape(folder.name)}">
                            <description>{escape(description)}</description>
                            <files>
                                <folder source="10 Patches\\{escape(folder.name)}" destination="" priority="0"/>
                            </files>
                            <typeDescriptor>
                                <dependencyType>
                                    <defaultType name="Optional"/>
                                    <patterns>
                                        <pattern>
                                            <dependencies operator="And">
                                                <fileDependency file="{escape(plugin)}" state="Active"/>
                                            </dependencies>
                                            <type name="Recommended"/>
                                        </pattern>
                                    </patterns>
                                </dependencyType>
                            </typeDescriptor>
                        </plugin>"""


def module_config(patches: list[Path]) -> str:
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<config xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="http://qconsulting.ca/fo3/ModConfig5.0.xsd">
    <moduleName>Temperature Gauge</moduleName>
    <requiredInstallFiles>
        <folder source="00 Core" destination=""/>
    </requiredInstallFiles>
    <installSteps order="Explicit">
        <installStep name="Survival Mods">
            <optionalFileGroups order="Explicit">
                <group name="Patches (Survival Mode from the Creation Club is always supported)" type="SelectAny">
                    <plugins order="Explicit">{''.join(patch_xml(p) for p in patches)}
                    </plugins>
                </group>
            </optionalFileGroups>
        </installStep>
    </installSteps>
</config>
"""


def info(ver: str) -> str:
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<fomod>
    <Name>Temperature Gauge</Name>
    <Author>W1ter</Author>
    <Version>{ver}</Version>
    <Description>A Crimson Desert style temperature gauge for survival mods, with optional hunger, thirst and fatigue gauges and a stress gauge and fear indicator for Stress and Fear, set up in SKSE Menu Framework.</Description>
    <Groups>
        <element>User Interface</element>
    </Groups>
</fomod>
"""


def main():
    ver = version("TemperatureGauge")
    plugin = BUILD / "windows" / "x64" / "releasedbg"
    files = {"00 Core/SKSE/Plugins/TemperatureGauge.dll": plugin / "TemperatureGauge.dll"}
    for path in (ROOT / "res" / "core").rglob("*"):
        if path.is_file():
            files["00 Core/" + path.relative_to(ROOT / "res" / "core").as_posix()] = path
    patches = sorted(p for p in PATCHES.iterdir() if p.is_dir())
    for patch in patches:
        for path in patch.rglob("*"):
            if path.is_file():
                files[f"10 Patches/{patch.name}/" + path.relative_to(patch).as_posix()] = path

    OUTPUT.mkdir(parents=True, exist_ok=True)
    archive = OUTPUT / f"Temperature Gauge {ver}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zf:
        for entry, path in files.items():
            zf.write(path, entry)
        zf.writestr("fomod/info.xml", info(ver))
        zf.writestr("fomod/ModuleConfig.xml", module_config(patches))
    print(f"wrote {archive} ({archive.stat().st_size / 1024:.0f} KB, {len(files) + 2} files, patches: "
          f"{', '.join(p.name for p in patches)})")


if __name__ == "__main__":
    main()
