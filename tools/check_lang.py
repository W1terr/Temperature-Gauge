"""
Checks the translations in res/core/SKSE/Plugins/TemperatureGauge/lang against the texts the code translates.

The English texts are the keys: T("..."), F("..."), Id("..."), Hint("..."), Color("...", ...), EnumCombo("...", ...), the combo name lists
(xxxNames{ ... }), the sidebar pages (SidebarPage{ "..." }), kGlyphHint and Lang::T(...) in Gauge.cpp.
Reports keys missing from a language file, keys the code no longer uses and broken {} placeholders.
usage: python check_lang.py [--keys]   (--keys: print the keys as a json template)
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).parent.parent
SOURCES = [ROOT / "src" / name for name in ("Menu.cpp", "Gauge.cpp")]
LANG_DIR = ROOT / "res" / "core" / "SKSE" / "Plugins" / "TemperatureGauge" / "lang"
LANGUAGES = ["french", "german", "italian", "spanish", "polish", "russian", "japanese", "chinese"]

LITERAL = r'"(?:[^"\\]|\\.)*"'


def code_keys() -> list[str]:
    keys = []

    def add(key):
        if key not in keys:
            keys.append(key)

    for path in SOURCES:
        src = re.sub(r"//[^\n]*", "", path.read_text(encoding="utf-8"))
        for m in re.finditer(rf"\b(?:T|F|Id|Hint|Color|EnumCombo)\(\s*({LITERAL})", src):
            add(json.loads(m.group(1)))
        for m in re.finditer(r"\w+Names\{([^}]*)\}", src):
            for lit in re.findall(LITERAL, m.group(1)):
                add(json.loads(lit))
        for m in re.finditer(rf"SidebarPage\{{\s*({LITERAL})", src):
            add(json.loads(m.group(1)))
        for m in re.finditer(rf"kGlyphHint\s*=\s*({LITERAL})", src):
            add(json.loads(m.group(1)))
        for m in re.finditer(r"Lang::T\(([^;]*)\);", src):
            for lit in re.findall(LITERAL, m.group(1)):
                add(json.loads(lit))
    return keys


def placeholders(text: str) -> list[str]:
    return sorted(re.findall(r"\{[^{}]*\}|%[-.\d]*[a-z]", text))


def main():
    keys = code_keys()
    if "--keys" in sys.argv:
        print(json.dumps({k: "" for k in keys}, ensure_ascii=False, indent="\t"))
        return 0
    problems = 0
    for language in LANGUAGES:
        path = LANG_DIR / f"{language}.json"
        if not path.exists():
            print(f"{language}: MISSING FILE")
            problems += 1
            continue
        table = json.loads(path.read_text(encoding="utf-8"))
        missing = [k for k in keys if not table.get(k)]
        unused = [k for k in table if k not in keys]
        broken = [k for k in keys if table.get(k) and placeholders(k) != placeholders(table[k])]
        problems += len(missing) + len(unused) + len(broken)
        print(f"{language}: {len(keys) - len(missing)}/{len(keys)}"
              + "".join(f"\n  missing: {k}" for k in missing)
              + "".join(f"\n  unused: {k}" for k in unused)
              + "".join(f"\n  placeholders differ: {k}" for k in broken))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
