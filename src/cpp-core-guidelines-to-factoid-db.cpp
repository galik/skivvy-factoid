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

using namespace hol::simple_logger;
using namespace hol::small_types::basic;
using namespace hol::small_types::string_containers;
using namespace hol::small_types::ios;
using namespace hol::small_types::ios::functions;

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
			str key1 = m.str(1) + m.str(2);
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

		convert(guide, db);
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


