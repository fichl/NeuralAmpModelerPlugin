import html
import json
import re
import sys
import urllib.request


url = sys.argv[1]
sys.stdout.reconfigure(encoding="utf-8")
with urllib.request.urlopen(url) as response:
    source = response.read().decode("utf-8")

seen = set()
for match in re.finditer(r'"((?:\\.|[^"\\])*)"', source):
    try:
        value = json.loads(f'"{match.group(1)}"')
    except json.JSONDecodeError:
        continue
    value = html.unescape(value)
    lowered = value.lower()
    if (
        180 <= len(value) <= 20000
        and not value.lstrip().startswith(("{", "["))
        and value not in seen
        and any(
            term in lowered
            for term in (
                "oversampl",
                "time-scale",
                "timescale",
                "latenza",
                "linear phase",
                "multicore",
                "interpol",
                "accuratezza",
                "esr",
            )
        )
    ):
        seen.add(value)
        print("\n--- MESSAGE ---\n")
        print(value)
