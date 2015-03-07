/*
Copyright (C) 2014 Declan Ireland <http://github.com/torndeco/extDB>

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

Code to Convert SteamID -> BEGUID 
From Frank https://gist.github.com/Fank/11127158

*/


#include "steamworker.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <Poco/MD5Engine.h>
#include <Poco/DigestEngine.h>

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/Path.h>
#include <Poco/URI.h>

#include <Poco/DateTimeFormatter.h>
#include <Poco/NumberParser.h>
#include <Poco/Thread.h>
#include <Poco/Types.h>
#include <Poco/Exception.h>

#include <string>


// --------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------

void STEAMGET::init(AbstractExt *extension)
{
	extension_ptr = extension;
	session = new Poco::Net::HTTPClientSession("api.steampowered.com", 80);
}


void STEAMGET::update(std::string &input_str, boost::property_tree::ptree &ptree)
{
	path = input_str;
	pt = &ptree;
}


int STEAMGET::getResponse()
{
	return response;
}


void STEAMGET::run()
{
	response = 0;
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
	session->sendRequest(request);

	#ifdef TESTING
		extension_ptr->console->info("{0}", path);
	#endif
	#ifdef DEBUG_LOGGING
		extension_ptr->logger->info("{0}", path);
	#endif

	Poco::Net::HTTPResponse res;
	if (res.getStatus() == Poco::Net::HTTPResponse::HTTP_OK)
	{
		try
		{
			std::istream &is = session->receiveResponse(res);
			boost::property_tree::read_json(is, *pt);
			response = 1;
		}
		catch (boost::property_tree::json_parser::json_parser_error &e)
		{
			#ifdef TESTING
				extension_ptr->console->error("extDB2: Steam: Parsing Error Message: {0}, URI: {1}", e.message(), path);
			#endif
			extension_ptr->logger->error("extDB2: Steam: Parsing Error Message: {0}, URI: {1}", e.message(), path);
			response = -1;
		}
	}
}


void STEAMGET::abort()
{
	session->abort();
}


void STEAMGET::stop()
{
	session->reset();
}


// --------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------


void STEAMWORKER::init(AbstractExt *extension, std::string &extension_path, Poco::DateTime &current_dateTime)
{
	extension_ptr = extension;
	steam_run_flag = new std::atomic<bool>(false);

	STEAM_api_key = extension_ptr->pConf->getString("Steam.API Key", "");

	rconBanSettings.autoBan = extension_ptr->pConf->getBool("VAC.Auto Ban", false);
	rconBanSettings.NumberOfVACBans = extension_ptr->pConf->getInt("VAC.NumberOfVACBans", 1);
	rconBanSettings.DaysSinceLastBan = extension_ptr->pConf->getInt("VAC.DaysSinceLastBan", 0);
	rconBanSettings.BanDuration = extension_ptr->pConf->getString("VAC.BanDuration", "0");
	rconBanSettings.BanMessage = extension_ptr->pConf->getString("VAC.BanMessage", "VAC Ban");

	SteamVacBans_Cache = new Poco::ExpireCache<std::string, SteamVACBans>(extension_ptr->pConf->getInt("STEAM.BanCacheTime", 3600000));
	SteamFriends_Cache = new Poco::ExpireCache<std::string, SteamFriends>(extension_ptr->pConf->getInt("STEAM.FriendsCacheTime", 3600000));

	if (rconBanSettings.autoBan)
	{
		std::string log_filename = Poco::DateTimeFormatter::format(current_dateTime, "%H-%M-%S.log");

		boost::filesystem::path vacBans_log_relative_path;
		vacBans_log_relative_path = boost::filesystem::path(extension_path);
		vacBans_log_relative_path /= "extDB";
		vacBans_log_relative_path /= "vacban_logs";
		vacBans_log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%Y");
		vacBans_log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%n");
		vacBans_log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%d");
		boost::filesystem::create_directories(vacBans_log_relative_path);
		vacBans_log_relative_path /= log_filename;

		auto vacBans_logger_temp = spdlog::daily_logger_mt("extDB vacBans Logger", vacBans_log_relative_path.make_preferred().string(), true);
		extension_ptr->vacBans_logger.swap(vacBans_logger_temp);
	}
}


