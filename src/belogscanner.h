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


#pragma once

#include <regex>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

#include <Poco/DateTime.h>
#include <Poco/DirectoryWatcher.h>
#include <Poco/ExpireCache.h>
#include <Poco/MD5Engine.h>

#include "spdlog/spdlog.h"


class BELogScanner
{
	public:
		BELogScanner();
		~BELogScanner();

		void start(std::string &bepath, boost::asio::io_service &io_service, std::shared_ptr<spdlog::logger> spdlog);
		void stop();

		void updateAdd(std::string &steam_id, std::string &value);
		void updateRemove(std::string &steam_id, std::string &value);

	protected:

	private:
		boost::filesystem::path be_custom_log_path;
		boost::filesystem::path filters_path;

		boost::asio::io_service *io_service_ptr;
		std::shared_ptr<spdlog::logger> logger;

		std::unique_ptr<Poco::DirectoryWatcher> be_directory_watcher;
		std::unique_ptr<Poco::DirectoryWatcher> filters_directory_watcher;


		struct Filter
		{
			std::regex regex;
			std::string regex_str;
			bool dynamic_regex = false;
		};

		struct Spam
		{
			std::regex regex;
			std::string regex_str;
			bool dynamic_regex = false;

			int action=0; //0=NONE, 1=KICK, 2=BAN
			int count=-1;
			int time=-1;

			std::shared_ptr<Poco::ExpireCache<std::string, std::list<Poco::DateTime> > > cache;
		};

		struct FilterRules
		{
			std::vector<Filter> banlist_regex;
			std::vector<Filter> kicklist_regex;
			std::vector<Filter> whitelist_regex;
			std::vector<Spam> spam_rules;
		};
		std::unordered_map<std::string, FilterRules> filters_rules;

		struct LogData
		{
			//Poco::DateTime date_time;
			std::string date_time;

			std::string player_name;
			std::string player_guid;

			std::string player_ip;
			std::string player_port;

			std::string logged_line;
		};

		struct BELog
		{
			std::streamoff f_pos;
			LogData log_data;

			std::unique_ptr<boost::asio::deadline_timer> scan_timer;

			std::shared_ptr<spdlog::logger> kick_logger;
			std::shared_ptr<spdlog::logger> ban_logger;
			std::shared_ptr<spdlog::logger> unknown_logger;
		};
		std::unordered_map<std::string, BELog> belogs;
		std::mutex belogs_mutex;


		struct PlayerKey
		{
			std::list<std::string> keys;
			std::string str;
		};
		std::unordered_map<std::string, PlayerKey> player_key;
		std::mutex player_key_mutex;

		struct AddPlayerKey
		{
			std::string steam_id;
			std::string value;
		};

		struct RemovePlayerKey
		{
			std::string steam_id;
			std::string value;
			Poco::Timestamp timestamp;
		};

		Poco::MD5Engine md5;
		std::vector<AddPlayerKey> add_player_key;
		std::vector<RemovePlayerKey> remove_player_key;
		std::mutex update_player_key_mutex;


		void loadFilters();
		void reloadFilters(const Poco::DirectoryWatcher::DirectoryEvent& event);

		void loadRegexFile(std::string &path_str, std::vector<Filter> &filters);
		void loadSpamFile(std::string &path_str, std::vector<Spam> &spam_rules);

		void onFileAdded(const Poco::DirectoryWatcher::DirectoryEvent& event);
		void onFileRemoved(const Poco::DirectoryWatcher::DirectoryEvent& event);
		void onFileModified(const Poco::DirectoryWatcher::DirectoryEvent& event);
		void onFileMovedFrom(const Poco::DirectoryWatcher::DirectoryEvent& event);
		void onFileMovedTo(const Poco::DirectoryWatcher::DirectoryEvent& event);

		void launchProcess();
		void getBEGUID(std::string &steam_id, std::string &beguid);

		void checkLogData(std::string &filename);
		void scanLog(std::string path_str);
		void timerScanLog(const size_t delay, std::string &belog_path);
};