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

#include <hol/string_utils.h>
#include <hol/small_types.h>
//#include <sookee/ios.h>

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

#define log(m) LOG::I << m

namespace skivvy {
namespace ircbot {
namespace factoid {

IRC_BOT_PLUGIN(FactoidIrcBotPlugin);
PLUGIN_INFO("factoid", "Factoid", "0.1");

using namespace hol::simple_logger;
using namespace skivvy;
using namespace skivvy::irc;
using namespace hol::small_types::basic;
using namespace skivvy::utils;
using namespace skivvy::ircbot;
using namespace skivvy::ircbot::chanops;
//using namespace sookee::ios;

const str STORE_FILE = "factoid.store.file";
const str STORE_FILE_DEFAULT = "factoid-store.txt";

const str INDEX_FILE = "factoid.index.file";
const str INDEX_FILE_DEFAULT = "factoid-index.txt";

const str FACT_USER = "factoid.fact.user";
const str FACT_WILD_USER = "factoid.fact.wild.user";
const str FACT_PREG_USER = "factoid.fact.preg.user";
const str FACT_CHANOPS_USERS = "factoid.fact.chanops.users";

const str FACT_CHANNEL_GROUP = "factoid.channel.group";

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

	str_set keys;

	for(auto&& k: wild_keys)
	{
		for(auto&& g: index.get_set(k))
			if(groups.find(g) != groups.end())
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
			if(wild_match(wild_group, g))
				groups.insert(g);

	return groups;
}

template<typename Container1, typename Container2>
bool group_intersect(Container1&& group1, Container2&& group2)
{
	for(auto&& g1: group1)
		for(auto&& g2: group2)
			if(g1 == g2)
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
//, store(bot.getf(STORE_FILE, STORE_FILE_DEFAULT))
//, index(bot.getf(INDEX_FILE, INDEX_FILE_DEFAULT))
, chanops(bot, "chanops")
, fm(bot.getf(STORE_FILE, STORE_FILE_DEFAULT), bot.getf(INDEX_FILE, INDEX_FILE_DEFAULT))
{
}

FactoidIrcBotPlugin::~FactoidIrcBotPlugin() {}

str FactoidIrcBotPlugin::get_user(const message& msg)
{
	bug_fun();
	bug_var(chanops);
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

	// Manual overrides from config file
	for(const str& r: bot.get_vec(FACT_USER))
		if(r == msg.get_userhost())
			return true;
	for(const str& r: bot.get_vec(FACT_WILD_USER))
		if(bot.wild_match(r, msg.get_userhost()))
			return true;
	for(const str& r: bot.get_vec(FACT_PREG_USER))
		if(bot.preg_match(r, msg.get_userhost()))
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
		log("ERROR: parameters needed");
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

bool FactoidIrcBotPlugin::fact(const message& msg, FactoidManager& fm)
{
	BUG_COMMAND(msg);

	// !fact *([group1,group2]) <wildcard>"

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

	str key;
	sgl(iss, key);

	if(hol::trim_mute(key).empty())
		return bot.cmd_error(msg, "Expected: !fact <key>.");
	else
		fact(msg, fm, hol::lower_mute(key), groups, get_prefix(msg, IRC_Aqua_Light));

	return true;
}

bool FactoidIrcBotPlugin::give(const message& msg, FactoidManager& fm)
{
	BUG_COMMAND(msg);

	// !give <nick> *[group1, group2] <key>

	siss iss(msg.get_user_params());

	str nick;
	if(!(iss >> nick >> std::ws))
		return bot.cmd_error(msg, "Expected: !give <nick> *[group1, group2] <key>.");

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

FactoidManager& FactoidIrcBotPlugin::select_fm(const message& msg)
{
	auto found = fms.find(msg.get_chan());
	return found != fms.end() ? *found->second : this->fm;
}

// INTERFACE: BasicIrcBotPlugin

bool FactoidIrcBotPlugin::initialize()
{
	// {bug: #24} update store to ass user

	str_set unique_check;
	for(auto const& channels: bot.get_vec(FACT_CHANNEL_GROUP))
	{
		siss iss(channels);
		str channel;
		while(iss >> channel)
		{
			if(!unique_check.insert(channel).second)
				continue;

			auto store_root = channel + "-" + bot.get(STORE_FILE, STORE_FILE_DEFAULT);
			auto index_root = channel + "-" + bot.get(INDEX_FILE, INDEX_FILE_DEFAULT);

			auto store_file = bot.getf("none", store_root);
			auto index_file = bot.getf("none", index_root);

			fms[channel] = std::make_shared<FactoidManager>(store_file, index_file);

			auto primary = channel;
			while(iss >> channel)
				fms.emplace(channel, fms[primary]);
		}
	}

	add
	({
		"!addfact"
		, "[<group1>(,<group2>)*]? <key> \"<fact>\" - Display key fact optionally restricted by group(s)."
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
		, "=!findfact."
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
		, "=!findgroup."
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
		, "=!fact."
		, [&](const message& msg){ fact(msg, select_fm(msg)); }
	});
	add
	({
		"!give"
		, "<nick> <key> - Display fact highlighting <nick>."
		, [&](const message& msg){ give(msg, select_fm(msg)); }
	});
	add
	({
		"!reloadfacts"
		, "Reload fact database."
		, [&](const message& msg){ reloadfacts(msg, select_fm(msg)); }
//		, action::INVISIBLE
	});
//	bot.add_monitor(*this);
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