void STEAMWORKER::stop()
{
	*steam_run_flag = false;
}


std::string STEAMWORKER::convertSteamIDtoBEGUID(const std::string &input_str)
// From Frank https://gist.github.com/Fank/11127158
// Modified to use libpoco
{
	Poco::Int64 steamID = Poco::NumberParser::parse64(input_str);
	Poco::Int8 i = 0, parts[8] = { 0 };

	do
	{
		parts[i++] = steamID & 0xFFu;
	} while (steamID >>= 8);

	std::stringstream bestring;
	bestring << "BE";
	for (int i = 0; i < sizeof(parts); i++) {
		bestring << char(parts[i]);
	}

	boost::lock_guard<boost::mutex> lock(mutex_md5);
	md5.update(bestring.str());
	return Poco::DigestEngine::digestToHex(md5.digest());
}


std::vector<std::string> STEAMWORKER::generateSteamIDStrings(std::vector<std::string> &steamIDs)
// Steam Only Allows 100 SteamIDs at a time
{
	std::string steamIDs_str;
	std::vector<std::string> steamIDs_list;
	
	int counter = 0;
	for (auto &val: steamIDs)
	{
		if (counter == 100)
		{
			steamIDs_str.erase(steamIDs_str.begin());
			steamIDs_str.pop_back();
			steamIDs_list.push_back(steamIDs_str);
		
			steamIDs_str.clear();
			counter = 0;
		}
		++counter;
		steamIDs_str += val + ",";
	}

	if (!steamIDs_str.empty())
	{
		steamIDs_str.pop_back();
		steamIDs_list.push_back(std::move(steamIDs_str));
	}
	return steamIDs_list;
}


void STEAMWORKER::updateSteamBans(std::vector<std::string> &steamIDs)
{
	bool loadBans = false;

	// Lose Duplicate steamIDs for Steam WEB API Query
	std::sort(steamIDs.begin(), steamIDs.end());
	auto last = std::unique(steamIDs.begin(), steamIDs.end());
	steamIDs.erase(last, steamIDs.end());

	std::vector<std::string> update_steamIDs;
	for (auto &steamID: steamIDs)
	{
		if (!(SteamVacBans_Cache->has(steamID)))
		{
			update_steamIDs.push_back(steamID);
		}
	}

	Poco::Thread steam_thread;
	std::vector<std::string> steamIDStrings = generateSteamIDStrings(update_steamIDs);
	for (auto &steamIDString: steamIDStrings)
	{
		std::string query = "/ISteamUser/GetPlayerBans/v1/?key=" + STEAM_api_key + "&format=json&steamids=" + steamIDString;	

		boost::property_tree::ptree pt;
		steam_get.update(query, pt);
		steam_thread.start(steam_get);
		try
		{
			steam_thread.join(10000); // 10 Seconds
			switch (steam_get.getResponse())
			{
				case 1:
					// SUCCESS STEAM QUERY
					for (const auto &val : pt.get_child("players"))
					{
						SteamVACBans steam_info;
						steam_info.steamID = val.second.get<std::string>("SteamId", "");
						steam_info.NumberOfVACBans = val.second.get<int>("NumberOfVACBans", 0);
						steam_info.VACBanned = val.second.get<bool>("VACBanned", false);
						steam_info.DaysSinceLastBan = val.second.get<int>("DaysSinceLastBan", 0);

						#ifdef TESTING
							extension_ptr->console->info();
							extension_ptr->console->info("VAC Bans Info: steamID {0}", steam_info.steamID);
							extension_ptr->console->info("VAC Bans Info: NumberOfVACBans {0}", steam_info.NumberOfVACBans);
							extension_ptr->console->info("VAC Bans Info: VACBanned {0}", steam_info.VACBanned);
							extension_ptr->console->info("VAC Bans Info: DaysSinceLastBan {0}", steam_info.DaysSinceLastBan);
						#endif

						if ((steam_info.NumberOfVACBans >= rconBanSettings.NumberOfVACBans) && (steam_info.DaysSinceLastBan <= rconBanSettings.DaysSinceLastBan))
						{
							steam_info.extDBBanned = true;
							if ((extension_ptr->extDB_connectors_info.rcon) && rconBanSettings.autoBan)
							{
								std::string beguid =  convertSteamIDtoBEGUID(steam_info.steamID);
								extension_ptr->rconCommand("addBan " + beguid + " " + rconBanSettings.BanDuration + " " + rconBanSettings.BanMessage);
								extension_ptr->vacBans_logger->warn("Banned: {0}, BEGUID: {1}, Duration: {2}, Ban Message: {3}", steam_info.steamID, beguid, rconBanSettings.BanDuration, rconBanSettings.BanMessage);
								loadBans = true;
							}
						}
						SteamVacBans_Cache->add(steam_info.steamID, steam_info);
					}
					break;
				case 0:
					// HTTP ERROR
					break;
				case -1:
					// ERROR STEAM QUERY
					break;
			}
		}
		catch (Poco::TimeoutException&)
		{
			steam_get.abort();
			steam_thread.join();
			#ifdef TESTING
				extension_ptr->console->error("extDB2: Steam: Request Timed Out");
			#endif
			extension_ptr->logger->error("extDB2: Steam: Request Timed Out");
		}
	}
	if (loadBans)
	{
		extension_ptr->rconCommand("loadBans");
	}
}


