/*
Copyright (C) 2014 Declan Ireland <http://github.com/torndeco/extDB2>

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

#include <cstdlib>
#include <iostream>
#include <thread>

#include "Poco/Dynamic/Var.h"
#include "Poco/JSON/Parser.h"

#include <Poco/MD5Engine.h>
#include <Poco/DigestEngine.h>

#include <Poco/AbstractCache.h>
#include <Poco/DateTime.h>
#include <Poco/ExpireCache.h>

#include <Poco/Net/HTTPClientSession.h>

#include "../abstract_ext.h"


class SteamGet: public Poco::Runnable
{
	public:
		void init(AbstractExt *extension);

		void run();
		void stop();

		void abort();

		void update(std::string &update_path, Poco::Dynamic::Var &var);
		int getResponse();

	private:
		AbstractExt *extension_ptr;

		std::string path;
		std::string steam_api_key;

		std::unique_ptr<Poco::Net::HTTPClientSession> session;

		Poco::Dynamic::Var *json;
		Poco::JSON::Parser parser;

		int response=-1;
};


class Steam: public Poco::Runnable
{
	public:
		void run();
		void stop();

		void init(AbstractExt *extension, std::string &extension_path, Poco::DateTime &current_dateTime);
		void initBanslogger();
		void addQuery(const unsigned int &unique_id, bool queryFriends, bool queryVacBans, std::vector<std::string> &steamIDs);

	private:
		AbstractExt *extension_ptr;
		
		struct SteamVACBans
		{
			int NumberOfVACBans;
			int DaysSinceLastBan;
			std::string steamID;

			bool extDBBanned=false;
			bool VACBanned;
		};

		struct SteamFriends
		{
			std::string steamID;
			std::vector<std::string> friends;
		};
		
		struct SteamQuery
		{
			unsigned long unique_id;
			std::vector<std::string> steamIDs;

			bool queryFriends;
			bool queryVACBans;
		};

		struct RConBan
		{
			int NumberOfVACBans;
			int DaysSinceLastBan;
			std::string BanDuration;
			std::string BanMessage;
			bool autoBan;
		};

		std::vector<SteamQuery> query_queue;
		std::mutex mutex_query_queue;
		
		std::string STEAM_api_key;
		RConBan rconBanSettings;
		std::unique_ptr<Poco::ExpireCache<std::string, SteamVACBans> > SteamVacBans_Cache; // 1 Hour (3600000)
		std::unique_ptr<Poco::ExpireCache<std::string, SteamFriends> > SteamFriends_Cache; // 1 Hour (3600000)

		void updateSteamBans(std::vector<std::string> &steamIDs);
		void updateSteamFriends(std::vector<std::string> &steamIDs);
		std::string convertSteamIDtoBEGUID(const std::string &input_str);
		std::vector<std::string> generateSteamIDStrings(std::vector<std::string> &steamIDs);

		Poco::MD5Engine md5;
		std::mutex mutex_md5;

		std::atomic<bool> *steam_run_flag;

		SteamGet steam_get;

		std::string log_filename;
};
