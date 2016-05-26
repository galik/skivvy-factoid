#pragma once
#ifndef _SKIVVY_IRCBOT_FACTOID_H_
#define _SKIVVY_IRCBOT_FACTOID_H_
/*
 * plugin-factoid.h
 *
 *  Created on: 04 Jan 2013
 *      Author: oaskivvy@gmail.com
 */

/*-----------------------------------------------------------------.
| Copyright (C) 2013 SooKee oaskivvy@gmail.com               |
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

#include <skivvy/ircbot.h>

#include <deque>
#include <mutex>

#include <skivvy/store.h>
//#include <skivvy/plugin-chanops.h>

namespace skivvy { namespace factoid {

using namespace skivvy::utils;
using namespace skivvy::ircbot;

// !addfact, !addgroup, !delfact, !fact, !ff, !fg, !findfact, !findgroup, !give, !reloadfacts

class FactoidManager
{
	BackupStore store;
	BackupStore index;

public:
	static const uns noline = uns(-1);

	str error;

	FactoidManager(const str& store_file, const str& index_file);

	bool reload()
	{
		store.reload();
		index.reload();
		return true;
	}

	/**
	 * Add a fact by keyword and optionally add it to groups.
	 * @param key
	 * @param fact
	 * @param groups
	 * @return
	 */
	void add_fact(const str& key, const str& fact, const str_set& groups = {});

	/**
	 * Delete all facts, or a single fact from a keyword.
	 * @param key
	 * @param line
	 * @param groups
	 * @return
	 */
	bool del_fact(const str& key, uns line = noline, const str_set& groups = {});

	/**
	 * Add keyword to groups.
	 * @param key
	 * @param groups
	 * @return
	 */
	void add_to_groups(const str& key, const str_set& groups);

	/**
	 * Remove keyword from groups.
	 * @param key
	 * @param groups
	 */
	void del_from_groups(const str& key, const str_set& groups);

	/**
	 * Get a set of kewords that match the wildcard expression
	 * @return
	 */
	str_set find_fact(const str& wild_key, const str_set& groups = {});

	/**
	 * Get a set of groups that match the wildcard expression
	 * @return
	 */
	str_set find_group(const str& wild_group);

	/**
	 * Retrieve all facts for key optionally restricted by groups..
	 * @param key The key of the facts to retrieve
	 * @param groups If not empty redtrict fact search to these groups.
	 * @return str_vec of facts
	 */
	str_vec get_fact(const str& key, const str_set& groups);

};

class FactoidIrcBotPlugin
: public BasicIrcBotPlugin
{
public:

private:

	IrcBotPluginHandle chanops;

	FactoidManager fm;

	str get_user(const message& msg);

	bool is_user_valid(const message& msg);

	bool reloadfacts(const message& msg);
	bool addgroup(const message& msg);
	bool addfact(const message& msg);
	bool delfact(const message& msg);
//	bool addtopic(const message& msg);

	bool findfact(const message& msg); // !fs
	bool findgroup(const message& msg); // !fs

	bool fact(const message& msg, const str& key, const str_set& groups, const str& prefix = "");
	bool fact(const message& msg);
	bool give(const message& msg);

	bool reply(const message& msg, const str& text, bool error = false);

public:
	FactoidIrcBotPlugin(IrcBot& bot);
	virtual ~FactoidIrcBotPlugin();

	// INTERFACE: BasicIrcBotPlugin

	bool initialize() override;

	// INTERFACE: IrcBotPlugin

	std::string get_id() const override;
	std::string get_name() const override;
	std::string get_version() const override;
	void exit() override;
};

}} // skivvy::factoid

#endif // _SKIVVY_IRCBOT_FACTOID_H_
