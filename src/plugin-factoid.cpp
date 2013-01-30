/*
 * ircbot-grabber.cpp
 *
 *  Created on: 30 Jan 2012
 *      Author: oaskivvy@gmail.com
 */

/*-----------------------------------------------------------------.
| Copyright (C) 2011 SooKee oaskivvy@gmail.com               |
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

#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>

#include <skivvy/logrep.h>
#include <skivvy/str.h>
#include <skivvy/ios.h>
#include <skivvy/irc.h>

namespace skivvy { namespace ircbot {

IRC_BOT_PLUGIN(FactoidIrcBotPlugin);
PLUGIN_INFO("factoid", "Factoid", "0.1");

using namespace skivvy;
using namespace skivvy::irc;
using namespace skivvy::types;
using namespace skivvy::utils;
using namespace skivvy::string;

const str STORE_FILE = "factoid.store.file";
const str STORE_FILE_DEFAULT = "factoid-store.txt";
const str INDEX_FILE = "factoid.index.file";
const str INDEX_FILE_DEFAULT = "factoid-index.txt";
const str FACT_USER = "factoid.fact.user";

FactoidIrcBotPlugin::FactoidIrcBotPlugin(IrcBot& bot)
: BasicIrcBotPlugin(bot)
, store(bot.getf(STORE_FILE, STORE_FILE_DEFAULT))
, index(bot.getf(INDEX_FILE, INDEX_FILE_DEFAULT))
{
}

FactoidIrcBotPlugin::~FactoidIrcBotPlugin() {}

bool FactoidIrcBotPlugin::is_user_valid(const message& msg, const str& svar)
{
	bug_func();
	for(const str& r: bot.get_vec(svar))
		if(bot.preg_match(r, msg.get_userhost()))
			return true;
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

bool FactoidIrcBotPlugin::reloadfacts(const message& msg)
{
	BUG_COMMAND(msg);

	if(!is_user_valid(msg, FACT_USER))
		return bot.cmd_error(msg, msg.get_nick() + " is not authorised to reload facts.");

	store.reload();
	bot.fc_reply(msg, get_prefix(msg, IRC_Red) + " Fact database reloaded.");

	return true;
}

bool FactoidIrcBotPlugin::addgroup(const message& msg)
{
	BUG_COMMAND(msg);

	// !addgroup <key> <group>,<group>
	str_set groups;
	str key, list, group;
	siss iss(msg.get_user_params());

	ios::getstring(iss, key) >> std::ws;

	while(sgl(iss, group, ','))
		groups.insert(trim(group));

	iss.clear();
	iss.str(index.get(lowercase(key)));
	while(sgl(iss, group, ','))
		groups.insert(group);

	str sep;
	list.clear();
	for(const str& group: groups)
		{ list += sep + group; sep = ","; }

	index.set(lowercase(key), list);
	bot.fc_reply(msg, get_prefix(msg, IRC_Green) + " Fact added to group.");

	return true;
}

bool FactoidIrcBotPlugin::addfact(const message& msg)
{
	BUG_COMMAND(msg);

	// !addfact *([topic1,topic2]) <key> <fact>"

	if(!is_user_valid(msg, FACT_USER))
		return bot.cmd_error(msg, msg.get_nick() + " is not authorised to add facts.");

	str_vec param(3);
	siss iss(msg.get_user_params());
	ios::getstring(iss, param[0]);
	ios::getstring(iss, param[1]);
	ios::getstring(iss, param[2]);

	if(!param[0].empty() && param[0][0] == '[') // group
	{
		// add group
		str_set groups;
		str list, group;
		sgl(sgl(siss(param[0]), list, '['), list, ']');

		while(sgl(siss(list), group, ','))
			groups.insert(trim(group));
		while(sgl(siss(index.get(lowercase(param[1]))), group, ','))
			groups.insert(group);

		str sep;
		list.clear();
		for(const str& group: groups)
			{ list += sep + group; sep = ","; }

		index.set(lowercase(param[1]), list);
		param.erase(param.begin()); // eat the optional parameter
	}

	store.add(lowercase(param[0]), param[1]);
	bot.fc_reply(msg, get_prefix(msg, IRC_Green) + " Fact added to database.");

	return true;
}

bool FactoidIrcBotPlugin::findfact(const message& msg)
{
	BUG_COMMAND(msg);

	// !findfact *([group1,group2]) <wildcard>"


	siss iss(msg.get_user_params());

	str_set groups;
	if(iss.peek() == '[') // groups
	{
		str list; // groups
		sgl(sgl(iss, list, '['), list, ']');
//		sgl(sgl(iss, list, '['), list, ']');
		siss iss(list);
		str group;
		while(sgl(iss, group, ','))
		{
			bug_var(group);
			groups.insert(trim(group));
		}
	}

	str key_match;
	ios::getstring(iss, key_match);
	bug_var(key_match);

	str_set keys = store.get_keys_if_wild(lowercase(key_match));

	if(!groups.empty()) // restrict search to these groups
	{
		for(auto ki = keys.cbegin(); ki != keys.cend();)
//		for(const str& key: keys)
		{
			siss iss(index.get(*ki));
			str group;
			siz c = 0;
			while(sgl(iss, group, ','))
			{
				bug_var(group);
				if(groups.find(group) != groups.end())
					++c;
			}
			bug_var(c);
			if(c)
				++ki;
			else
				ki = keys.erase(ki);
		}
	}

	if(keys.empty())
		bot.fc_reply(msg, get_prefix(msg, IRC_Dark_Gray) + "No results.");
	else
	{
		if(keys.size() > 20)
			bot.fc_reply(msg, get_prefix(msg, IRC_Dark_Gray) + "Too many results, printing the first 20.");

		str line, sep;
		siz c = 20;
		for(const str& key: keys) // TODO: filter out aliases here ?
			{ line += sep + "'" + key + "'"; sep = ", "; if(!--c) break; }
		bot.fc_reply(msg, get_prefix(msg, IRC_Dark_Gray) + line);
	}

	return true;
}

// item1, item2 , item3,item4 -> str_vec{item1,item2,item3,item4}
str_vec split_list(const str& list, char delim = ',')
{
	str_vec split;

	siss iss (list);
	str item;
	while(sgl(iss, item, delim))
		split.push_back(trim(item));

	return split;
}

bool FactoidIrcBotPlugin::findgroup(const message& msg)
{
	BUG_COMMAND(msg);

	//  !fg <wildcard>

	str_set groups;
	str_set keys = index.get_keys();

	for(const str& key: keys)
	{
		siss iss(index.get(key));
		str group;
		while(sgl(iss, group, ','))
			if(bot.wild_match(msg.get_user_params(), group))
				groups.insert(group);
	}


	if(groups.empty())
		bot.fc_reply(msg, get_prefix(msg, IRC_Dark_Gray) + "No results.");
	else
	{
		if(groups.size() > 20)
			bot.fc_reply(msg, get_prefix(msg, IRC_Dark_Gray) + "Too many results, printing the first 20.");

		str line, sep;
		siz c = 20;
		for(const str& group: groups)
			{ line += sep + group; sep = ", "; if(!--c) break; }
		bot.fc_reply(msg, get_prefix(msg, IRC_Dark_Gray) + line);
	}

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

bool FactoidIrcBotPlugin::fact(const message& msg, const str& key, const str& prefix)
{
	BUG_COMMAND(msg);

	// !fact <key>
	const str_vec facts = store.get_vec(lowercase(key));
	siz c = 0;
	for(const str& fact: facts)
	{
		if(!fact.empty() && fact[0] == '=')
		{
			// follow a fact alias link: <fact2>: = <fact1>
			str key;
			std::getline(std::istringstream(fact).ignore() >> std::ws, key);
			this->fact(msg, key, prefix);
		}
		else
		{
			// Max of 2 lines in channel
			if(c < 2)
				bot.fc_reply(msg, prefix + IRC_BOLD + IRC_COLOR + IRC_Navy_Blue + fact);
			else
				bot.fc_reply_pm(msg, prefix + IRC_BOLD + IRC_COLOR + IRC_Navy_Blue + fact);
		}
		++c;
	}

	return true;
}

bool FactoidIrcBotPlugin::fact(const message& msg)
{
	BUG_COMMAND(msg);

	// !fact <key>
	str key = msg.get_user_params();
	if(trim(key).empty())
		return bot.cmd_error(msg, "Expected: !fact <key>.");
	fact(msg, key);

	return true;
}

bool FactoidIrcBotPlugin::give(const message& msg)
{
	BUG_COMMAND(msg);

	// !give <nick> <key>
	str nick, key;
	if(!(std::getline(std::istringstream(msg.get_user_params()) >> nick >> std::ws, key)))
		return bot.cmd_error(msg, "Expected: !give <nick> <key>.");

	fact(msg, key, nick + ": " + irc::IRC_BOLD + "(" + key + ") - " + irc::IRC_NORMAL);

	return true;
}

// INTERFACE: BasicIrcBotPlugin

bool FactoidIrcBotPlugin::initialize()
{
	add
	({
		"!addfact"
		, "!addfact <key> \"<fact>\" - Display key fact."
		, [&](const message& msg){ addfact(msg); }
	});
	add
	({
		"!addgroup"
		, "!addgroup <key> <group>,<group> - Add key to groups."
		, [&](const message& msg){ addgroup(msg); }
	});
	add
	({
		"!findfact"
		, "!findfact *([<group1>,<group2>]) <wildcard> - Get a list of matching fact keys."
		, [&](const message& msg){ findfact(msg); }
	});
	add
	({
		"!ff"
		, "!ff - alias for !findfact."
		, [&](const message& msg){ findfact(msg); }
	});
	add
	({
		"!findgroup"
		, "!findgroup <wildcard> - Get a list of matching groups."
		, [&](const message& msg){ findgroup(msg); }
	});
	add
	({
		"!fg"
		, "!fg - alias for !findgroup."
		, [&](const message& msg){ findgroup(msg); }
	});
	add
	({
		"!fact"
		, "!fact <key> - Display key fact."
		, [&](const message& msg){ fact(msg); }
	});
	add
	({
		"!give"
		, "!give <nick> <key> - Display fact highlighting <nick>."
		, [&](const message& msg){ give(msg); }
	});
	add
	({
		"!reloadfacts"
		, "!reloadfacts - Reload fact database."
		, [&](const message& msg){ reloadfacts(msg); }
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
//	bug_func();
}

// INTERFACE: IrcBotMonitor

}} // skivvy::ircbot
