#!/usr/bin/env bash
# Pull the play log and saves from the device the game is played on, then
# print a summary of how the player plays.
#
# Usage: eesky/pull.sh [user@host] [destination]
# Defaults: deck@steamdeck (over Tailscale) and ~/eesky-data.

set -euo pipefail

HOST=${1:-deck@steamdeck}
DEST=${2:-$HOME/eesky-data}
CONFIG=.local/share/endless-sky

mkdir -p "$DEST"
rsync -a -e "ssh -o ConnectTimeout=15" \
  "$HOST:$CONFIG/stats.jsonl" "$HOST:$CONFIG/saves" "$DEST/"

python3 "$(dirname "$0")/summarize.py" "$DEST"
