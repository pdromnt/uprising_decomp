#!/bin/bash
# CI/build input only. Never installs system-wide Vulkan files.
set -euo pipefail
root="${1:?SDK destination required}"
version=1.4.363.0
if [[ -f "$root/macOS/share/vulkan/icd.d/libkosmickrisp_icd.json" ]]; then exit 0; fi
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
curl --fail --location --retry 3 "https://sdk.lunarg.com/sdk/download/$version/mac/vulkansdk-macos-$version.zip" -o "$work/sdk.zip"
echo "163431e0203c8215286a17cf393643ce6b951f77d1b4f74ab30821a1f0beb09b  $work/sdk.zip" | shasum -a 256 -c -
ditto -x -k "$work/sdk.zip" "$work/installer"
installer=$(find "$work/installer" -type f -path '*/Contents/MacOS/vulkansdk-macOS-*' -print -quit)
test -n "$installer"
"$installer" --root "$root" --accept-licenses --default-answer --confirm-command install copy_only=1
test -f "$root/macOS/share/vulkan/icd.d/libkosmickrisp_icd.json"
