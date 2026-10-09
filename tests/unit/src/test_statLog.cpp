/* test_statLog.cpp
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

#include "es-test.hpp"

// Include only the tested class's header.
#include "../../../source/StatLog.h"

// ... and any system includes needed for the test file.
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace { // test namespace

// #region unit tests
SCENARIO( "Quoting strings for the play log", "[StatLog]" ) {
	GIVEN( "plain text" ) {
		THEN( "it is wrapped in quotes" ) {
			CHECK( StatLog::Quote("Food") == "\"Food\"" );
			CHECK( StatLog::Quote("") == "\"\"" );
		}
	}
	GIVEN( "text with quotes, backslashes, and control characters" ) {
		THEN( "they are escaped" ) {
			CHECK( StatLog::Quote("say \"hi\"") == "\"say \\\"hi\\\"\"" );
			CHECK( StatLog::Quote("a\\b") == "\"a\\\\b\"" );
			CHECK( StatLog::Quote("line\nbreak\ttab") == "\"line\\nbreak\\ttab\"" );
			CHECK( StatLog::Quote(std::string(1, '\x01')) == "\"\\u0001\"" );
		}
	}
}

SCENARIO( "Building play log entries", "[StatLog]" ) {
	GIVEN( "an empty entry" ) {
		StatLog::Entry entry;
		THEN( "it is an empty JSON object" ) {
			CHECK( entry.IsEmpty() );
			CHECK( entry.ToString() == "{}" );
		}
	}
	GIVEN( "an entry with string and number fields" ) {
		StatLog::Entry entry;
		entry.Add("verb", "commodity sold").Add("amount", int64_t{-5}).Add("first visit", 1);
		THEN( "the fields are written in order" ) {
			CHECK_FALSE( entry.IsEmpty() );
			CHECK( entry.ToString() == "{\"verb\":\"commodity sold\",\"amount\":-5,\"first visit\":1}" );
		}
		WHEN( "another entry is appended" ) {
			StatLog::Entry nested;
			nested.Add("plugin", "1.0");
			entry.Append(StatLog::Entry().Add("price", 300)).AddRaw("plugins", nested.ToString());
			THEN( "its fields follow, and raw JSON is not quoted" ) {
				const std::string expected = "{\"verb\":\"commodity sold\",\"amount\":-5,\"first visit\":1,"
					"\"price\":300,\"plugins\":{\"plugin\":\"1.0\"}}";
				CHECK( entry.ToString() == expected );
			}
		}
	}
}

SCENARIO( "Writing the play log", "[StatLog]" ) {
	GIVEN( "a log file path" ) {
		const std::filesystem::path path = std::filesystem::temp_directory_path() / "es-test-stats.jsonl";
		std::filesystem::remove(path);
		StatLog::SetPath(path);

		WHEN( "two entries are written" ) {
			StatLog::Write(StatLog::Entry().Add("verb", "jumped"));
			StatLog::Write(StatLog::Entry().Add("verb", "landed"));
			THEN( "each is one line, prefixed with the time" ) {
				std::ifstream in(path);
				std::string first;
				std::string second;
				std::string third;
				REQUIRE( std::getline(in, first) );
				REQUIRE( std::getline(in, second) );
				CHECK_FALSE( std::getline(in, third) );
				CHECK( first.starts_with("{\"time\":\"") );
				CHECK( first.ends_with(",\"verb\":\"jumped\"}") );
				CHECK( second.ends_with(",\"verb\":\"landed\"}") );
			}
		}
		WHEN( "the path is cleared" ) {
			StatLog::SetPath({});
			StatLog::Write(StatLog::Entry().Add("verb", "jumped"));
			THEN( "nothing is written" ) {
				CHECK_FALSE( std::filesystem::exists(path) );
			}
		}

		StatLog::SetPath({});
		std::filesystem::remove(path);
	}
}

SCENARIO( "Timestamps for the play log", "[StatLog]" ) {
	GIVEN( "the current time" ) {
		const std::string now = StatLog::Now();
		THEN( "it is an ISO 8601 local time with a UTC offset" ) {
			// e.g. "2026-10-09T21:15:03-07:00"
			REQUIRE( now.size() == 25 );
			CHECK( now[4] == '-' );
			CHECK( now[10] == 'T' );
			CHECK( (now[19] == '+' || now[19] == '-') );
			CHECK( now[22] == ':' );
		}
	}
}
// #endregion unit tests



} // test namespace
