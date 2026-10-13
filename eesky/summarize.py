#!/usr/bin/env python3
# Summarize how someone plays Endless Sky, from the play log (stats.jsonl) and
# their most recent save. This is the first step of the nightly mutation loop:
# its output is what the analysis agent starts from.
#
# Usage: eesky/summarize.py [data directory]   (default: ~/eesky-data)
#        eesky/summarize.py --json [data directory]

import collections
import json
import re
import sys
from datetime import datetime
from pathlib import Path

# Words in job board mission names that tell passenger jobs from cargo jobs,
# for log lines written before missions recorded their passengers and cargo.
PEOPLE_WORDS = ("passenger", "tourist", "family", "professional", "celebration", "refugee", "student", "colonist")
CARGO_WORDS = ("delivery", "cargo", "bulk", "freight", "shipment", "equipment")


def load_log(directory):
	path = directory / "stats.jsonl"
	if not path.exists():
		return []
	rows = []
	for line in path.read_text().splitlines():
		line = line.strip()
		if line:
			try:
				rows.append(json.loads(line))
			except json.JSONDecodeError:
				pass
	return rows


def newest_save(directory):
	saves = [p for p in (directory / "saves").glob("*.txt") if "~~" not in p.name]
	return max(saves, key=lambda p: p.stat().st_mtime) if saves else None


def load_conditions(save):
	"""Read the top-level conditions block of a save file into a dict."""
	conditions = {}
	inBlock = False
	for line in save.read_text().splitlines():
		if line == "conditions":
			inBlock = True
			continue
		if inBlock:
			if not line.startswith("\t") or line.startswith("\t\t"):
				if not line.startswith("\t"):
					break
				continue
			match = re.match(r'^\t(?:"([^"]*)"|`([^`]*)`|(\S+))(?:\s+(-?\d+))?$', line)
			if match:
				name = match.group(1) or match.group(2) or match.group(3)
				conditions[name] = int(match.group(4)) if match.group(4) else 1
	return conditions


def job_kind(row):
	if row.get("passengers"):
		return "people"
	if row.get("cargo"):
		return "cargo"
	name = row.get("mission", "").lower()
	if any(word in name for word in PEOPLE_WORDS):
		return "people"
	if any(word in name for word in CARGO_WORDS):
		return "cargo"
	return "other"


def is_rush(row):
	if "deadline" in row:
		return bool(row["deadline"])
	return "rush" in row.get("mission", "").lower()


def is_real_mission(row):
	"""Leave out invisible bookkeeping missions such as message buoys."""
	if "visible" in row:
		return bool(row["visible"])
	return not row.get("mission", "").startswith(("Message Buoy", "Spaceport Reminder"))


def session_hours(rows):
	total = 0.
	start = None
	for row in rows:
		if row.get("verb") == "game started":
			start = datetime.fromisoformat(row["time"])
		elif row.get("verb") == "game quit" and start:
			total += (datetime.fromisoformat(row["time"]) - start).total_seconds()
			start = None
	return total / 3600.


