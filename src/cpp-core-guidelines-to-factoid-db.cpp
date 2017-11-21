/*
 * cpp-core-guidelines-to-factoid-db.cpp
 *
 *  Created on: Sep 22, 2016
 *      Author: galik
 */

#include <string>
#include <experimental/filesystem>

#include <hol/simple_logger.h>
#include <hol/small_types.h>
#include <hol/string_utils.h>

#include "include/skivvy/plugin-factoid.h"

//namespace adl {

namespace hol {
	using namespace header_only_library::string_utils;
}

using namespace header_only_library::simple_logger;
using namespace header_only_library::small_types::basic;
using namespace header_only_library::small_types::string_containers;
using namespace header_only_library::small_types::ios;
using namespace header_only_library::small_types::ios::functions;

namespace fs = std::experimental::filesystem;

void convert(fs::path const& guide, str const& db)
{
	bug_fun();
	bug_var(guide);
	bug_var(db);

	fs::path store_file = "factoid-" + db + "-db-store.txt";
	fs::path index_file = "factoid-" + db + "-db-index.txt";

	bug_var(store_file);
	bug_var(index_file);

	skivvy::ircbot::factoid::FactoidManager fm{store_file, index_file};

	// * [P.3: Express intent](#Rp-what)
	// * [Enum.1: Prefer enumerations over macros](#Renum-macro)

	// ###\s+<a\s+name="([^"]+)"></a>(\w+\.\d+):\s+.*?

	// #1 key2
	// #2 sect
	// #3 num
	// #4 desc

	//	<a name="([^"]+)"><\/a>(\w+)\.(\d+):\s+(.*)
	//	### <a name="Rp-typesafe"></a>P.4: Ideally, a program should be statically type safe
	//
	//	##### Reason
	//
	//	Ideally, a program would be completely statically (compile-time) type safe.
	//	Unfortunately, that is not possible. Problem areas:
	//
	//	* unions
	//	* casts
	//	* array decay
	//	* range errors
	//	* narrowing conversions
	//
	//	##### Note
	//
	//	These areas are sources of serious problems (e.g., crashes and security violations).
	//	We try to provide alternative techniques.
	//
	//	##### Enforcement
	//
	//	We can ban, restrain, or detect the individual problem categories separately, as required and feasible for individual programs.
	//	Always suggest an alternative.
	//	For example:
	//
	//	* unions -- use `variant` (in C++17)
	//	* casts -- minimize their use; templates can help
	//	* array decay -- use `span` (from the GSL)
	//	* range errors -- use `span`
	//	* narrowing conversions -- minimize their use and use `narrow` or `narrow_cast` (from the GSL) where they are necessary
	//	### <a name="Rp-typesafe"></a>P.4: Ideally, a program should be statically type safe


	// #1 sec
	// #2 sec-desc
	// #3 link
	std::regex es{R"(\*\s+\[([^:\d\]]+?):\s+([^\]]+?)\]\(#([^)]+?)\))", std::regex_constants::optimize};
	// #1 sec
	// #2 num
	// #3 desc
	// #4 link
	std::regex er{R"(\*\s+\[([^.]+)\.(\d+):\s+([^\]]+)\]\(#([^)]+)\))", std::regex_constants::optimize};
	std::smatch m;

	if(auto ifs = std::ifstream(guide, std::ios::binary))
	{
		for(str line; sgl(ifs, line);)
		{
			if(std::regex_search(line, m, es))
			{
				//	* [In: Introduction](#S-introduction)
				//	* [P: Philosophy](#S-philosophy)
				//	* [I: Interfaces](#S-interfaces)
				//	* [F: Functions](#S-functions)
				//	* [C: Classes and class hierarchies](#S-class)
				//	* [Enum: Enumerations](#S-enum)
				//	* [R: Resource management](#S-resource)
				//	* [ES: Expressions and statements](#S-expr)
				//	* [E: Error handling](#S-errors)
				str sec = "sec";
//				str key = hol::lower_copy(m.str(1));
//				str link = hol::lower_copy(m.str(3));
				str key = m.str(1);
				str link = m.str(3);
				str fact = "Section Name: [" + sec + "]: " + hol::lower_copy(m.str(2));

				fm.add_fact(key, fact, {sec});
			}
			else if(!std::regex_search(line, m, er))
				continue;

			bug_var(line);

//			str sec = hol::lower_copy(m.str(1));
//			str key1 = hol::lower_copy(m.str(1) + m.str(2));
//			str key2 = hol::lower_copy(m.str(4));
			str sec = m.str(1);
			str key1 = m.str(1) + "." + m.str(2);
			str key2 = m.str(4);
			str fact1 = "[" + key1 + "] " + m.str(3) + " {" + key2 + "}";
			str fact2 = "=" + key1;



			bug_var(sec);
			bug_var(key1);
			bug_var(fact1);
			bug_var(key2);
			bug_var(fact2);

			fm.add_fact(key1, fact1, {sec});
			fm.add_fact(key2, fact2, {sec});
		}

		return;
	}

	hol_throw_errno();
}

void convert2(fs::path const& guide, str const& db)
{
	bug_fun();
	bug_var(guide);
	bug_var(db);

	fs::path store_file = "factoid-" + db + "-db-store.txt";
	fs::path index_file = "factoid-" + db + "-db-index.txt";

	bug_var(store_file);
	bug_var(index_file);

	const std::regex e{R"~(###\s+<a name="([^"]+)"><\/a>(\w+)\.(\d+):\s+(.*))~"};

	skivvy::ircbot::factoid::FactoidManager fm{store_file, index_file};

	// #1 alt
	// #2 sect
	// #3 num
	// #4 fact

	//	<a name="([^"]+)"><\/a>(\w+)\.(\d+):\s+(.*)

	if(auto ifs = std::ifstream(guide, std::ios::binary))
	{
		str key;
		str alt;
		str fact;
		str link;
		str sect;

		str_vec reason;
		bool in_reason = false;
		int reason_count = 0;

		std::smatch m;
		for(str line; sgl(ifs, line);)
		{
			if(hol::trim_mute(line).empty())
			{
				if(reason_count)
					--reason_count;
//				if(in_reason && !reason.empty())
//					in_reason = false;
				continue;
			}

			if(std::regex_match(line, m, e))
			{
				if(!key.empty()) // previous result
				{
					fm.add_fact(key, fact, {sect});
					fm.add_fact(alt, link, {sect});
					for(auto const& fact: reason)
						fm.add_fact(key, fact, {sect});
					reason_count = 0;
				}

				key = m.str(2) + "." + m.str(3);
				alt = m.str(1);
				fact = "[" + key + "] " + m.str(4) + " {" + alt + "}";
				sect = m.str(2);
				link = "=" + key;

				continue;
			}

			if(key.empty())
				continue;

			if(!line.find("##### Reason"))
			{
				reason.clear();
				reason_count = 2; // indicate new reason gathering
				reason.push_back("Reason:");
				continue;
			}

			if(reason_count)
				reason.push_back(line);
		}
	}
}

//} // adl

int main(int, char** argv)
{
	try
	{
		if(!argv[1])
			hol_throw_runtime_error("must supply filename of CppCoreGuidelines");

		fs::path guide = argv[1];

		if(!argv[2])
			hol_throw_runtime_error("must supply output database name");

		str db = argv[2];

		convert2(guide, db);
	}
	catch(std::exception const& e)
	{
		LOG::X << e.what();
		return EXIT_FAILURE;
	}
	catch(...)
	{
		LOG::X << "Unknown exception";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;

}


