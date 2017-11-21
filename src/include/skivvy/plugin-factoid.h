#ifndef SKIVVY_IRCBOT_FACTOID_H
#define SKIVVY_IRCBOT_FACTOID_H
/*
 * plugin-factoid.h
 *
 *  Created on: 04 Jan 2013
 *      Author: oaskivvy@gmail.com
 */

/*-----------------------------------------------------------------.
| Copyright (C) 2013 SooKee oaskivvy@gmail.com                     |
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
#include <skivvy/plugin-chanops.h>

#include <hol/small_types.h>

namespace skivvy {
namespace ircbot {
namespace factoid {

using namespace skivvy::utils;
using namespace skivvy::ircbot;
using namespace header_only_library::small_types::basic;
using namespace header_only_library::small_types::string_containers;

// !addfact, !addgroup, !delfact, !fact, !ff, !fg, !findfact, !findgroup, !give, !reloadfacts

class FactoidManager
{
public:
	using RPtr = FactoidManager*;
	using UPtr = std::unique_ptr<FactoidManager>;
	using SPtr = std::shared_ptr<FactoidManager>;

	static const uns noline = uns(-1);

	str error;

	FactoidManager(FactoidManager&& fm)
	: store(std::move(fm.store)), index(std::move(fm.index)) {}

	FactoidManager(const str& store_file, const str& index_file);

	bool reload()
	{
		store.reload();
		index.reload();
		return true;
	}

	void set_metadata(const str& key, const str& val);
	str get_metadata(const str& key, const str& dflt);

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

private:
	str name;
	BackupStore store;
	BackupStore index;
};

class FactoidIrcBotPlugin
: public BasicIrcBotPlugin
{
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

private:

//	IrcBotPluginHandle chanops;
	BackupStore store; // persistant config (overrides config)

	FactoidManager fm;
	std::map<str, FactoidManager> fms; // database -> FactoidManager
	std::map<str, str> dbs; // channel -> database

	str db_filename(str const& db, str const& part);
	str_vec list_databases();
	bool validate_db_name(const message& msg, str const& db);
	bool load_db(str const& db, str const& chan);
	FactoidManager& select_fm(const message& msg);

	bool lsdb(const message& msg);
	bool mkdb(const message& msg);
	bool mvdb(const message& msg);
	bool rmdb(const message& msg);
	bool  cdb(const message& msg);

	str get_user(const message& msg);

	bool is_user_valid(const message& msg);

	bool reloadfacts(const message& msg, FactoidManager& fm);
	bool addgroup(const message& msg, FactoidManager& fm);
	bool addfact(const message& msg, FactoidManager& fm);
	bool delfact(const message& msg, FactoidManager& fm);
//	bool addtopic(const message& msg);

	bool findfact(const message& msg, FactoidManager& fm); // !fs
	bool findgroup(const message& msg, FactoidManager& fm); // !fs

	bool fact(const message& msg, FactoidManager& fm, const str& key, const str_set& groups, const str& prefix = "");
	bool fact(const message& msg, FactoidManager& fm);
	bool give(const message& msg, FactoidManager& fm);

	bool reply(const message& msg, const str& text, bool error = false);
};

} // factoid
} // ircbot
} // skivvy

#endif // SKIVVY_IRCBOT_FACTOID_H