void STEAMWORKER::updateSteamFriends(std::vector<std::string> &steamIDs)
{
	// Lose Duplicate steamIDs for Steam WEB API Query
	std::sort(steamIDs.begin(), steamIDs.end());
	auto last = std::unique(steamIDs.begin(), steamIDs.end());
	steamIDs.erase(last, steamIDs.end());

	std::vector<std::string> update_steamIDs;
	for (auto &steamID: steamIDs)
	{
		if (!(SteamVacBans_Cache->has(steamID)))
		{
			update_steamIDs.push_back(steamID);
		}
	}

	Poco::Thread steam_thread;
	for (auto &steamID: update_steamIDs)
	{
		std::string query = "/ISteamUser/GetFriendList/v0001/?key=" + STEAM_api_key + "&relationship=friend&format=json&steamid=" + steamID;	

		boost::property_tree::ptree pt;
		steam_get.update(query, pt);
		steam_thread.start(steam_get);

		int response = -2;
		try
		{
			steam_thread.join(10000); // 10 Seconds
			response = steam_get.getResponse();
		}
		catch (Poco::TimeoutException&)
		{
			steam_get.abort();
			steam_thread.join();
		}

		switch (response)
		{
			case 1:
				// SUCCESS STEAM QUERY
				{
					SteamFriends steam_info;
					for (const auto &val : pt.get_child("friendslist.friends"))
					{
						std::string friendsteamID = val.second.get<std::string>("steamid", "");
						if (!friendsteamID.empty())
						{
							steam_info.friends.push_back(friendsteamID);
						}
					}
					SteamFriends_Cache->add(steamID, steam_info);
				}
				break;
			case 0:
				// HTTP ERROR
				break;
			case -1:
				// ERROR STEAM QUERY
				break;
			case -2:
				// Timeout
				{
					steam_get.abort();
					steam_thread.join();
				}
				break;
		}
	}
}


