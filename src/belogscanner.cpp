/*
Copyright (C) 2015 Declan Ireland <http://github.com/torndeco/extDB2>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/


#include "belogscanner.h"

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>

#include <Poco/DateTime.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Delegate.h>
#include <Poco/DirectoryWatcher.h>
#include <Poco/FileStream.h>
#include <Poco/NumberParser.h>
#include <Poco/Process.h>
#include <Poco/Util/IniFileConfiguration.h>


BELogScanner::BELogScanner()
{
}


BELogScanner::~BELogScanner(void)
{
	stop();
}


void BELogScanner::start(std::string &bepath, boost::asio::io_service &io_service, std::shared_ptr<spdlog::logger> spdlog)
{
	io_service_ptr = &io_service;
	logger = spdlog;
	be_path = bepath;

	current_dateTime = Poco::DateTime();

	boost::filesystem::path belog_wipe_path(be_path);
	belog_wipe_path /= "belogscanner.shutdown";

	bool clean_shutdown = false;
	if (boost::filesystem::exists(belog_wipe_path))
	{
		clean_shutdown = true;
		std::time_t last_shutdown_timestamp = boost::filesystem::last_write_time(belog_wipe_path);

		boost::filesystem::directory_iterator iter(be_path);
		boost::filesystem::directory_iterator iter_end;

		for (; iter != iter_end; ++iter)
		{
			if (boost::filesystem::is_regular_file(iter->path()))
			{
				if (iter->path().extension().string() == "log")  // TODO Make case insensitive
				{
					if (boost::filesystem::last_write_time(iter->path()) > last_shutdown_timestamp)
					{
						clean_shutdown = false;
						break;
					}
				}
			}
		}
	}

	if (!clean_shutdown)
	{
		logger->warn("BELogscanner: Bad Shutdown Detected");

		boost::filesystem::directory_iterator iter(be_path);
		boost::filesystem::directory_iterator iter_end;

		for (; iter != iter_end; ++iter)
		{
			if (boost::filesystem::is_regular_file(iter->path()))
			{
				if (iter->path().extension().string() == "log")  // TODO Make case insensitive
				{
					boost::filesystem::path dest_file(be_path);
					dest_file /= "BELogscanner";
					dest_file /= "logs";
					dest_file /= Poco::DateTimeFormatter::format(current_dateTime, "%Y");
					dest_file /= Poco::DateTimeFormatter::format(current_dateTime, "%n");
					dest_file /= Poco::DateTimeFormatter::format(current_dateTime, "%d");
					dest_file /= "bad_shutdown";
					dest_file /= iter->path().filename();
					boost::filesystem::copy_file(iter->path(), dest_file);
				}
			}
		}
	}
	boost::filesystem::remove(belog_wipe_path);


	be_custom_log_path = boost::filesystem::path(be_path);
	be_custom_log_path /= "BELogscanner";
	be_custom_log_path /= "logs";
	be_custom_log_path /= Poco::DateTimeFormatter::format(current_dateTime, "%Y");
	be_custom_log_path /= Poco::DateTimeFormatter::format(current_dateTime, "%n");
	be_custom_log_path /= Poco::DateTimeFormatter::format(current_dateTime, "%d");
	boost::filesystem::create_directories(be_custom_log_path);

	filters_path = boost::filesystem::path(be_path);
	filters_path /= "BELogscanner";
	filters_path /= "filters";
	boost::filesystem::create_directories(filters_path);
	loadFilters();

	filters_directory_watcher.reset(new Poco::DirectoryWatcher(filters_path.make_preferred().string(), Poco::DirectoryWatcher::DW_FILTER_ENABLE_ALL, 5));
	filters_directory_watcher->itemAdded += Poco::delegate(this, &BELogScanner::reloadFilters);
	filters_directory_watcher->itemRemoved += Poco::delegate(this, &BELogScanner::reloadFilters);
	filters_directory_watcher->itemModified += Poco::delegate(this, &BELogScanner::reloadFilters);
	filters_directory_watcher->itemMovedFrom += Poco::delegate(this, &BELogScanner::reloadFilters);
	filters_directory_watcher->itemMovedTo += Poco::delegate(this, &BELogScanner::reloadFilters);

	be_directory_watcher.reset(new Poco::DirectoryWatcher(be_path, Poco::DirectoryWatcher::DW_FILTER_ENABLE_ALL, 5));
	be_directory_watcher->itemAdded += Poco::delegate(this, &BELogScanner::onFileAdded);
	be_directory_watcher->itemRemoved += Poco::delegate(this, &BELogScanner::onFileRemoved);
	be_directory_watcher->itemModified += Poco::delegate(this, &BELogScanner::onFileModified);
	be_directory_watcher->itemMovedFrom += Poco::delegate(this, &BELogScanner::onFileMovedFrom);
	be_directory_watcher->itemMovedTo += Poco::delegate(this, &BELogScanner::onFileMovedTo);
}


void BELogScanner::updateAdd(std::string &steam_id, std::string &value)
{
	AddPlayerKey key;
	key.steam_id = steam_id;
	key.value = value;

	std::lock_guard<std::mutex> lock(update_player_key_mutex);
	add_player_key.push_back(key);
}


void BELogScanner::updateRemove(std::string &steam_id, std::string &value)
{
	RemovePlayerKey key;
	key.steam_id = steam_id;
	key.value = value;

	std::lock_guard<std::mutex> lock(update_player_key_mutex);
	remove_player_key.push_back(key);
}


void BELogScanner::stop()
{
	if (be_directory_watcher)
	{
		be_directory_watcher.release();
	}
	if (filters_directory_watcher)
	{
		filters_directory_watcher.release();
	}


	boost::filesystem::directory_iterator iter(be_path);
	boost::filesystem::directory_iterator iter_end;

	for (; iter != iter_end; ++iter)
	{
		if (boost::filesystem::is_regular_file(iter->path()))
		{
			if (iter->path().extension().string() == "log")  // TODO Make case insensitive
			{
				boost::filesystem::path dest_file(be_path);
				dest_file /= "BELogscanner";
				dest_file /= "logs";
				dest_file /= Poco::DateTimeFormatter::format(current_dateTime, "%Y");
				dest_file /= Poco::DateTimeFormatter::format(current_dateTime, "%n");
				dest_file /= Poco::DateTimeFormatter::format(current_dateTime, "%d");
				dest_file /= iter->path().filename();
				boost::filesystem::copy_file(iter->path(), dest_file);
			}
		}
	}

	boost::filesystem::path belog_wipe_path(be_path);
	belog_wipe_path /= "belogscanner.shutdown";
	FILE *shutdown_file;
	shutdown_file = std::fopen(belog_wipe_path.make_preferred().string().c_str(), "w");
	std::fclose(shutdown_file);
}


void BELogScanner::onFileAdded(const Poco::DirectoryWatcher::DirectoryEvent& event)
{
	std::lock_guard<std::mutex> lock(belogs_mutex);
	belogs[event.item.path()].f_pos = 0;
	belogs[event.item.path()].log_data = LogData();
	belogs[event.item.path()].scan_timer.reset(new boost::asio::deadline_timer(*io_service_ptr));
	scanLog(event.item.path());
}


void BELogScanner::onFileRemoved(const Poco::DirectoryWatcher::DirectoryEvent& event)
{
	std::lock_guard<std::mutex> lock(belogs_mutex);
	belogs[event.item.path()].scan_timer->cancel();
	belogs.erase(event.item.path());
}


void BELogScanner::onFileModified(const Poco::DirectoryWatcher::DirectoryEvent& event)
{
	std::lock_guard<std::mutex> lock(belogs_mutex);
	scanLog(event.item.path());
}


void BELogScanner::onFileMovedFrom(const Poco::DirectoryWatcher::DirectoryEvent& event)
{
	std::lock_guard<std::mutex> lock(belogs_mutex);
	belogs.erase(event.item.path());
}


void BELogScanner::onFileMovedTo(const Poco::DirectoryWatcher::DirectoryEvent& event)
{
	//
}


void BELogScanner::reloadFilters(const Poco::DirectoryWatcher::DirectoryEvent& event)
{
	loadFilters();
}


void BELogScanner::loadFilters()
{
	std::string path_str;
	std::string filename;  // no extension
	std::string file_extension;

	boost::filesystem::path path(filters_path);

	boost::filesystem::directory_iterator iter(path);
	boost::filesystem::directory_iterator iter_end;

	std::lock_guard<std::mutex> lock(belogs_mutex);
	for (; iter != iter_end; ++iter)
	{
		if (boost::filesystem::is_regular_file(iter->path()))
		{
			boost::filesystem::path temp_path = iter->path();
			path = temp_path.make_preferred();
			path_str = path.string();
			file_extension = path.extension().string();

			filename = path.extension().filename().string();
			filename = filename.substr(0, (filename.size() - file_extension.size()));

			if (boost::algorithm::iequals(file_extension, "banlist") == 1)
			{
				loadRegexFile(path_str, filters_rules[filename].banlist_regex);
			}
			else if (boost::algorithm::iequals(file_extension, "kicklist") == 1)
			{
				loadRegexFile(path_str, filters_rules[filename].kicklist_regex);
			}
			else if (boost::algorithm::iequals(file_extension, "whitelist") == 1)
			{
				loadRegexFile(path_str, filters_rules[filename].whitelist_regex);
			}
			else if (boost::algorithm::iequals(file_extension, "spamlist") == 1)
			{
				loadSpamFile(path_str, filters_rules[filename].spam_rules);
			}
		}
	}
}


void BELogScanner::loadRegexFile(std::string &path_str, std::vector<BELogScanner::Filter> &filters)
{
	Poco::FileInputStream istr(path_str);

	std::string line;
	while (std::getline(istr, line))
	{
		boost::algorithm::trim(line);
		if ((!(boost::algorithm::starts_with(line, ";"))) && (!(line.empty())))
		{
			BELogScanner::Filter filter;
			if (boost::algorithm::icontains(line, "[:player_key:]"))
			{
				filter.dynamic_regex = true;
				filter.regex_str = line;
			}
			else
			{
				filter.regex = std::regex(line, std::regex_constants::ECMAScript | std::regex_constants::icase);
			}
			filters.push_back(std::move(filter));
		}
	}
}


void BELogScanner::loadSpamFile(std::string &path_str, std::vector<BELogScanner::Spam> &spam_rules)
{
	std::string action_str;
	std::vector<std::string> spam_keys;

	Poco::AutoPtr<Poco::Util::IniFileConfiguration> pConf(new Poco::Util::IniFileConfiguration(path_str));
	pConf->keys(spam_keys);
	for (auto &spam_key : spam_keys)
	{
		Spam spam_rule;
		std::string regex_str = pConf->getString(spam_key + ".Regex", "");
		spam_rule.regex = regex_str;
		spam_rule.count = pConf->getInt(spam_key + ".Count", -1);
		spam_rule.time = pConf->getInt(spam_key + ".Time", -1);

		action_str = pConf->getString(spam_key + ".Action", "NONE");
		if (boost::algorithm::iequals(action_str, std::string("BAN")) == 1)
		{
			spam_rule.action = 2;
		}
		else if (boost::algorithm::iequals(action_str, std::string("KICK")) == 1)
		{
			spam_rule.action = 1;
		}
		else
		{
			spam_rule.action = 0;
		}
		if ((spam_rule.action > 0) && (spam_rule.count > 0) && (spam_rule.time > 0) && (!(regex_str.empty())))
		{
			spam_rules.push_back(std::move(spam_rule));
		}
		else
		{
			logger->warn("ERROR: Incomplete Rule: {0}", regex_str);
		}
	}
}


void BELogScanner::launchProcess()
{
	std::string command;
	std::vector<std::string> args;
	std::string initialDirectory;

	Poco::Process::launch(command, args, initialDirectory);
}


void BELogScanner::checkLogData(std::string &filename)
{
	bool found = false;
	auto const_itr = filters_rules.find(filename);
	BELogScanner::LogData log_data = belogs[filename].log_data;

	std::lock_guard<std::mutex> lock(player_key_mutex);
	// WHITELIST
	for (auto &whitelist : const_itr->second.whitelist_regex)
	{
		if (whitelist.dynamic_regex)
		{
			std::string temp_str = whitelist.regex_str;
			//boost::algorithm::ireplace_all(temp_str, "[:player_key:]", player_key[log_data.player_guid]);
			//boost::algorithm::ireplace_all(temp_str, "[:server_key:]", server_key);
			std::regex regex(temp_str, std::regex_constants::ECMAScript | std::regex_constants::icase);
			if (std::regex_search(log_data.logged_line, regex))
			{
				logger->info("WHITELIST: FILE: {0}, PLAYER: {2}, GUID: {3}, IP: {4}, PORT: {5}, LINE: {6}", filename, log_data.player_name, log_data.player_guid, log_data.player_ip, log_data.player_port, log_data.logged_line);
				found = true;
				break;
			}
		}
		else
		{
			if (std::regex_search(log_data.logged_line, whitelist.regex))
			{
				logger->info("WHITELIST: FILE: {0}, PLAYER: {2}, GUID: {3}, IP: {4}, PORT: {5}, LINE: {6}", filename, log_data.player_name, log_data.player_guid, log_data.player_ip, log_data.player_port, log_data.logged_line);
				found = true;
				break;
			}
		}
	}
	if (found)
	{
		logger->info("WHITELISTED");
		logger->info("");
		return;
	}

	//KICK
	for (auto &kicklist : const_itr->second.kicklist_regex)
	{
		if (kicklist.dynamic_regex)
		{
			std::string temp_str = kicklist.regex_str;
			//boost::algorithm::ireplace_all(temp_str, "[:player_key:]", player_key[log_data.player_guid]);
			//boost::algorithm::ireplace_all(temp_str, "[:server_key:]", server_key);
			std::regex regex(temp_str, std::regex_constants::ECMAScript | std::regex_constants::icase);
			if (std::regex_search(log_data.logged_line, regex))
			{
				logger->info("KICKLIST: FILE: {0}, PLAYER: {2}, GUID: {3}, IP: {4}, PORT: {5}, LINE: {6}", filename, log_data.player_name, log_data.player_guid, log_data.player_ip, log_data.player_port, log_data.logged_line);
				found = true;

				if (!(belogs[filename].kick_logger))
				{
					boost::filesystem::path path(be_custom_log_path);
					path /= (filename + "-kick.log");
					belogs[filename].kick_logger.reset(new spdlog::logger(filename + "-kick.log", std::make_shared<spdlog::sinks::simple_file_sink_mt>(path.make_preferred().string(), true)));
				}
				belogs[filename].kick_logger->info("{0}", log_data.logged_line);
				break;
			}
		}
		else
		{
			if (std::regex_search(log_data.logged_line, kicklist.regex))
			{
				logger->info("KICKLIST: FILE: {0}, PLAYER: {2}, GUID: {3}, IP: {4}, PORT: {5}, LINE: {6}", filename, log_data.player_name, log_data.player_guid, log_data.player_ip, log_data.player_port, log_data.logged_line);
				found = true;

				if (!(belogs[filename].kick_logger))
				{
					boost::filesystem::path path(be_custom_log_path);
					path /= (filename + "-kick.log");
					belogs[filename].kick_logger.reset(new spdlog::logger(filename + "-kick.log", std::make_shared<spdlog::sinks::simple_file_sink_mt>(path.make_preferred().string(), true)));
				}
				belogs[filename].kick_logger->info("{0}", log_data.logged_line);
				break;
			}
		}
	}
	if (found)
	{
		logger->info("KICK");
		logger->info("");
		return;
	}

	//BAN
	for (auto &banlist : const_itr->second.banlist_regex)
	{
		if (banlist.dynamic_regex)
		{
			std::string temp_str = banlist.regex_str;
			//boost::algorithm::ireplace_all(temp_str, "[:player_key:]", player_key[log_data.player_guid]);
			//boost::algorithm::ireplace_all(temp_str, "[:server_key:]", server_key);
			std::regex regex(temp_str, std::regex_constants::ECMAScript | std::regex_constants::icase);
			if (std::regex_search(log_data.logged_line, regex))
			{
				logger->info("BANLIST: FILE: {0}, PLAYER: {2}, GUID: {3}, IP: {4}, PORT: {5}, LINE: {6}", filename, log_data.player_name, log_data.player_guid, log_data.player_ip, log_data.player_port, log_data.logged_line);
				found = true;

				if (!(belogs[filename].ban_logger))
				{
					boost::filesystem::path path(be_custom_log_path);
					path /= (filename + "-ban.log");
					belogs[filename].ban_logger.reset(new spdlog::logger(filename + "-ban.log", std::make_shared<spdlog::sinks::simple_file_sink_mt>(path.make_preferred().string(), true)));
				}
				belogs[filename].ban_logger->info("{0}", log_data.logged_line);
				break;
			}
		}
		else
		{
			if (std::regex_search(log_data.logged_line, banlist.regex))
			{
				logger->info("BANLIST: FILE: {0}, PLAYER: {2}, GUID: {3}, IP: {4}, PORT: {5}, LINE: {6}", filename, log_data.player_name, log_data.player_guid, log_data.player_ip, log_data.player_port, log_data.logged_line);
				found = true;

				if (!(belogs[filename].ban_logger))
				{
					boost::filesystem::path path(be_custom_log_path);
					path /= (filename + "-ban.log");
					belogs[filename].ban_logger.reset(new spdlog::logger(filename + "-ban.log", std::make_shared<spdlog::sinks::simple_file_sink_mt>(path.make_preferred().string(), true)));
				}
				belogs[filename].ban_logger->info("{0}", log_data.logged_line);
				break;
			}
		}
	}
	if (found)
	{
		logger->info("BAN");
		logger->info("");
		return;
	}

	//SPAM
	for (auto &spam_rules : const_itr->second.spam_rules)
	{
		// TODO Poco Expire Cache
		//, std::regex_constants::ECMAScript | std::regex_constants::icase);
	}
	if (found)
	{
		logger->info("SPAM ACTION");
		logger->info("");
		return;
	}

	// UNKNOWN
	if (!found)
	{
		if (!(belogs[filename].unknown_logger))
		{
			boost::filesystem::path path(be_custom_log_path);
			path /= (filename + "-unknown.log");
			belogs[filename].unknown_logger.reset(new spdlog::logger(filename + "-unknown.log", std::make_shared<spdlog::sinks::simple_file_sink_mt>(path.make_preferred().string(), true)));
		}
		belogs[filename].unknown_logger->info("{0}", log_data.logged_line);
	}
}


void BELogScanner::getBEGUID(std::string &steam_id, std::string &beguid)
// From Frank https://gist.github.com/Fank/11127158
// Modified to use libpoco
{
	bool status = true;

	if (steam_id.empty())
	{
		status = false;
	}
	else
	{
		for (unsigned int index = 0; index < steam_id.length(); index++)
		{
			if (!std::isdigit(steam_id[index]))
			{
				status = false;
				break;
			}
		}
	}

	if (status)
	{
		Poco::Int64 steamID = Poco::NumberParser::parse64(steam_id);
		Poco::Int8 i = 0, parts[8] = { 0 };

		do
		{
			parts[i++] = steamID & 0xFFu;
		} while (steamID >>= 8);

		std::stringstream bestring;
		bestring << "BE";
		for (auto &part: parts)
		{
			bestring << char(part);
		}

		md5.update(bestring.str());
		beguid = Poco::DigestEngine::digestToHex(md5.digest());
	}
}


void BELogScanner::scanLog(std::string path_str)
{
	boost::filesystem::path path(path_str);

	std::string filename = path.extension().filename().string();
	std::string file_extension = path.extension().string();
	filename = filename.substr(0, (filename.size() - file_extension.size()));


	Poco::FileInputStream istr(path_str);

	std::regex regex_date_time ("[0-3][0-9]\\.[0-1][0-9]\\.[0-9][0-9][0-9][0-9][ ][0-2][0-9][:][0-6][0-9][:][0-6][0-9][:]\\s");
	std::regex regex_ip_port ("[:digit:]{1,3}\\.[:digit:]{1,3}\\.[:digit:]{1,3}\\.[:digit:]{1,3}:[:digit:]{1,5}[0-9]");

	std::string line;
	std::string::size_type found;
	{
		std::lock_guard<std::mutex> lock(belogs_mutex);
		{
			std::lock_guard<std::mutex> lock(update_player_key_mutex);
			for (auto &key : add_player_key)
			{
				std::string beguid;
				getBEGUID(key.steam_id, beguid);
				if (std::count(player_key[beguid].keys.begin(), player_key[beguid].keys.end(), key.value) != 0)
				{
					// TODO OUTPUT WARNING DUPLICATE VALUE
				}
				else
				{
					player_key[beguid].keys.push_back(std::move(key.value));
				}
			}
			add_player_key.clear();

			std::vector<BELogScanner::RemovePlayerKey> new_remove_player_key;
			for (auto &key : remove_player_key)
			{
				std::string beguid;
				getBEGUID(key.steam_id, beguid);
				if (key.timestamp.elapsed() > 30000000) // Allow for overlap of player values, don't need to worry about slow sqf code / battleye buffering log output
				{
					if (player_key.count(beguid) != 0)
					{
						player_key[beguid].keys.remove(key.value);
						if (player_key[beguid].keys.empty())
						{
							player_key.erase(beguid);
						}
					}
					else
					{
						// TODO OUTOUT WARNING NON EXISTANT USER
					}
				}
				else
				{
					new_remove_player_key.push_back(key);
				}
			}
			remove_player_key = std::move(new_remove_player_key);
		}

		istr.seekg(belogs[filename].f_pos);
		while (std::getline(istr, line))
		{
			std::smatch match;
			if (std::regex_search(line, match, regex_date_time, std::regex_constants::match_continuous))
			{
				if (!((belogs[filename].log_data).player_name.empty()))
				{
					checkLogData(filename);
				}

				belogs[filename].log_data = LogData();
				belogs[filename].log_data.date_time = match[0];
				line = line.substr(line.find(":") + 2);

				std::regex_match(line, match, regex_ip_port);
				std::string match_str = match[0].str();

				found = match_str.find(":");
				belogs[filename].log_data.player_ip = match_str.substr(0, (found - 1));
				belogs[filename].log_data.player_port = match_str.substr(found + 1);

				found = line.find(match_str);
				belogs[filename].log_data.player_name = line.substr(0, found - 2);  // -2 = _(
				line = line.substr(found + match_str.size() + 2);

				found = belogs[filename].log_data.player_guid.find(" -");
				belogs[filename].log_data.player_guid = line.substr(0, found - 1);
				belogs[filename].log_data.logged_line = line.substr(found + 2);  // LEFT WITH #......................................
			}
			else
			{
				belogs[filename].log_data.logged_line += line;
			}
		}

		if (belogs[filename].f_pos == istr.tellg())
		{
			timerScanLog(0, path_str);
		}
		else
		{
			belogs[filename].f_pos = istr.tellg();
			timerScanLog(3, path_str);
		}
	}
}


void BELogScanner::timerScanLog(const size_t delay, std::string &belog_path)
{
	if (delay == 0)
	{
		belogs[belog_path].scan_timer->cancel();
	}
	else
	{
		belogs[belog_path].scan_timer->expires_from_now(boost::posix_time::seconds(delay));
		belogs[belog_path].scan_timer->async_wait(std::bind(&BELogScanner::scanLog, this, std::move(belog_path)));
	}
}