def summarize(directory):
	rows = load_log(directory)
	verbs = collections.Counter(row.get("verb") for row in rows)
	missions = [row for row in rows if row.get("verb", "").startswith("mission ") and is_real_mission(row)]
	jobs = [row for row in missions if row.get("object") == "job"]
	accepted = [row for row in jobs if row["verb"] == "mission accepted"]
	completedJobs = [row for row in jobs if row["verb"] == "mission completed"]
	kinds = collections.Counter(job_kind(row) for row in accepted)
	rush = sum(is_rush(row) for row in accepted)

	sold = [row for row in rows if row.get("verb") == "commodity sold"]
	trades = sum(1 for row in rows if row.get("verb") in ("commodity bought", "commodity sold"))
	profit = sum(row.get("profit", 0) for row in sold)

	save = newest_save(directory)
	conditions = load_conditions(save) if save else {}
	eesky = {k: v for k, v in conditions.items() if k.startswith("eesky: ")}

	return {
		"log lines": len(rows),
		"first event": rows[0]["time"] if rows else None,
		"last event": rows[-1]["time"] if rows else None,
		"sessions": verbs["game started"],
		"hours played": round(session_hours(rows), 1),
		"pilot": next((row["pilot"] for row in reversed(rows) if "pilot" in row), None),
		"save": save.name if save else None,
		"travel": {
			"jumps": verbs["jumped"],
			"landings": verbs["landed"],
			"systems discovered": conditions.get("stat: systems discovered", 0),
			"planets discovered": conditions.get("stat: planets discovered", 0),
		},
		"missions": {
			"offered": sum(1 for row in missions if row["verb"] == "mission offered"),
			"accepted": sum(1 for row in missions if row["verb"] == "mission accepted"),
			"completed": sum(1 for row in missions if row["verb"] == "mission completed"),
			"failed": sum(1 for row in missions if row["verb"] == "mission failed"),
			"aborted": sum(1 for row in missions if row["verb"] == "mission aborted"),
			"declined": sum(1 for row in missions if row["verb"] == "mission declined"),
			"by where offered": dict(collections.Counter(row["object"] for row in missions
				if row["verb"] == "mission accepted")),
		},
		"jobs": {
			"accepted": len(accepted),
			"completed": len(completedJobs),
			"people": kinds["people"],
			"cargo": kinds["cargo"],
			"other": kinds["other"],
			"rush": rush,
			"passengers delivered": conditions.get("stat: passengers delivered", 0),
			"cargo tons delivered": conditions.get("stat: mission cargo delivered", 0),
		},
		"trading": {"trades": trades, "profit": profit},
		"combat": {
			"destroyed": verbs["destroyed"],
			"disabled": verbs["disabled"],
			"boarded": verbs["boarded"],
			"captured": verbs["captured"],
			"assisted": verbs["assisted"],
			"provoked": verbs["provoked"],
			"scans": verbs["scanned cargo"] + verbs["scanned outfits"],
			"flagship destroyed": verbs["flagship destroyed"],
			"deaths": verbs["died"],
		},
		"shopping": {
			"outfits bought": verbs["outfit bought"],
			"ships bought": verbs["ship bought"],
			"ships sold": verbs["ship sold"],
		},
		"hails": verbs["hailed ship"] + verbs["hailed planet"],
		"credits": {
			"peak": conditions.get("stat: peak: credits"),
			"low": conditions.get("stat: low: credits"),
		},
		"traits": {k[len("eesky: trait: "):]: v for k, v in eesky.items() if k.startswith("eesky: trait: ")},
		"wants": {k[len("eesky: wants: "):]: v for k, v in eesky.items() if k.startswith("eesky: wants: ")},
		"answers": {k[len("eesky: answer: "):]: v for k, v in eesky.items() if k.startswith("eesky: answer: ")},
		"beggar": {k[len("eesky: beggar: "):]: v for k, v in eesky.items()
			if k.startswith("eesky: beggar: ") and k not in ("eesky: beggar: last",)},
		"conversation choices": verbs["conversation choice"],
	}


def print_summary(summary):
	print(f"Pilot {summary['pilot']} ({summary['save']}): {summary['sessions']} sessions, "
		f"{summary['hours played']} hours, {summary['first event']} to {summary['last event']}")
	for section in ("travel", "missions", "jobs", "trading", "combat", "shopping", "credits", "traits", "wants",
			"answers", "beggar"):
		values = summary[section]
		if not values or not any(v for v in values.values()):
			continue
		text = ", ".join(f"{k} {v}" for k, v in values.items() if v)
		print(f"  {section}: {text}")
	print(f"  hails: {summary['hails']}, conversation choices: {summary['conversation choices']}")


def main():
	args = [arg for arg in sys.argv[1:] if arg != "--json"]
	directory = Path(args[0]).expanduser() if args else Path.home() / "eesky-data"
	summary = summarize(directory)
	if "--json" in sys.argv:
		print(json.dumps(summary, indent=2))
	else:
		print_summary(summary)


if __name__ == "__main__":
	main()
