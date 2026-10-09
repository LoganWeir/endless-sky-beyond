/* StatLog.h
Copyright (c) 2026 by Logan Weir

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>



// Append-only log of play events, one JSON object per line ("stats.jsonl" in
// the config directory). It records what the player does so that tools outside
// the game can analyze how they play. Writing never affects the game itself.
class StatLog {
public:
	// One line of the log: an ordered list of JSON fields.
	class Entry {
	public:
		Entry &Add(const std::string &key, const std::string &value);
		Entry &Add(const std::string &key, const char *value);
		Entry &Add(const std::string &key, int64_t value);
		Entry &Add(const std::string &key, int value);
		// Add a field whose value is already valid JSON.
		Entry &AddRaw(const std::string &key, const std::string &json);
		// Append the fields of another entry to this one.
		Entry &Append(const Entry &other);

		bool IsEmpty() const;
		// Format the entry as a single-line JSON object.
		std::string ToString() const;

	private:
		std::vector<std::pair<std::string, std::string>> fields;
	};


public:
	// Quote and escape a string as a JSON string literal.
	static std::string Quote(const std::string &text);
	// The current real-world time as an ISO 8601 UTC timestamp.
	static std::string Now();

	// Set the file to append to. By default, this is "stats.jsonl" in the
	// config directory. An empty path disables the log.
	static void SetPath(const std::filesystem::path &path);
	// Write the entry, prefixed with the current real-world time.
	static void Write(const Entry &entry);
};
