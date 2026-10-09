/* StatLog.cpp
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

#include "StatLog.h"

#include "Files.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <memory>
#include <mutex>

using namespace std;

namespace {
	mutex logMutex;
	filesystem::path logPath;
	bool pathWasSet = false;
	unique_ptr<ofstream> logFile;
}



StatLog::Entry &StatLog::Entry::Add(const string &key, const string &value)
{
	return AddRaw(key, Quote(value));
}



StatLog::Entry &StatLog::Entry::Add(const string &key, const char *value)
{
	return AddRaw(key, Quote(value));
}



StatLog::Entry &StatLog::Entry::Add(const string &key, int64_t value)
{
	return AddRaw(key, to_string(value));
}



StatLog::Entry &StatLog::Entry::Add(const string &key, int value)
{
	return AddRaw(key, to_string(value));
}



StatLog::Entry &StatLog::Entry::AddRaw(const string &key, const string &json)
{
	fields.emplace_back(key, json);
	return *this;
}



StatLog::Entry &StatLog::Entry::Append(const Entry &other)
{
	fields.insert(fields.end(), other.fields.begin(), other.fields.end());
	return *this;
}



bool StatLog::Entry::IsEmpty() const
{
	return fields.empty();
}



string StatLog::Entry::ToString() const
{
	string result = "{";
	for(const auto &[key, value] : fields)
	{
		if(result.size() > 1)
			result += ',';
		result += Quote(key);
		result += ':';
		result += value;
	}
	result += '}';
	return result;
}



string StatLog::Quote(const string &text)
{
	static const char HEX[] = "0123456789abcdef";
	string result = "\"";
	for(char c : text)
	{
		unsigned char u = static_cast<unsigned char>(c);
		if(c == '"' || c == '\\')
		{
			result += '\\';
			result += c;
		}
		else if(c == '\n')
			result += "\\n";
		else if(c == '\t')
			result += "\\t";
		else if(c == '\r')
			result += "\\r";
		else if(u < 0x20)
		{
			result += "\\u00";
			result += HEX[u >> 4];
			result += HEX[u & 0xF];
		}
		else
			result += c;
	}
	result += '"';
	return result;
}



string StatLog::Now()
{
	// Use local time with its UTC offset, so that the time of day the player
	// plays at is preserved: "2026-10-09T21:15:03-07:00".
	time_t timestamp = chrono::system_clock::to_time_t(chrono::system_clock::now());
	tm date;
#ifdef _WIN32
	localtime_s(&date, &timestamp);
#else
	localtime_r(&timestamp, &date);
#endif
	char str[32];
	size_t length = strftime(str, sizeof(str), "%Y-%m-%dT%H:%M:%S%z", &date);
	string result(str, length);
	// strftime writes the offset as "-0700"; ISO 8601 readers expect "-07:00".
	if(result.size() >= 5)
		result.insert(result.size() - 2, ":");
	return result;
}



void StatLog::SetPath(const filesystem::path &path)
{
	lock_guard<mutex> lock(logMutex);
	logPath = path;
	pathWasSet = true;
	logFile.reset();
}



void StatLog::Write(const Entry &entry)
{
	lock_guard<mutex> lock(logMutex);
	if(!pathWasSet)
	{
		// The config directory is only known once the game has started.
		if(Files::Config().empty())
			return;
		logPath = Files::Config() / "stats.jsonl";
		pathWasSet = true;
	}
	if(logPath.empty())
		return;

	if(!logFile)
	{
		logFile = make_unique<ofstream>(logPath, ios::out | ios::app | ios::binary);
		if(!*logFile)
		{
			// Don't retry on every event if the file can't be opened.
			logPath.clear();
			logFile.reset();
			return;
		}
	}

	Entry line;
	line.Add("time", Now()).Append(entry);
	*logFile << line.ToString() << '\n';
	logFile->flush();
}
