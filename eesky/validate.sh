#!/usr/bin/env bash
# Parse the game data plus every plugin in eesky/plugins, without a save, and
# exit with an error if anything fails to parse or references missing content.
# This is the check to run after changing a plugin.
#
# Usage: eesky/validate.sh [path/to/endless-sky]
# By default, the first Debug or Release binary found under build/ is used.

set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ES=${1:-$(ls "$ROOT"/build/*/Debug/endless-sky "$ROOT"/build/*/Release/endless-sky 2>/dev/null | head -n 1 || true)}
if [ -z "$ES" ] || [ ! -x "$ES" ]; then
	echo "No endless-sky binary found. Build the game first, or pass its path." >&2
	exit 1
fi

# Use a scratch config directory that contains only links to this repo's plugins.
CONFIG=$(mktemp -d)
trap 'rm -rf "$CONFIG"' EXIT
mkdir -p "$CONFIG/plugins"
for plugin in "$ROOT"/eesky/plugins/*/; do
	ln -s "${plugin%/}" "$CONFIG/plugins/"
done

"$ES" -p --config "$CONFIG" --resources "$ROOT"
