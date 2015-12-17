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

Code to Convert SteamID -> BEGUID
From Frank https://gist.github.com/Fank/11127158

*/


#include "steam.h"

#include <memory>
#include <string>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>

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
#include <Poco/SharedPtr.h>
#include <Poco/Thread.h>
#include <Poco/Types.h>
#include <Poco/Exception.h>



// --------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------

void SteamGet::init(AbstractExt *extension)
{
	extension_ptr = extension;
	session.reset(new Poco::Net::HTTPClientSession("api.steampowered.com", 80));
}


void SteamGet::update(std::string &update_path, Poco::Dynamic::Var &var)
{
	path = update_path;
	json = &var;
}


int SteamGet::getResponse()
{
	return response;
}


void SteamGet::run()
{
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
	session->sendRequest(request);

	#ifdef DEBUG_TESTING
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
			*json = parser.parse(is); // Dynamic::Var
			response = 1;
		}
		catch (Poco::Exception& e)
		{
			#ifdef DEBUG_TESTING
				extension_ptr->console->error("extDB2: Steam: Parsing Error Message: {0}, URI: {1}", e.displayText(), path);
			#endif
			extension_ptr->logger->error("extDB2: Steam: Parsing Error Message: {0}, URI: {1}", e.displayText(), path);
			response = -1;
		}
	}
	else
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: Steam: HTTP Error: {0}, URI: {1}", res.getStatus(), path);
		#endif
		extension_ptr->logger->error("extDB2: Steam: HTTP Error: {0}, URI: {1}", res.getStatus(), path);
		response = 0;
	}
}


void SteamGet::abort()
{
	session->abort();
}


void SteamGet::stop()
{
	session->reset();
}


// --------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------


void Steam::init(AbstractExt *extension, std::string &extension_path, Poco::DateTime &current_dateTime)
{
	extension_ptr = extension;
	steam_run_flag = new std::atomic<bool>(false);

	STEAM_api_key = extension_ptr->pConf->getString("Steam.API Key", "");

	rconBanSettings.autoBan = extension_ptr->pConf->getBool("VAC.Auto Ban", false);
	rconBanSettings.NumberOfVACBans = extension_ptr->pConf->getInt("VAC.NumberOfVACBans", 1);
	rconBanSettings.DaysSinceLastBan = extension_ptr->pConf->getInt("VAC.DaysSinceLastBan", 0);
	rconBanSettings.BanDuration = extension_ptr->pConf->getString("VAC.BanDuration", "0");
	rconBanSettings.BanMessage = extension_ptr->pConf->getString("VAC.BanMessage", "VAC Ban");

	SteamVacBans_Cache.reset(new Poco::ExpireCache<std::string, SteamVACBans>(extension_ptr->pConf->getInt("STEAM.BanCacheTime", 3600000)));
	SteamFriends_Cache.reset(new Poco::ExpireCache<std::string, SteamFriends>(extension_ptr->pConf->getInt("STEAM.FriendsCacheTime", 3600000)));

	if (rconBanSettings.autoBan)
	{
		boost::filesystem::path vacBans_log_relative_path;
		vacBans_log_relative_path = boost::filesystem::path(extension_path);
		vacBans_log_relative_path /= "extDB";
		vacBans_log_relative_path /= "vacban_logs";
		vacBans_log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%Y");
		vacBans_log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%n");
		vacBans_log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%d");
		boost::filesystem::create_directories(vacBans_log_relative_path);
		vacBans_log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%H-%M-%S");
		log_filename = vacBans_log_relative_path.make_preferred().string();
	}
}

void Steam::initBanslogger()
{
	if (rconBanSettings.autoBan)
	{
		auto vacBans_logger_temp = spdlog::daily_logger_mt("extDB vacBans Logger", log_filename, true);
		extension_ptr->vacBans_logger.swap(vacBans_logger_temp);
	}
}

void Steam::stop()
{
	*steam_run_flag = false;
}