void STEAMWORKER::addQuery(const int &unique_id, bool queryFriends, bool queryVacBans, std::vector<std::string> &steamIDs)
{
	if (*steam_run_flag)
	{
		SteamQuery info;
		info.unique_id = unique_id;
		info.queryFriends = queryFriends;
		info.queryVACBans = queryVacBans;
		info.steamIDs = steamIDs;

		boost::lock_guard<boost::mutex> lock(mutex_query_queue);
		query_queue.push_back(std::move(info));
	}
	else
	{
		AbstractExt::resultData result_data;
		result_data.message = "[0, \"extDB2: Steam Web API is not enabled\"]";
		extension_ptr->saveResult_mutexlock(unique_id, result_data);
	}
}


void STEAMWORKER::run()
{
	std::string result;
	std::vector<SteamQuery> query_queue_copy;
	Poco::SharedPtr<SteamFriends> friends_info;
	Poco::SharedPtr<SteamVACBans> vac_info;

	steam_get.init(extension_ptr);
	*steam_run_flag = true;
	while (*steam_run_flag)
	{
		#ifdef TESTING
			extension_ptr->console->info("extDB2: Steam: Sleep");
		#endif
		#ifdef DEBUG_LOGGING
			extension_ptr->logger->info("extDB2: Steam: Sleep");
		#endif

		Poco::Thread::trySleep(60000); // 1 Minute Sleep unless woken up

		#ifdef TESTING
			extension_ptr->console->info("extDB2: Steam: Wake Up");
		#endif
		#ifdef DEBUG_LOGGING
			extension_ptr->logger->info("extDB2: Steam: Wake Up");
		#endif

		{
			boost::lock_guard<boost::mutex> lock(mutex_query_queue);
			query_queue_copy = query_queue;
			query_queue.clear();
		}

		if (!query_queue_copy.empty())
		{
			std::vector<std::string> steamIDs_friends;
			std::vector<std::string> steamIDs_bans;

			for (auto &val: query_queue_copy)
			{
				if (val.queryFriends)
				{
					steamIDs_friends.insert(steamIDs_friends.end(), val.steamIDs.begin(), val.steamIDs.end());
				}
				if (val.queryVACBans)
				{
					steamIDs_bans.insert(steamIDs_bans.end(), val.steamIDs.begin(), val.steamIDs.end());
				}
			}

			updateSteamFriends(steamIDs_friends);
			updateSteamBans(steamIDs_bans);

			result.clear();
			for (auto &val: query_queue_copy)
			{
				if (val.unique_id > 0)
				{
					result.clear();
					if (val.queryFriends)
					{
						for (auto &steamID: val.steamIDs)
						{
							friends_info = SteamFriends_Cache->get(steamID);
							if (friends_info.isNull())
							{
								result += "[],";
								#ifdef TESTING
									extension_ptr->logger->error("extDB2: Steam: No Friends Entry for: {0}", steamID);
								#endif
								#ifdef DEBUG_LOGGING
									extension_ptr->logger->warn("extDB2: Steam: No Friends Entry for: {0}", steamID);
								#endif
							}
							else
							{
								for (auto &friendSteamID: friends_info->friends)
								{
									result += "\"" + friendSteamID + "\",";
								}
							}
						}
					}
					else if (val.queryVACBans)
					{
						for (auto &steamID : val.steamIDs)
						{
							vac_info = SteamVacBans_Cache->get(steamID);
							if (vac_info.isNull()) // Incase entry expired
							{
								result += "false,";
								#ifdef TESTING
									extension_ptr->logger->error("extDB2: Steam: No Bans Entry for: {0}", steamID);
								#endif
								#ifdef DEBUG_LOGGING
									extension_ptr->logger->warn("extDB2: Steam: No Bans Entry for: {0}", steamID);
								#endif
							}
							else
							{
								if (vac_info->extDBBanned)
								{
									result += "true,";
								}
								else
								{
									result += "false,";
								}
							}
						}
					}
					if (!result.empty())
					{
						result.pop_back();
					}
					result = "[1,[" + result + "]]";
					AbstractExt::resultData result_data;
					result_data.message = result;
					extension_ptr->saveResult_mutexlock(val.unique_id, result_data);
				}
			}
		}
	}
}
