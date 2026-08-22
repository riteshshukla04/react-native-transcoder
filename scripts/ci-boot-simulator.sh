#!/bin/bash
# Picks an available iPhone simulator, boots it, and exports the device name and
# iOS version so rn-harness.config.mjs targets the same one.
set -euo pipefail

IFS=$'\t' read -r NAME VERSION UDID <<< "$(xcrun simctl list devices available --json | python3 -c '
import json, sys, re
data = json.load(sys.stdin)["devices"]
best = None
for runtime, devices in data.items():
    match = re.search(r"iOS-(\d+)-(\d+)", runtime)
    if match is None:
        continue
    version = (int(match.group(1)), int(match.group(2)))
    for device in devices:
        if not device.get("isAvailable"):
            continue
        if not device["name"].startswith("iPhone"):
            continue
        if best is None or version > best[0]:
            best = (version, device["name"], device["udid"])
if best is None:
    raise SystemExit("no available iPhone simulator")
(major, minor), name, udid = best
print(f"{name}\t{major}.{minor}\t{udid}")
')"

echo "booting $NAME (iOS $VERSION) $UDID"
xcrun simctl boot "$UDID" 2>/dev/null || true
xcrun simctl bootstatus "$UDID" -b

if [ -n "${GITHUB_ENV:-}" ]; then
  {
    echo "HARNESS_IOS_DEVICE=$NAME"
    echo "HARNESS_IOS_VERSION=$VERSION"
    echo "SIMULATOR_UDID=$UDID"
  } >> "$GITHUB_ENV"
fi
