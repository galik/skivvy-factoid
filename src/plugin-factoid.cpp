/*
 *  Created on: 30 Jan 2012
 *      Author: oaskivvy@gmail.com
 */

/*-----------------------------------------------------------------.
| Copyright (C) 2011 SooKee oaskivvy@gmail.com                     |
'------------------------------------------------------------------'

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.

http://www.gnu.org/licenses/gpl-2.0.html

'-----------------------------------------------------------------*/

#include <skivvy/plugin-factoid.h>
#include <skivvy/plugin-chanops.h>

#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <experimental/filesystem>

#include <hol/string_utils.h>
#include <hol/small_types.h>
#include <hol/random_utils.h> // TODO: random fact
#include <hol/time_utils.h>

#include <skivvy/logrep.h>
#include <skivvy/irc.h>
#include <skivvy/utils.h>

#include <hol/bug.h>
#include <hol/simple_logger.h>

#define throw_error(msg) \
	do { \
		std::ostringstream oss; \
		oss << msg; \
		throw std::runtime_error(oss.str()); \
	}while(0)

namespace skivvy {
namespace ircbot {
namespace factoid {

IRC_BOT_PLUGIN(FactoidIrcBotPlugin);
PLUGIN_INFO("factoid", "Factoid", "0.6.0");

using namespace skivvy;
using namespace skivvy::irc;
using namespace skivvy::utils;
using namespace skivvy::ircbot;
using namespace skivvy::ircbot::chanops;

using namespace hol::random_utils;
using namespace hol::simple_logger;
using namespace hol::small_types::basic;

namespace fs = std::experimental::filesystem;

const str DATABASE_FILE_PREFIX_KEY = "factoid.db.prefix";
const str DATABASE_FILE_PREFIX_DEFAULT = "factoid";

const str DATABASE_FILE_SUFFIX_KEY = "factoid.db.suffix";
const str DATABASE_FILE_SUFFIX_DEFAULT = "db.txt";

const str DATABASE_META_PREFIX = "[METADATA]";

const str METADATA_TITLE_KEY = "Title";
const str METADATA_CREATED_KEY = "Created";
const str METADATA_UPDATED_KEY = "Updated";

const str STORE_FILE = "factoid.store.file";
const str STORE_FILE_DEFAULT = "factoid-store.txt";
//
//const str INDEX_FILE = "factoid.index.file";
//const str INDEX_FILE_DEFAULT = "factoid-index.txt";

const str FACT_USER = "factoid.fact.user";
const str FACT_WILD_USER = "factoid.fact.wild.user";
const str FACT_SREG_USER = "factoid.fact.sreg.user";
const str FACT_CHANOPS_USERS = "factoid.fact.chanops.users";

const str FACT_CHANNEL_GROUP = "factoid.channel.group";

const str FACT_DATABASE = "factoid.db";
const str FACT_CHANNEL_DATABASE = "factoid.channel.db";

// from sookee::ios
static
std::istream& getnested(std::istream& is, str& s, char d1, char d2)
{
	using std::ws;

	if(is.peek() != d1)
	{
		is.setstate(std::ios::failbit);
		return is;
	}

	str n;
	uns d = 0;
	char c;
	while(is.get(c))
	{
		n.append(1, c);
		if(c == d1)
			++d;
		else if(c == d2)
			--d;
		if(!d)
			break;
	}

	if(n.empty())
		is.setstate(std::ios::failbit);
	else
	{
		s = n;
		is.clear();
	}

	return is;
}

FactoidManager::FactoidManager(const str& store_file, const str& index_file)
: store(store_file)
, index(index_file)
{
	store.case_insensitive(true);
	index.case_insensitive(true);
}

void FactoidManager::set_metadata(const str& key, const str& val)
{
	store.set(DATABASE_META_PREFIX + "-" + key, val);
}

str FactoidManager::get_metadata(const str& key, const str& dflt)
{
	return store.get(DATABASE_META_PREFIX + "-" + key, dflt);
}

/**
 * Add a fact by keyword and optionally add it to groups.
 * @param key
 * @param fact
 * @param groups
 * @return
 */
void FactoidManager::add_fact(const str& key, const str& fact, const str_set& groups)
{
	store.add(key, fact);

	if(!groups.empty())
		add_to_groups(key, groups);
}

/**
 * Add keyword to groups.
 * @param key
 * @param groups
 * @return
 */
void FactoidManager::add_to_groups(const str& key, const str_set& groups)
{
	str_set current_groups = index.get_set(key);
	current_groups.insert(groups.begin(), groups.end());
	index.set_from(key, current_groups);
}

/**
 * Delete all facts, or a single fact from a keyword.
 * @param key
 * @param line Line number of (multi-line) fact to delete (noline -> delete all lines)
 * @param groups
 * @return
 */
bool FactoidManager::del_fact(const str& key, uns line, const str_set& groups)
{
	bool kill = groups.empty();

	if(!kill)
	{
		str_set current_groups = index.get_set(key);

		for(auto&& g: current_groups)
			if(groups.find(g) != groups.end())
				{ kill = true; break; }
	}

	if(!kill)
	{
		error = "fact not found within specified group(s)";
		return false;
	}

	str_vec tmps = store.get_vec(key);

	if(line == noline)
		tmps.clear();
	else if(line <= tmps.size())
		tmps.erase(tmps.begin() + line - 1);
	else
	{
		error = "line number for key '" + key + "' does not exist: " + std::to_string(line);
		return false;
	}
	store.clear(key);

	if(tmps.empty())
		index.clear(key);
	else
		store.set_from(key, tmps);

	return true;
}

/**
 * Remove keyword from groups.
 * @param key
 * @param groups
 * @return
 */
void FactoidManager::del_from_groups(const str& key, const str_set& groups)
{
	str_set current_groups = index.get_set(key);
	for(auto&& g: groups)
		current_groups.erase(current_groups.find(g));
	index.set_from(key, current_groups);
}

/**
 * Get a set of keywords that match the wildcard expression
 * @return
 */
str_set FactoidManager::find_fact(const str& wild_key, const str_set& groups)
{
	str_set wild_keys = store.get_keys_if_wild(wild_key);

	if(groups.empty())
		return wild_keys;

	str_set ci_groups;
	for(auto const& g: groups)
		ci_groups.insert(hol::lower_copy(g));

	str_set keys;

	for(auto&& k: wild_keys)
	{
		for(auto&& g: index.get_set(k))
			if(ci_groups.find(hol::lower_copy(g)) != groups.end())
				{ keys.insert(k); break; }
	}

	return keys;
}

bool wild_match(const str& w, const str& s, int flags = 0)
{
	return !fnmatch(w.c_str(), s.c_str(), flags | FNM_EXTMATCH);
}

/**
 * Get a set of groups that match the wildcard expression
 * @return
 */
str_set FactoidManager::find_group(const str& wild_group)
{
	str_set groups;

	str_set keys = index.get_keys();

	for(auto&& k: keys)
		for(auto&& g: index.get_set(k))
			if(wild_match(wild_group, g, FNM_CASEFOLD))
				groups.insert(g);

	return groups;
}

template<typename Container1, typename Container2>
bool group_intersect(Container1&& group1, Container2&& group2)
{
	for(auto&& g1: group1)
		for(auto&& g2: group2)
			if(hol::lower_copy(g1) == hol::lower_copy(g2))
				return true;
	return false;
}

str_vec FactoidManager::get_fact(const str& key, const str_set& groups)
{
	if(groups.empty() || group_intersect(groups, index.get_vec(key)))
		return store.get_vec(key);
	return {};
}

FactoidIrcBotPlugin::FactoidIrcBotPlugin(IrcBot& bot)
: BasicIrcBotPlugin(bot)
, store(bot.get(STORE_FILE, STORE_FILE_DEFAULT))
, fm(db_filename("default", "store"), db_filename("default", "index"))
{
}

str FactoidIrcBotPlugin::db_filename(str const& db, str const& part)
{
	auto base =
		bot.get(DATABASE_FILE_PREFIX_KEY, DATABASE_FILE_PREFIX_DEFAULT)
		+ "-" + db + "-"
		+ bot.get(DATABASE_FILE_SUFFIX_KEY, DATABASE_FILE_SUFFIX_DEFAULT)
		+ "-" + part + ".txt";
	return bot.getf("none", base);
}

FactoidIrcBotPlugin::~FactoidIrcBotPlugin() {}

str FactoidIrcBotPlugin::get_user(const message& msg)
{
	bug_fun();
//	bug_var(chanops);
	// chanops user | msg.userhost

	// TODO:Fix this
//	if(chanops && chanops->api(ChanopsApi::is_userhost_logged_in, {msg.get_userhost()}).empty())
//	{
//		str_vec r = chanops->api(ChanopsApi::get_userhost_username, {msg.get_userhost()});
//		if(!r.empty())
//			return r[0];
//	}

	return msg.get_userhost();
}

bool FactoidIrcBotPlugin::is_user_valid(const message& msg)
{
//	bug_fun();

	// Manual overrides in config file
	for(const str& r: bot.get_vec(FACT_USER))
		if(r == msg.get_userhost())
			return true;
	for(const str& r: bot.get_vec(FACT_WILD_USER))
		if(bot.wild_match(r, msg.get_userhost()))
			return true;
	for(const str& r: bot.get_vec(FACT_SREG_USER))
		if(bot.sreg_match(r, msg.get_userhost()))
			return true;
	// FIXME:
//	if(bot.get(FACT_CHANOPS_USERS, false) && chanops)
//		return !chanops->api(ChanopsApi::is_userhost_logged_in, {msg.get_userhost()}).empty();

	return false;
}

// IRC_BOLD + IRC_COLOR + IRC_Green + "addfact: " + IRC_NORMAL

str get_prefix(const message& msg, const str& color)
{
	str cmd = msg.get_user_cmd();
	if(!cmd.empty())
		cmd.erase(0, 1); // loose the '!'

	return IRC_BOLD + IRC_COLOR + color + cmd + ": " + IRC_NORMAL;
}

str get_prefix(const message& msg)
{
	str_vec const colors
	{
		IRC_Black,
		IRC_Navy_Blue,
		IRC_Green,
		IRC_Red,
		IRC_Brown,
		IRC_Purple,
		IRC_Olive,
		IRC_Yellow,
		IRC_Lime_Green,
		IRC_Teal,
		IRC_Aqua_Light,
		IRC_Royal_Blue,
		IRC_Hot_Pink,
		IRC_Dark_Gray,
		IRC_Light_Gray,
		IRC_White,
	};

	auto hash = std::hash<str>()(msg.get_user_cmd());

	return get_prefix(msg, colors[hash % colors.size()]);
}

bool FactoidIrcBotPlugin::reloadfacts(const message& msg, FactoidManager& fm)
{
	BUG_COMMAND(msg);

	if(!is_user_valid(msg))
		return bot.cmd_error(msg, msg.get_nickname() + " is not authorised to reload facts.");

	if(fm.reload())
		bot.fc_reply(msg, get_prefix(msg, IRC_Green) + " Fact database reloaded.");
	else
	{
		bot.fc_reply(msg, get_prefix(msg, IRC_Red) + " ERROR: reloading fact database.");
	}

	return true;
}

// TODO: make a testable business object for the functionality

bool FactoidIrcBotPlugin::addgroup(const message& msg, FactoidManager& fm)
{
	BUG_COMMAND(msg);

	// !addgroup <key> <group>,<group>
	str key;
	str_set groups;
	siss iss(msg.get_user_params());

	if(!(iss >> key))
		return reply(msg, "expected <key>", true);

	bug_var(key);

	str group;
	while(sgl(iss >> std::ws, group, ','))
		groups.insert(hol::trim_mute(group));

	if(groups.empty())
		return reply(msg, "expected groups <group1>, <group2>, etc...");

	bug_cnt(groups);

	fm.add_to_groups(key, groups);

	reply(msg, "Fact added to group.");

	return true;
}

bool FactoidIrcBotPlugin::addfact(const message& msg, FactoidManager& fm)
{
	BUG_COMMAND(msg);

	// !addfact *([topic1,topic2]) <key> <fact>"

	if(!is_user_valid(msg))
		return bot.cmd_error(msg, msg.get_nickname() + " is not authorised to add facts.");

	str topics, key, fact;
	siss iss(msg.get_user_params());

	// add group
	str_set groups;

	if(!getnested(iss, topics, '[', ']')) // group
		iss.clear();
	else
	{
		str list, group;
		sgl(sgl(siss(topics), list, '['), list, ']');

		bug_var(list);

		siss iss(list);
		while(sgl(iss, group, ','))
			groups.insert(hol::trim_mute(group));
	}

	sgl(iss >> key >> std::ws, fact);
	if(hol::trim_mute(fact).empty())
		return reply(msg, "Empty fact rejected.", true);

	bug_var(key);
	bug_var(fact);
	bug_cnt(groups);

	fm.add_fact(key, fact, groups);

	reply(msg, "Fact added to database.");

	return true;
}

bool FactoidIrcBotPlugin::reply(const message& msg, const str& text, bool error)
{
	const str col = error ? IRC_Red : IRC_Green;
	bot.fc_reply(msg, get_prefix(msg, col) + " " + text);
	return true;
}

bool FactoidIrcBotPlugin::delfact(const message& msg, FactoidManager& fm)
{
	BUG_COMMAND(msg);

	// !delfact *([topic1,topic2]) <key> ?(#<idx>)"

	if(!is_user_valid(msg))
		return bot.cmd_error(msg, msg.get_nickname() + " is not authorised to edit facts.");

	str topics, key, idx;
	siss iss(msg.get_user_params());

	// add group
	str_set groups;

	if(!getnested(iss, topics, '[', ']')) // group
		iss.clear();
	else
	{
		str list, group;
		sgl(sgl(siss(topics), list, '['), list, ']');

		bug_var(list);

		siss iss(list);
		while(sgl(iss, group, ','))
			groups.insert(hol::trim_mute(group));
	}

	if(!(iss >> key))
	{
		LOG::E << "parameters needed";
		return true;
	}

	uns line_number = FactoidManager::noline; // -1 == all lines

	iss >> idx; // #n where n > 0
	if(!idx.empty() && (idx[0] != '#' || !(siss(idx).ignore() >> line_number) || !line_number))
	{
		reply(msg, "bad line number: " + idx);
		return true;
	}

	bug_var(key);
	bug_var(line_number);
	bug_var(groups.empty());

	if(!fm.del_fact(key, line_number, groups))
		return reply(msg, fm.error, true);

	if(line_number != FactoidManager::noline)
		reply(msg, "Fact removed.");
	else
		reply(msg, "Line " + std::to_string(line_number - 1) + " removed.");

	return true;
}

bool FactoidIrcBotPlugin::findfact(const message& msg, FactoidManager& fm)
{
	BUG_COMMAND(msg);

	// !findfact *([group1,group2]) <wildcard>"

	siss iss(msg.get_user_params());

	str_set groups;
	if(iss.peek() == '[') // groups
	{
		iss.ignore();
		str list; // groups
		sgl(iss, list, ']');
		siss iss(list);
		str group;
		while(sgl(iss, group, ','))
		{
			bug_var(group);
			groups.insert(hol::trim_mute(group));
		}
	}

	str key_match;
	sgl(iss, key_match);

	if(hol::trim_mute(key_match).empty())
		return reply(msg, "Empty key rejected.");

	bug_var(key_match);

	str_set keys = fm.find_fact(key_match, groups);

	if(keys.empty())
		return reply(msg, "No results.", true);

	if(keys.size() > 20)
	{
		uns max = bot.get("factoid.max.results", 20U);
		reply(msg, "Too many results, printing the first " + std::to_string(max) + ".");
		keys.erase(std::next(keys.begin(), max));
	}

	str line, sep;
	for(const str& key: keys) // TODO: filter out aliases here ?
		{ line += sep + "'" + key + "'"; sep = ", "; }

	reply(msg, line);

	return true;
}

// item1, item2 , item3,item4 -> str_vec{item1,item2,item3,item4}
str_vec split_list(const str& list, char delim = ',')
{
	str_vec split;

	siss iss (list);
	str item;
	while(sgl(iss, item, delim))
		split.push_back(hol::trim_mute(item));

	return split;
}

bool FactoidIrcBotPlugin::findgroup(const message& msg, FactoidManager& fm)
{
	BUG_COMMAND(msg);

	//  !fg <wildcard>

	siss iss(msg.get_user_params());

	str wild_group;
	if(!sgl(iss, wild_group) || hol::trim_mute(wild_group).empty())
		return reply(msg, "expected wildcard group expression", true);

	str_set groups = fm.find_group(wild_group);

	if(groups.empty())
		return reply(msg, "No results.", true);

	if(groups.size() > 20)
	{
		uns max = bot.get("factoid.max.results", 20U);
		reply(msg, "Too many results, printing the first " + std::to_string(max) + ".");
		groups.erase(std::next(groups.begin(), max));
	}

	str line, sep;
	for(const str& key: groups)
		{ line += sep + "'" + key + "'"; sep = ", "; }

	reply(msg, line);

	return true;
}

str_vec chain_list(const str_set& v, const str& sep, siz max = 0)
{
	str line, s;
	str_vec lines;

	for(const str& i: v)
	{
		line += s + i;
		s = sep;
		if(max && line.size() > max)
		{
			lines.push_back(line);
			line.clear();
			s.clear();
		}
	}

	if(!line.empty())
		lines.push_back(line);

	return lines;
}

str_set get_topics_from_fact(const str& fact)
{
	str topic;
	str_set topics;

	std::istringstream iss(fact);
	std::getline(iss, topic, '[');
	std::getline(iss, topic, ']');

	iss.clear();
	iss.str(topic);
	while(std::getline(iss, topic, ','))
		topics.insert(topic);

	return topics;
}

bool FactoidIrcBotPlugin::fact(const message& msg, FactoidManager& fm, const str& key, const str_set& groups, const str& prefix)
{
	BUG_COMMAND(msg);

	const str_vec facts = fm.get_fact(key, groups);

	if(facts.empty())
	{
		if(groups.empty())
			return bot.cmd_error(msg, "No facts associated with key: " + key);
		else
			return bot.cmd_error(msg, "No facts associated with key: " + key + " for those groups.");
	}

	// !fact *[group1,group2] <key>
	siz c = 0;
	for(auto&& fact: facts)
	{
		if(!fact.empty() && fact[0] == '=')
		{
			// follow a fact alias link: <fact2>: = <fact1>
			str key;
			sgl(siss(fact).ignore() >> std::ws, key);
			this->fact(msg, fm, key, groups, prefix);
		}
		else
		{
			// Max of 2 lines in channel
			uns max_lines = bot.get("factoid.max.lines", 2U);
			if(c < max_lines)
				bot.fc_reply(msg, prefix + IRC_BOLD + IRC_COLOR + IRC_Navy_Blue + fact);
			else
			{
				if(c == max_lines)
					bot.fc_reply(msg, prefix + IRC_BOLD + IRC_COLOR + IRC_Navy_Blue +
						"...additional lines sent to PM.");
				bot.fc_reply_pm(msg, prefix + IRC_BOLD + IRC_COLOR + IRC_Navy_Blue + fact);
			}
		}
		++c;
	}

	return true;
}

//str extract_group_list(str const& params)
//{
//	// stuff [g1, g2, g3] other stuff
//	str s;
//	sgl(sgl(siss(msg.get_user_params()), s, '['), s, ']');
//	return s; // "g1, g2, g3"
//}
//
//str_set extract_groups()

template<typename Container, typename T = typename Container::value_type>
std::set<T> to_set(Container const& c)
{
	return{c.begin(), c.end()};
}

bool FactoidIrcBotPlugin::fact(const message& msg, FactoidManager& fm)
{
	BUG_COMMAND(msg);

	// !fact [<group1>(,<group2>)*]? <key>
	// (\[\w+(,\s*\w+)*\]\s+)?(\w+)

	str_set groups;
	str key = hol::trim_copy(msg.get_user_params());

	if(auto rv = extract_delimited_text(key, "[", "]", 0))
	{
		groups = to_set(hol::split_copy(hol::trim_mute(rv.text), ","));
		key = hol::trim_copy(key.substr(rv.pos));
	}

	if(key.empty())
		return bot.cmd_error(msg, "Expected: " + msg.get_user_cmd() + " [<group1>(,<group2>)*]? <key>");
	else
		fact(msg, fm, key, groups, get_prefix(msg, IRC_Aqua_Light));
//		fact(msg, fm, hol::lower_mute(key), groups, get_prefix(msg, IRC_Aqua_Light));

	return true;
}

bool FactoidIrcBotPlugin::give(const message& msg, FactoidManager& fm)
{
	BUG_COMMAND(msg);

	// !give <nick> [<group1>(,<group2>)*]? <key>

	siss iss(msg.get_user_params());

	str nick;
	if(!(iss >> nick >> std::ws))
		return bot.cmd_error(msg, "Expected: " + msg.get_user_cmd() +  " <nick> [<group1>(,<group2>)*]? <key>");

	str_set groups;
	if(iss.peek() == '[') // groups
	{
		iss.ignore();
		str list; // groups
		sgl(iss, list, ']');
		siss iss(list);
		str group;
		while(sgl(iss, group, ','))
		{
			bug_var(group);
			groups.insert(hol::trim_mute(group));
		}
	}

	str key;
	if(!sgl(iss >> std::ws, key) || hol::trim_mute(key).empty())
		return bot.cmd_error(msg, "Expected: !give <nick> *[group1, group2] <key>.");

	fact(msg, fm, hol::lower_mute(key), groups, nick + ": " + irc::IRC_BOLD + "(" + key + ") - " + irc::IRC_NORMAL);

	return true;
}

str_vec FactoidIrcBotPlugin::list_databases()
{
	fs::directory_iterator dir{bot.get_data_folder()};
	fs::directory_iterator end;

	std::regex e{"(?:" + db_filename(")(.*?)(?:", "store") + ")"};

	str s;
	str_vec v;
	std::smatch m;
	for(; dir != end; ++dir)
	{
		s = dir->path().string();
		if(!std::regex_match(s, m, e))
			continue;

		v.push_back(m.str(1));
	}

	std::sort(v.begin(), v.end());

	return v;
}

bool FactoidIrcBotPlugin::lsdb(const message& msg)
{
	BUG_COMMAND(msg);

	// !lsdb

	if(!is_user_valid(msg))
		return bot.cmd_error(msg, msg.get_nickname() + " is not authorised to see databases.");

	auto dbs = list_databases();

	bot.fc_reply_pm(msg, get_prefix(msg) + IRC_COLOR + IRC_Black
		+ "Found " + std::to_string(dbs.size()) + " databases");

	auto i = 0U;
	for(auto const& db: dbs)
		bot.fc_reply_pm(msg, get_prefix(msg)
			+ IRC_BOLD + IRC_COLOR + IRC_Black + std::to_string(++i) + IRC_NORMAL + ": "
			+ IRC_COLOR + IRC_Dark_Gray + db);

	return true;
}

bool FactoidIrcBotPlugin::validate_db_name(const message& msg, str const& db)
{
	bug_fun();
	bug_var(db);
	if(std::count_if(db.begin(), db.end(), [](char c){return !std::isalnum(c);}))
		return bot.cmd_error(msg, "name: '" + db + "' must contain only alphanumeric charscters");

	if(std::isdigit(db[0]))
		return bot.cmd_error(msg, "name: '" + db + "' must not begin with a digit");

	return true;
}

bool FactoidIrcBotPlugin::mkdb(const message& msg)
{
	BUG_COMMAND(msg);

	// !mkdb <db-name> [<title>]?

	if(!is_user_valid(msg))
		return bot.cmd_error(msg, msg.get_nickname() + " is not authorised to create databases.");

	str db;
	str title;

	sgl(siss(msg.get_user_params()) >> db >> std::ws, title);

	hol::lower_mute(db);
	hol::trim_mute(title);

	if(db.empty())
		return bot.cmd_error(msg, msg.get_user_cmd() + " needs a name");


	{
		auto dbs = list_databases();
		if(std::find(dbs.begin(), dbs.end(), db) != dbs.end())
			return bot.cmd_error(msg, "name: '" + db + "' already exists");
	}

	// validate name

	if(!validate_db_name(msg, db))
		return false;

	try
	{
		auto store_file = db_filename(db, "store");
		auto index_file = db_filename(db, "index");

		FactoidManager fm{store_file, index_file};

		if(!title.empty())
			fm.set_metadata(METADATA_TITLE_KEY, title);

		fm.set_metadata(METADATA_CREATED_KEY, hol::time_stamp_now());
		fm.set_metadata(METADATA_UPDATED_KEY, hol::time_stamp_now());
	}
	catch(std::exception const& e)
	{
		return bot.cmd_error(msg, "Error creating db '" + db + "': " + e.what());
	}

	return true;
}

bool FactoidIrcBotPlugin::mvdb(const message& msg)
{
	BUG_COMMAND(msg);

	// !mvdb <db-name|#number> <new-db-name>

	if(!is_user_valid(msg))
		return bot.cmd_error(msg, msg.get_nickname() + " is not authorised to rename databases.");

	str db;
	str new_db;

	siss(msg.get_user_params()) >> db >> new_db;

	hol::lower_mute(db);
	hol::lower_mute(new_db);

	bug_var(db);
	bug_var(new_db);

	if(db.empty())
		return bot.cmd_error(msg, msg.get_user_cmd() + " needs the name of #number of a database to rename");

	if(new_db.empty())
		return bot.cmd_error(msg, msg.get_user_cmd() + " needs a new name for database '" + db + "'");

	{
		auto dbs = list_databases();
		if(std::find(dbs.begin(), dbs.end(), db) != dbs.end())
			return bot.cmd_error(msg, "name: '" + new_db + "' already exists");
	}

	if(!validate_db_name(msg, new_db))
		return false;


	// remember which channels were pointing to the current db name
	str_set chans;
	for(auto const& dbp: dbs)
		if(dbp.second == db)
			chans.insert(dbp.first);

	bug_cnt(chans);

	// drop current name for db

	fms.erase(db);

	std::error_code ec;

	// first copy BOTH strre & index files

	fs::copy(db_filename(db, "store"), db_filename(new_db, "store"), ec);

	if(!ec)
		return bot.cmd_error(msg, "error renaming db from: '" + db + "': " + ec.message());

	fs::copy(db_filename(db, "index"), db_filename(new_db, "index"), ec);

	if(!ec)
		return bot.cmd_error(msg, "error renaming db from: '" + db + "': " + ec.message());

	// Now delete both

	fs::remove(db_filename(db, "store"), ec);

	if(!ec)
		return bot.cmd_error(msg, "error renaming db to: '" + new_db + "': " + ec.message());

	fs::remove(db_filename(db, "index"), ec);

	if(!ec)
		return bot.cmd_error(msg, "error renaming db to: '" + new_db + "': " + ec.message());

	// TODO: reload under new name (add to persistent store override)

	for(auto const& chan: chans)
		load_db(new_db, chan);

	return true;
}

bool FactoidIrcBotPlugin::rmdb(const message& msg)
{
	BUG_COMMAND(msg);

	if(!is_user_valid(msg))
		return bot.cmd_error(msg, msg.get_nickname() + " is not authorised to remove databases.");

	return true;
}

bool FactoidIrcBotPlugin::cdb(const message& msg)
{
	BUG_COMMAND(msg);

	if(!is_user_valid(msg))
		return bot.cmd_error(msg, msg.get_nickname() + " is not authorised to change databases.");

	return true;
}

FactoidManager& FactoidIrcBotPlugin::select_fm(const message& msg)
{
	BUG_COMMAND(msg);

	auto fount_db = dbs.find(hol::lower_copy(msg.get_chan()));

	if(fount_db == dbs.end())
		return this->fm;

	bug_var(fount_db->first);
	bug_var(fount_db->second);

	auto found_fm = fms.find(fount_db->second);

	if(found_fm == fms.end())
		return this->fm;

	bug_var(found_fm->first);

	return found_fm->second;
}

bool FactoidIrcBotPlugin::load_db(str const& db, str const& chan)
{
	bug_fun();
	bug_var(db);
	bug_var(chan);
	bug("-----------------------------------------------------");
	dbs.erase(chan);
	fms.erase(db);

	auto store_file = db_filename(db, "store");
	auto index_file = db_filename(db, "index");

	bug_var(store_file);
	bug_var(index_file);

	try
	{
		fms.emplace(std::piecewise_construct,
				std::forward_as_tuple(db),
				std::forward_as_tuple(store_file, index_file));

		dbs.emplace(std::piecewise_construct,
				std::forward_as_tuple(chan),
				std::forward_as_tuple(db));

		LOG::I << "DB: " << db << " loaded for channel: " << chan;

		store.set(FACT_CHANNEL_DATABASE, chan + " " + db);
	}
	catch(std::exception const& e)
	{
		LOG::E << "Unable to load factoid database: " << db;
		return false;
	}
	return true;
}

// INTERFACE: BasicIrcBotPlugin

bool FactoidIrcBotPlugin::initialize()
{
//	store.case_insensitive(true);

	//	factoid.channel.db: #skivvy skivvy
	//	factoid.channel.db: #skivvy autotools
	//	factoid.channel.db: #autotools autotools
	//	factoid.channel.db: #CppCoreGuidelines cpp
	//	factoid.channel.db: ##CppCoreGuidelines cpp

	// current config
	LOG::I << "Loading persistent database config";
	for(auto const& cdb: store.get_vec(FACT_CHANNEL_DATABASE))
	{
		bug_var(cdb);
		str chan, db;
		if(!(std::stringstream(cdb) >> chan >> db))
		{
			LOG::W << "Duplicate channel database entry in config: " << chan;
			continue;
		}

		auto found = dbs.find(hol::trim_mute(chan));

		if(found != dbs.end())
		{
			LOG::E << "Duplicate channel database entry in store: " << chan
				<< ": " << db;
			continue;
		}

		load_db(db, chan);
	}

	LOG::I << "Loading disk based database config";
	// load from hard config hat does not conflict
	for(auto const& cdb: bot.get_vec(FACT_CHANNEL_DATABASE))
	{
		str chan, db;
		std::stringstream(cdb) >> chan >> db;

		hol::lower_mute(hol::trim_mute(db));
		hol::lower_mute(hol::trim_mute(chan));

		auto found = dbs.find(hol::trim_mute(chan));

		if(found != dbs.end() && found->second != db)
		{
			LOG::W << "Runtime config overrides config file for channel: " << chan
				<< ": using db" << found->second;
			continue;
		}

		load_db(db, chan);
	}

	add
	({
		"!lsdb"
		, "List available databases by their #number (numbers change when dbs are added and removed)."
		, [&](const message& msg){ lsdb(msg); }
	});
	add
	({
		"!mkdb"
		, "<db-name> - Create new databases."
		, [&](const message& msg){ mkdb(msg); }
	});
	add
	({
		"!mvdb"
		, "<db-name|#number> <new-db-name> - Rename database from <db-name> (or number from !lsdb) to <new-db-name>."
		, [&](const message& msg){ mkdb(msg); }
	});
	add
	({
		"!cdb"
		, "<db-name> - Change database."
		, [&](const message& msg){ cdb(msg); }
	});
	add
	({
		"!addfact"
		, "[<group1>(,<group2>)*]? <key> \"<fact>\" - Add a key fact optionally restricted by group(s)."
		, [&](const message& msg){ addfact(msg, select_fm(msg)); }
	});
	add
	({
		"!delfact"
		, "[<group1>(,<group2>)*]? <key> ?(#n) - Delete fact or single line from fact."
		, [&](const message& msg){ delfact(msg, select_fm(msg)); }
	});
	add
	({
		"!addgroup"
		, "<key> <group1>(,<group2>)* - Add key to groups."
		, [&](const message& msg){ addgroup(msg, select_fm(msg)); }
	});
	add
	({
		"!findfact"
		, "[<group1>(,<group2>)*]? <wildcard> - Get a list of matching fact keys."
		, [&](const message& msg){ findfact(msg, select_fm(msg)); }
	});
	add
	({
		"!ff"
		, "=!findfact"
		, [&](const message& msg){ findfact(msg, select_fm(msg)); }
	});
	add
	({
		"!findgroup"
		, "<wildcard> - Get a list of matching groups."
		, [&](const message& msg){ findgroup(msg, select_fm(msg)); }
	});
	add
	({
		"!fg"
		, "=!findgroup"
		, [&](const message& msg){ findgroup(msg, select_fm(msg)); }
	});
	add
	({
		"!fact"
		, "[<group1>(,<group2>)*]? <key> - Display key fact."
		, [&](const message& msg){ fact(msg, select_fm(msg)); }
	});
	add
	({
		"!f"
		, "=!fact"
		, [&](const message& msg){ fact(msg, select_fm(msg)); }
	});
	add
	({
		"!give"
		, "<nick> [<group1>(,<group2>)*]? <key> - Display (optionally group restricted) fact highlighting <nick>."
		, [&](const message& msg){ give(msg, select_fm(msg)); }
	});
	add
	({
		"!reloadfacts"
		, "Reload fact database."
		, [&](const message& msg){ reloadfacts(msg, select_fm(msg)); }
	});

	return true;
}

// INTERFACE: IrcBotPlugin

str FactoidIrcBotPlugin::get_id() const { return ID; }
str FactoidIrcBotPlugin::get_name() const { return NAME; }
str FactoidIrcBotPlugin::get_version() const { return VERSION; }

void FactoidIrcBotPlugin::exit()
{
//	bug_fun();
}

// INTERFACE: IrcBotMonitor

} // factoid
} // ircbot
} // skivvy