std::string Steam::convertSteamIDtoBEGUID(const std::string &input_str)
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

	std::lock_guard<std::mutex> lock(mutex_md5);
	md5.update(bestring.str());
	return Poco::DigestEngine::digestToHex(md5.digest());
}


std::vector<std::string> Steam::generateSteamIDStrings(std::vector<std::string> &steamIDs)
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


void Steam::updateSteamBans(std::vector<std::string> &steamIDs)
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

		Poco::Dynamic::Var json;
		steam_get.update(query, json);
		steam_thread.start(steam_get);
		try
		{
			steam_thread.join(10000); // 10 Seconds
			switch (steam_get.getResponse())
			{
				case 1:
				// SUCCESS STEAM QUERY
					{
						Poco::JSON::Object::Ptr json_object = json.extract<Poco::JSON::Object::Ptr>();
						for (const auto &val : *(json_object->getArray("players")))
						{
							Poco::JSON::Object::Ptr sub_json_object = val.extract<Poco::JSON::Object::Ptr>();
							SteamVACBans steam_info;
							//SteamId
							if (sub_json_object->has("SteamId"))
							{
								steam_info.steamID = sub_json_object->getValue<std::string>("SteamId");
							}
							else
							{
								steam_info.steamID = "";
								#ifdef DEBUG_TESTING
									extension_ptr->console->error("extDB2: Steam: Missing SteamId for playerinfo");
								#endif
								extension_ptr->logger->error("extDB2: Steam: Missing SteamId for playerinfo");
							}
							//NumberOfVACBans
							if (sub_json_object->has("NumberOfVACBans"))
							{
								steam_info.NumberOfVACBans = sub_json_object->getValue<int>("NumberOfVACBans");
							}
							else
							{
								steam_info.NumberOfVACBans = 0;
								#ifdef DEBUG_TESTING
									extension_ptr->console->error("extDB2: Steam: Missing NumberOfVACBans for playerinfo");
								#endif
								extension_ptr->logger->error("extDB2: Steam: Missing NumberOfVACBans for playerinfo");
							}
							//VACBanned
							if (sub_json_object->has("VACBanned"))
							{
								steam_info.VACBanned = sub_json_object->getValue<bool>("VACBanned");
							}
							else
							{
								steam_info.VACBanned = false;
								#ifdef DEBUG_TESTING
									extension_ptr->console->error("extDB2: Steam: Missing VACBanned for playerinfo");
								#endif
								extension_ptr->logger->error("extDB2: Steam: Missing VACBanned for playerinfo");
							}
							//DaysSinceLastBan
							if (sub_json_object->has("DaysSinceLastBan"))
							{
								steam_info.DaysSinceLastBan = sub_json_object->getValue<int>("DaysSinceLastBan");
							}
							else
							{
								steam_info.DaysSinceLastBan = 0;
								#ifdef DEBUG_TESTING
									extension_ptr->console->error("extDB2: Steam: Missing DaysSinceLastBan for playerinfo");
								#endif
								extension_ptr->logger->error("extDB2: Steam: Missing DaysSinceLastBan for playerinfo");
							}

							#ifdef DEBUG_TESTING
								extension_ptr->console->info();
								extension_ptr->console->info("VAC Bans Info: SteamId {0}", steam_info.steamID);
								extension_ptr->console->info("VAC Bans Info: NumberOfVACBans {0}", steam_info.NumberOfVACBans);
								extension_ptr->console->info("VAC Bans Info: VACBanned {0}", steam_info.VACBanned);
								extension_ptr->console->info("VAC Bans Info: DaysSinceLastBan {0}", steam_info.DaysSinceLastBan);
							#endif

							if ((steam_info.NumberOfVACBans >= rconBanSettings.NumberOfVACBans) && (steam_info.DaysSinceLastBan <= rconBanSettings.DaysSinceLastBan))
							{
								steam_info.extDBBanned = true;
								if ((extension_ptr->ext_connectors_info.rcon) && rconBanSettings.autoBan)
								{
									if (extension_ptr->vacBans_logger)
									{
										initBanslogger();
									}
									std::string beguid = convertSteamIDtoBEGUID(steam_info.steamID);
									extension_ptr->rconCommand("addBan " + beguid + " " + rconBanSettings.BanDuration + " " + rconBanSettings.BanMessage);
									extension_ptr->vacBans_logger->warn("Banned: {0}, BEGUID: {1}, Duration: {2}, Ban Message: {3}", steam_info.steamID, beguid, rconBanSettings.BanDuration, rconBanSettings.BanMessage);
									loadBans = true;
								}
							}
							SteamVacBans_Cache->add(steam_info.steamID, steam_info);
						}
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
			#ifdef DEBUG_TESTING
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


void Steam::updateSteamFriends(std::vector<std::string> &steamIDs)
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

		Poco::Dynamic::Var json;
		steam_get.update(query, json);
		steam_thread.start(steam_get);
		try
		{
			steam_thread.join(10000); // 10 Seconds
			switch (steam_get.getResponse())
			{
				case 1:
					// SUCCESS STEAM QUERY
					{
						Poco::JSON::Object::Ptr json_object = json.extract<Poco::JSON::Object::Ptr>();
						SteamFriends steam_info;
						std::string friendsteamID;
						for (const auto &val : *(json_object->getObject("friendslist")->getArray("friends")))
						{
							Poco::JSON::Object::Ptr sub_json_object = val.extract<Poco::JSON::Object::Ptr>();
							if (sub_json_object->has("steamid"))
							{
								friendsteamID = sub_json_object->getValue<std::string>("steamid"); // TODO Default Value
								if (!friendsteamID.empty())
								{
									steam_info.friends.push_back(friendsteamID);
								}
							}
							else
							{
								#ifdef DEBUG_TESTING
									extension_ptr->console->error("extDB2: Steam: Missing steamid for friend");
								#endif
								extension_ptr->logger->error("extDB2: Steam: Missing steamid for friend");
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
			}
		}
		catch (Poco::TimeoutException&)
		{
			steam_get.abort();
			steam_thread.join();
			#ifdef DEBUG_TESTING
				extension_ptr->console->error("extDB2: Steam: Request Timed Out");
			#endif
			extension_ptr->logger->error("extDB2: Steam: Request Timed Out");
		}
	}
}


void Steam::addQuery(const unsigned int &unique_id, bool queryFriends, bool queryVacBans, std::vector<std::string> &steamIDs)
{
	if (*steam_run_flag)
	{
		SteamQuery info;
		info.unique_id = unique_id;
		info.queryFriends = queryFriends;
		info.queryVACBans = queryVacBans;
		info.steamIDs = steamIDs;

		std::lock_guard<std::mutex> lock(mutex_query_queue);
		query_queue.push_back(std::move(info));
	}
	else
	{
		AbstractExt::resultData result_data;
		result_data.message = "[0, \"extDB2: Steam Web API is not enabled\"]";
		extension_ptr->saveResult_mutexlock(unique_id, result_data);
	}
}


void Steam::run()
{
	std::string result;
	std::vector<SteamQuery> query_queue_copy;
	Poco::SharedPtr<SteamFriends> friends_info;
	Poco::SharedPtr<SteamVACBans> vac_info;

	steam_get.init(extension_ptr);
	*steam_run_flag = true;
	while (*steam_run_flag)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->info("extDB2: Steam: Sleep");
		#endif

		Poco::Thread::trySleep(60000); // 1 Minute Sleep unless woken up

		#ifdef DEBUG_TESTING
			extension_ptr->console->info("extDB2: Steam: Wake Up");
		#endif

		{
			std::lock_guard<std::mutex> lock(mutex_query_queue);
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
				if (val.unique_id > 1)
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
								#ifdef DEBUG_TESTING
									extension_ptr->console->error("extDB2: Steam: No Friends Entry for: {0}", steamID);
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
								#ifdef DEBUG_TESTING
									extension_ptr->console->error("extDB2: Steam: No Bans Entry for: {0}", steamID);
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
					result_data.message = std::move(result);
					extension_ptr->saveResult_mutexlock(val.unique_id, result_data);
				}
			}
		}
	}
}
