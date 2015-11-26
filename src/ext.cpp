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
#include "ext.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <regex>
#include <thread>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#ifdef TEST_APP
	#include <boost/program_options.hpp>
#endif
#ifdef _WIN32
	#include <boost/random/random_device.hpp>
	#include <boost/random/uniform_int_distribution.hpp>
#endif

#include <Poco/AutoPtr.h>
#include <Poco/DateTime.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Exception.h>
#include <Poco/LocalDateTime.h>
#include <Poco/NumberFormatter.h>
#include <Poco/NumberParser.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Util/IniFileConfiguration.h>

#include <Poco/Data/Session.h>
#include <Poco/Data/SessionPool.h>
#include <Poco/Data/MySQL/Connector.h>
#include <Poco/Data/MySQL/MySQLException.h>
#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Data/SQLite/SQLiteException.h>

#include "abstract_ext.h"
#include "backends/rcon.h"
#include "backends/steam.h"

#include "protocols/abstract_protocol.h"
#include "protocols/sql_custom_v2.h"
#include "protocols/sql_raw_v2.h"
#include "protocols/log.h"
#include "protocols/misc.h"
#include "protocols/rcon.h"
#include "protocols/steam_v2.h"



Ext::Ext(std::string shared_library_path, std::unordered_map<std::string, std::string> &options)
{
	try
	{
		timestamp.update();
		bool conf_found = false;
		#ifdef _WIN32
			bool conf_randomized = false;
		#endif

		boost::filesystem::path config_path;

		ext_info.be_path = options["BEPATH"];
		ext_info.var = "\"" + options["VAR"] + "\"";

		if (options.count("WORK") > 0)
		{
			// Override extDB2 Location
			config_path = options["WORK"];
			config_path /= "extdb-conf.ini";
			if (boost::filesystem::exists(config_path))
			{
				conf_found = true;
				ext_info.path = config_path.parent_path().string();
			}
			else
			{
				// Override extDB2 Location -- Randomized Search
				#ifdef _WIN32
					config_path = config_path.parent_path();
					search(config_path, conf_found, conf_randomized);
				#endif
			}
		}
		else
		{
			// extDB2 Shared Library Location
			config_path = shared_library_path;
			config_path = config_path.parent_path();
			config_path /= "extdb-conf.ini";
			if (boost::filesystem::is_regular_file(config_path))
			{
				conf_found = true;
				ext_info.path = config_path.parent_path().string();
			}
			// extDB2 Arma3 Location
			else if (boost::filesystem::is_regular_file("extdb-conf.ini"))
			{
				conf_found = true;
				config_path = boost::filesystem::path("extdb-conf.ini");
				ext_info.path = config_path.parent_path().string();
			}
			else
			{
				#ifdef _WIN32	// Windows Only, Linux Arma2 Doesn't have extension Support
					// Search for Randomize Config File -- Legacy Security Support For Arma2Servers

					config_path = config_path.parent_path();
					// CHECK DLL PATH FOR CONFIG)
					if (!config_path.string().empty())
					{
						search(config_path, conf_found, conf_randomized);
					}

					// CHECK ARMA ROOT DIRECTORY FOR CONFIG
					if (!conf_found)
					{
						config_path = boost::filesystem::current_path().string();
						search(config_path, conf_found, conf_randomized);
					}
				#endif
			}
		}

		if (conf_found)
		{
			pConf = new Poco::Util::IniFileConfiguration(config_path.make_preferred().string());
			ext_info.logger_flush = pConf->getBool("Log.Flush", true);

			#ifdef _WIN32	// Windows Only, Linux Arma2 Doesn't have extension Support
				// Search for Randomize Config File -- Legacy Security Support For Arma2Servers

				if ((pConf->getBool("Main.Randomize Config File", false)) && (!conf_randomized))
				// Only Gonna Randomize Once, Keeps things Simple
				{
					std::string chars("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
									  "1234567890");
					// Skipping Lowercase, this function only for arma2 + extensions only available on windows.
					boost::random::random_device rng;
					boost::random::uniform_int_distribution<> index_dist(0, chars.size() - 1);

					std::string randomized_filename = "extdb-conf-";
					for (int i = 0; i < 8; ++i)
					{
						randomized_filename += chars[index_dist(rng)];
					}
					randomized_filename += ".ini";

					boost::filesystem::path randomize_configfile_path = config_path.parent_path() /= randomized_filename;
					boost::filesystem::rename(config_path, randomize_configfile_path);
				}
			#endif
		}

		// Initialize Loggers
		//		Console Logger
		//size_t q_size = 1048576; //queue size must be power of 2
		//spdlog::set_async_mode(q_size);

		#ifdef DEBUG_TESTING
			auto console_temp = spdlog::stdout_logger_mt("extDB Console logger");
			console.swap(console_temp);
		#endif

		//		File Logger
		Poco::DateTime current_dateTime;

		boost::filesystem::path log_relative_path;
		log_relative_path = boost::filesystem::path(ext_info.path);
		log_relative_path /= "extDB";
		log_relative_path /= "logs";
		log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%Y");
		log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%n");
		log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%d");

		ext_info.log_path = log_relative_path.make_preferred().string();
		boost::filesystem::create_directories(log_relative_path);

		log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%H-%M-%S");

		logger = spdlog::rotating_logger_mt("extDB2 File Logger", log_relative_path.make_preferred().string(), 1048576 * 100, 3, ext_info.logger_flush);

		spdlog::set_level(spdlog::level::info);
		spdlog::set_pattern("%v");


		logger->info("extDB2: Version: {0}", EXTDB_VERSION);
		logger->info("extDB2: https://github.com/Torndeco/extDB2");
		#ifdef __GNUC__
			#ifndef DEBUG_TESTING
				logger->info("extDB2: Linux Version");
			#else
				logger->info("extDB2: Linux Debug Version");
			#endif
		#endif

		#ifdef _MSC_VER
			#ifndef DEBUG_LOGGING
				logger->info("extDB2: Windows Version");
			#else
				logger->info("extDB2: Windows Debug Version");
				logger->info();
			#endif
		#endif

		#ifdef TEST_APP
			console->info("Welcome to extDB2 Test Application");
			console->info("OutputSize is set to 80 for Test Application, just so it is readable");
			console->info("OutputSize for Arma3 is more like 10k in size ");
			console->info();
			console->info("Typing test will spam 1:SQL:TEST<1-5>:testing");
			console->info("This is used for poor man stress testing");
			console->info();
			console->info("Type 'test' for spam test");
			console->info("Type 'quit' to exit");
		#else
			logger->info("Message: All development for extDB2 is done on a Linux Dedicated Server");
			logger->info("Message: If you would like to Donate to extDB2 Development");
			logger->info("Message: https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=2SUEFTGABTAM2");
			logger->info("Message: Also leave a message if there is any particular feature you would like to see added.");
			logger->info("Message: Thanks for all the people that have donated.");
			logger->info("Message: Torndeco: 20/02/15");
			logger->info();
			logger->info();
		#endif

		if (!conf_found)
		{
			std::cout << "extDB2: Unable to find extdb-conf.ini" << std::endl;
			logger->critical("extDB2: Unable to find extdb-conf.ini");
			// Kill Server no config file found -- Evil
			std::exit(EXIT_SUCCESS);
		}
		else
		{
			#ifdef DEBUG_TESTING
				console->info("extDB2: Found extdb-conf.ini");
			#endif
			logger->info("extDB2: Found extdb-conf.ini");

			if ((pConf->getInt("Main.Version", 0) != EXTDB_CONF_VERSION))
			{
				logger->critical("extDB2: Incompatiable Config Version: {0},  Required Version: {1}", (pConf->getInt("Main.Version", 0)), EXTDB_CONF_VERSION);
				std::cout << "extDB2: extDB2: Incompatiable Config Version" << std::endl;
				// Kill Server if wrong config version -- Evil
				std::exit(EXIT_SUCCESS);
			}

			// Start Threads + ASIO
			ext_info.max_threads = pConf->getInt("Main.Threads", 0);
			int detected_cpu_cores = boost::thread::hardware_concurrency();
			if (ext_info.max_threads <= 0)
			{
				// Auto-Detect
				if (detected_cpu_cores > 6)
				{
					#ifdef DEBUG_TESTING
						console->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 6);
					#endif
					logger->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 6);
					ext_info.max_threads = 6;
				}
				else if (detected_cpu_cores <= 2)
				{
					#ifdef DEBUG_TESTING
						console->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 2);
					#endif
					logger->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 2);
					ext_info.max_threads = 2;
				}
				else
				{
					ext_info.max_threads = detected_cpu_cores;
					#ifdef DEBUG_TESTING
						console->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, ext_info.max_threads);
					#endif
					logger->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, ext_info.max_threads);
				}
			}
			else if (ext_info.max_threads > 8)  // Sanity Check
			{
				// Manual Config
				#ifdef DEBUG_TESTING
					console->info("extDB2: Sanity Check, Setting up {0} Worker Threads (config settings {1})", 8, ext_info.max_threads);
				#endif
				logger->info("extDB2: Sanity Check, Setting up {0} Worker Threads (config settings {1})", 8, ext_info.max_threads);
				ext_info.max_threads = 8;
			}
			else
			{
				// Manual Config
				#ifdef DEBUG_TESTING
					console->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads (config settings)", detected_cpu_cores, ext_info.max_threads);
				#endif
				logger->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads (config settings)", detected_cpu_cores, ext_info.max_threads);
			}

			// Setup ASIO Worker Pool
			io_work_ptr.reset(new boost::asio::io_service::work(io_service));
			for (int i = 0; i < ext_info.max_threads; ++i)
			{
				threads.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));
			}

			// Initialize so have atomic setup correctly + Setup VAC Ban Logger
			steam.init(this, ext_info.path, current_dateTime);
		}

		logger->info();
		logger->info();

		#ifdef _WIN32
			spdlog::set_pattern("[%H:%M:%S:%f %z] [Thread %t] %v");
		#else
			spdlog::set_pattern("[%H:%M:%S %z] [Thread %t] %v");
		#endif

		// Unique Random Strings Characters
		random_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	}
	catch(const boost::filesystem::filesystem_error& e)
	{
		std::cout << "BOOST FILESYSTEM ERROR: " << e.what() << std::endl;
		std::exit(EXIT_FAILURE);
	}
	catch (spdlog::spdlog_ex& e)
	{
		std::cout << "SPDLOG ERROR: " <<  e.what() << std::endl;
		std::exit(EXIT_FAILURE);
	}
}


Ext::~Ext(void)
{
	stop();
}


void Ext::stop()
{
	#ifdef DEBUG_TESTING
		console->info("extDB2: Stopping ...");
	#endif
	logger->info("extDB2: Stopping ...");
	//io_work_ptr.reset();
	if (ext_connectors_info.belog_scanner)
	{
		belog_scanner.stop();
	}
	if (ext_connectors_info.rcon)
	{
		rcon->disconnect();
	}
	io_work_ptr.reset();
	rcon_io_work_ptr.reset();
	threads.join_all();
	rcon_threads.join_all();
	io_service.stop();
	rcon_io_service.stop();

	for (auto &database : ext_connectors_info.databases)
	{
		//database.second.sql_pool->shutdown();
	}
	if (ext_connectors_info.mysql)
	{
		//Poco::Data::MySQL::Connector::unregisterConnector();
	}
	if (ext_connectors_info.sqlite)
	{
		//Poco::Data::SQLite::Connector::unregisterConnector();
	}
	spdlog::drop_all();
}

#ifdef _WIN32
	void Ext::search(boost::filesystem::path &config_path, bool &conf_found, bool &conf_randomized)
	{
		std::regex expression("extdb-conf.*ini");
		for (boost::filesystem::directory_iterator it(config_path); it != boost::filesystem::directory_iterator(); ++it)
		{
			if (boost::filesystem::is_regular_file(it->path()))
			{
				if(std::regex_search(it->path().string(), expression))
				{
					conf_found = true;
					conf_randomized = true;
					config_path = boost::filesystem::path(it->path().string());
					ext_info.path = config_path.parent_path().string();
					break;
				}
			}
		}
	}
#endif


Poco::Data::Session Ext::getDBSession_mutexlock(AbstractExt::DBConnectionInfo &database)
// Gets available DB Session (mutex lock)
{
	std::lock_guard<std::mutex> lock(database.mutex_sql_pool);
	return database.sql_pool->get();
}


Poco::Data::Session Ext::getDBSession_mutexlock(AbstractExt::DBConnectionInfo &database, Poco::Data::SessionPool::SessionDataPtr &session_data_ptr)
// Gets available DB Session (mutex lock) + Cached Statemetns
{
	std::lock_guard<std::mutex> lock(database.mutex_sql_pool);
	return database.sql_pool->get(session_data_ptr);
}


void Ext::createPlayerKey_mutexlock(std::string &player_beguid, int len_of_key)
{
	std::string player_unique_key;
	int num_of_retrys = 0;
	int i = 0;
	{
		std::lock_guard<std::mutex> lock(mutex_RandomString);
		boost::random::uniform_int_distribution<> index_dist(0, random_chars.size() - 1);

		while (true)
		{
			std::stringstream random_stream;
			for (int x = 0; x < len_of_key; ++x)
			{
				random_stream << random_chars[index_dist(random_chars_rng)];
			}
			player_unique_key = random_stream.str();

			if (std::find(uniqueRandomVarNames.begin(), uniqueRandomVarNames.end(), player_unique_key) != uniqueRandomVarNames.end())
			{
				if (num_of_retrys >= 10)
				{
					num_of_retrys = 0;
					++len_of_key; // Increase Random String Length if we failed 10 times
				}
				else
				{
					++num_of_retrys;
				}
			}
			else
			{
				uniqueRandomVarNames.push_back(player_unique_key);
				break;
			}
		}
	}
	logger->info("Player Unique ID: {0}", player_unique_key);
	{
		std::lock_guard<std::mutex> lock(player_unique_keys_mutex);
		player_unique_keys[player_beguid].keys.push_back(std::move(player_unique_key));

		// Create Regex Rule
		player_unique_keys[player_beguid].regex_rule.clear();
		for (auto &key : player_unique_keys[player_beguid].keys)
		{
			player_unique_keys[player_beguid].regex_rule += key + "|";
		}
		if (!(player_unique_keys[player_beguid].regex_rule.empty()))
		{
			player_unique_keys[player_beguid].regex_rule.pop_back();
		}
		player_unique_keys[player_beguid].regex_rule = "(" + player_unique_keys[player_beguid].regex_rule + ")";
	}
}


void Ext::delPlayerKey_delayed(std::string &player_beguid)
{
	std::lock_guard<std::mutex> lock(player_unique_keys_mutex);

	logger->info("Removed Player Timer for BEGUID: {0}", player_beguid);

	int diff_time = boost::posix_time::time_duration((boost::posix_time::microsec_clock::local_time() + boost::posix_time::seconds(30)) - timer->expires_at()).total_seconds();
	if (diff_time <= 0)
	{
		del_players_keys.push_back(std::make_pair(30, player_beguid));
		timer->expires_from_now(boost::posix_time::seconds(30));
		timer->async_wait(boost::bind(&Ext::delPlayerKey_mutexlock, this));
	}
	else
	{
		std::size_t wait_time = 30 - diff_time;
		del_players_keys.push_back(std::make_pair(wait_time, player_beguid));
	}
}


void Ext::delPlayerKey_mutexlock()
{
	std::lock_guard<std::mutex> lock(player_unique_keys_mutex);

	logger->info("Removed Player Unique ID ");

	while (true)
	{
		if (del_players_keys.size() == 0)
		{
			break;
		}
		if (del_players_keys.front().first > 1)
		{
			timer->expires_from_now(boost::posix_time::seconds(del_players_keys.front().first));
			timer->async_wait(boost::bind(&Ext::delPlayerKey_mutexlock, this));
			break;
		}
		else
		{
			if (player_unique_keys.count(del_players_keys.front().second))
			{
				player_unique_keys[del_players_keys.front().second].keys.pop_front();
				if (player_unique_keys[del_players_keys.front().second].keys.empty())
				{
					player_unique_keys.erase(del_players_keys.front().second);
				}
				logger->info("Removed Player Unique ID for BEGUID 2: {0}", del_players_keys.front().second);
			}
			del_players_keys.pop_front();
		}
	}
}


void Ext::getPlayerKey_BEGuid(std::string &player_beguid, std::string &player_key)
{
	std::lock_guard<std::mutex> lock(player_unique_keys_mutex);
	if (player_unique_keys.count(player_beguid))
	{
		player_key = player_unique_keys[player_beguid].keys.back();
	}
}


std::string Ext::getPlayerRegex_BEGuid(std::string &player_beguid)
{
	std::lock_guard<std::mutex> lock(player_unique_keys_mutex);
	if (player_unique_keys.count(player_beguid))
	{
		return player_unique_keys[player_beguid].regex_rule;
	}
	else
	{
		return "";
	}
}


void Ext::getPlayerKey_SteamID(std::string &player_steam_id, std::string &player_key)
{
	Poco::Int64 steamID = Poco::NumberParser::parse64(player_steam_id);
	Poco::Int8 i = 0;
	Poco::Int8 parts[8] = { 0 };

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

	std::string player_beguid;
	{
		std::lock_guard<std::mutex> lock(mutex_md5);
		md5.update(bestring.str());
		player_beguid = Poco::DigestEngine::digestToHex(md5.digest());
	}

	std::lock_guard<std::mutex> lock(player_unique_keys_mutex);
	if (player_unique_keys.count(player_beguid))
	{
		player_key = player_unique_keys[player_beguid].keys.back();
	}
}


void Ext::steamQuery(const unsigned int &unique_id, bool queryFriends, bool queryVacBans, std::string &steamID, bool wakeup)
// Adds Query to Steam Protocol, wakeup option is to wakeup steam thread. Note: Steam thread periodically checks every minute anyway.
{
	std::vector<std::string> steamIDs;
	steamIDs.push_back(steamID);
	steam.addQuery(unique_id, queryFriends, queryVacBans, steamIDs);
	if (wakeup)
	{
		steam_thread.wakeUp();
	}
}


void Ext::steamQuery(const unsigned int &unique_id, bool queryFriends, bool queryVacBans, std::vector<std::string> &steamIDs, bool wakeup)
// Adds Query to Steam Protocol, wakeup option is to wakeup steam thread. Note: Steam thread periodically checks every minute anyway.
{
	steam.addQuery(unique_id, queryFriends, queryVacBans, steamIDs);
	if (wakeup)
	{
		steam_thread.wakeUp();
	}
}


void Ext::startBELogscanner(char *output, const std::string &conf)
{
	if (pConf->getBool(conf + ".Enable", false))
	{
		if (!ext_connectors_info.belog_scanner)
		{
			belog_scanner.start(this, io_service);
			ext_connectors_info.belog_scanner = true;
			std::strcpy(output, ("[1]"));
		}
		else
		{
			std::strcpy(output, ("[0,\"BELogscanner Already Started\"]"));
		}
	}
	else
	{
		std::strcpy(output, ("[0,\"BELogscanner Disabled in Config\"]"));
	}
}


void Ext::startRcon(char *output, const std::string &conf, std::vector<std::string> &extra_rcon_options)
// Start RCon
{
	if (ext_connectors_info.rcon)
	{
		std::strcpy(output, ("[0,\"Rcon is Already Running\"]"));
	}
	else if (pConf->hasOption(conf + ".password"))
	{
		if (!rcon_io_work_ptr)
		{
			// Setup ASIO Worker Pool
			rcon_io_work_ptr.reset(new boost::asio::io_service::work(rcon_io_service));
			rcon_threads.create_thread(boost::bind(&boost::asio::io_service::run, &rcon_io_service));

			// Initialize so have atomic setup correctly
			rcon.reset(new Rcon(rcon_io_service, logger));
			rcon->extInit(this);
		}
		#ifdef DEBUG_TESTING
			console->info("extDB2: Loading Rcon Config");
		#endif
		logger->info("extDB2: Loading Rcon Config");

		// Rcon Settings
		Rcon::RconSettings rcon_settings;
		rcon_settings.address = pConf->getString((conf + ".IP"), "127.0.0.1");
		rcon_settings.port = pConf->getInt((conf + ".Port"), 2302);
		rcon_settings.password = pConf->getString((conf + ".Password"), "password");

		for (auto &extra_rcon_option : extra_rcon_options)
		{
			if (boost::algorithm::iequals(extra_rcon_option, "PLAYERKEY") == 1)
			{
				rcon_settings.generate_unique_id = true;
			}
			else if (boost::algorithm::iequals(extra_rcon_option, "FULL_PLAYER_INFO") == 1)
			{
				rcon_settings.return_full_player_info = true;
			}
		}

		#ifdef DEBUG_TESTING
			console->info("extDB2: Loading Rcon IP: {0}, Port: {1}", rcon_settings.address, rcon_settings.port);
		#endif
		logger->info("extDB2: Loading Rcon IP: {0}, Port: {1}", rcon_settings.address, rcon_settings.port);

		// Bad Player Name
		Rcon::BadPlayernameSettings bad_playername_settings;
		bad_playername_settings.enable = pConf->getBool((conf + ".Bad Playername Enable"), false);

		if (bad_playername_settings.enable)
		{
			#ifdef DEBUG_TESTING
				console->info("extDB2: RCon Bad Playername Enabled");
			#endif
			logger->info("extDB2: RCon Bad Playername Enabled");

			bad_playername_settings.kick_message = pConf->getString(((conf) + ".Bad Playername Kick Message"), "");

			bad_playername_settings.bad_strings.push_back(":");
			Poco::StringTokenizer tokens(pConf->getString(((conf) + ".Bad Playername Strings"), ""), ":");
			for (auto &token : tokens)
			{
				bad_playername_settings.bad_strings.push_back(token);
			}

			int regrex_rule_num = 0;
			std::string regex_rule_num_str;
			while (true)
			{
				++regrex_rule_num;
				regex_rule_num_str = conf + ".Bad Playername Regex_" + Poco::NumberFormatter::format(regrex_rule_num);
				if (!(pConf->has(regex_rule_num_str)))
				{
					logger->info("extDB2: Missing {0}", regex_rule_num_str);
					break;
				}
				else
				{
					logger->info("extDB2: Loading {0}", regex_rule_num_str);
					bad_playername_settings.bad_regexs.push_back(pConf->getString(regex_rule_num_str));
				}
			}
		}

		// Reserved Slots
		Rcon::WhitelistSettings whitelist_settings;
		whitelist_settings.enable = pConf->getBool((conf + ".Whitelist Enable"), false);
		if (whitelist_settings.enable)
		{
			whitelist_settings.open_slots = pConf->getInt((conf + ".Whitelist Public Slots"), 0);
			whitelist_settings.kick_message = pConf->getString((conf + ".Whitelist Kick Message"), "");

			whitelist_settings.database = pConf->getString((conf + ".Whitelist Database"), "");
			whitelist_settings.sql_statement = pConf->getString((conf + ".Whitelist SQL Prepared Statement"), "");
			whitelist_settings.kick_on_failed_sql_query = pConf->getBool((conf + ".Whitelist Kick on SQL Query Failed"), false);

			Poco::StringTokenizer tokens(pConf->getString((conf + ".Whitelist BEGuids"), ""), ":", Poco::StringTokenizer::TOK_TRIM);
			for (auto &token : tokens)
			{
				whitelist_settings.whitelisted_guids.push_back(token);
			}
		}

		// Start Rcon
		timer.reset(new boost::asio::deadline_timer(io_service));
		rcon->start(rcon_settings, bad_playername_settings, whitelist_settings, pConf);
		ext_connectors_info.rcon = true;
		std::strcpy(output, "[1]");
	}
	else
	{
		std::strcpy(output, ("[0,\"No Config Option Found\"]"));
	}
}


void Ext::rconCommand(std::string input_str)
// Adds RCon Command to be sent to Server.
{
	rcon->sendCommand(input_str);
}


void Ext::rconAddBan(std::string input_str)
// Adds RCon Command to be sent to Server.
{
	rcon->addBan(input_str);
}


void Ext::rconMissions(unsigned int unique_id)
// Adds RCon Command to be sent to Server.
{
	rcon->getMissions(unique_id);
}


void Ext::rconPlayers(unsigned int unique_id)
// Adds RCon Command to be sent to Server.
{
	rcon->getPlayers(unique_id);
}


void Ext::getUPTime(std::string &token, std::string &result)
{
	if (token == "SECONDS")
	{
		result = "[1,[" + Poco::NumberFormatter::format(Poco::Timespan(timestamp.elapsed()).totalSeconds()) + "]]";
	} else if (token == "MINUTES") {
		result = "[1,[" + Poco::NumberFormatter::format(Poco::Timespan(timestamp.elapsed()).totalMinutes()) + "]]";
	} else if (token == "HOURS") {
		result = "[1,[" + Poco::NumberFormatter::format(Poco::Timespan(timestamp.elapsed()).totalHours()) + "]]";
	}
}


void Ext::getDateTime(const std::string &input_str, std::string &result)
{
	if (!(input_str.empty()))
	{
		if (!(Poco::NumberParser::tryParse(input_str, dateTime_offset)))
		{
			dateTime_offset = 0;
		}
	} else {
		dateTime_offset = 0;
	}

	dateTime = Poco::DateTime() + Poco::Timespan(dateTime_offset * Poco::Timespan::HOURS);
	result = "[1,[" + Poco::DateTimeFormatter::format(dateTime, "%Y,%n,%d,%H,%M") + "]]";
}


void Ext::getLocalDateTime(std::string &result)
{
	result = "[1,[" + Poco::DateTimeFormatter::format(Poco::DateTime(), "%Y,%n,%d,%H,%M") + "]]";
}


void Ext::getDateAdd(std::string& time1, std::string& input_str, std::string &result)
{
	input_str.erase(input_str.begin());
	input_str.pop_back();
	Poco::StringTokenizer tokens(input_str, ",");
	if (tokens.count() == 4)
	{
		int days;
		int hours;
		int minutes;
		int seconds;
		int microSeconds = 0;

		if (!(Poco::NumberParser::tryParse(tokens[0], days)))
		{
			days = 0;
		}
		if (!(Poco::NumberParser::tryParse(tokens[1], hours)))
		{
			hours = 0;
		}
		if (!(Poco::NumberParser::tryParse(tokens[2], minutes)))
		{
			minutes = 0;
		}
		if (!(Poco::NumberParser::tryParse(tokens[3], seconds)))
		{
			seconds = 0;
		}
		timespan = Poco::Timespan(days,hours,minutes,seconds,microSeconds);
		Poco::DateTimeParser::parse(timeDiff_fmt, time1, dateTime, timeDiff_zoneDiff);
		dateTime = dateDiffTime_1 + timespan;
		result = "[1,[" + Poco::DateTimeFormatter::format(dateTime, "%Y,%n,%d,%H,%M") + "]]";
	};
}


void Ext::getCurrentTimeDiff(std::string &type, std::string& time1, std::string &result)
{
	Poco::DateTimeParser::parse(timeDiff_fmt, time1, dateDiffTime_1, timeDiff_zoneDiff);
	timespan = dateDiffTime_1 - Poco::DateTime();

	if (type == "ALL") {
		result = "[1,[" + Poco::DateTimeFormatter::format(timespan, "%d,%H,%M") + "]]";
	} else if (type == "DAYS") {
		result = "[1," + Poco::NumberFormatter::format(timespan.days())+ "]";
	} else if (type == "HOURS") {
		result = "[1," + Poco::NumberFormatter::format(timespan.totalHours())+ "]";
	} else if (type == "MINUTES") {
		result = "[1," + Poco::NumberFormatter::format(timespan.totalMinutes())+ "]";
	} else if (type == "SECONDS") {
		result = "[1," + Poco::NumberFormatter::format(timespan.totalSeconds())+ "]";
	};
}

void Ext::getCurrentLocalTimeDiff(std::string &type, std::string& time1, std::string &result)
{
	Poco::DateTimeParser::parse(timeDiff_fmt, time1, dateDiffTime_1, timeDiff_zoneDiff);
	timespan = dateDiffTime_1 - Poco::LocalDateTime().utc();

	if (type == "ALL") {
		result = "[1,[" + Poco::DateTimeFormatter::format(timespan, "%d,%H,%M") + "]]";
	} else if (type == "DAYS") {
		result = "[1," + Poco::NumberFormatter::format(timespan.days())+ "]";
	} else if (type == "HOURS") {
		result = "[1," + Poco::NumberFormatter::format(timespan.totalHours())+ "]";
	} else if (type == "MINUTES") {
		result = "[1," + Poco::NumberFormatter::format(timespan.totalMinutes())+ "]";
	} else if (type == "SECONDS") {
		result = "[1," + Poco::NumberFormatter::format(timespan.totalSeconds())+ "]";
	};
}


void Ext::getCurrentTimeDiff(std::string &type, std::string &time1, std::string &offset, std::string &result)
{
	if (!(offset.empty()))
	{
		if (!(Poco::NumberParser::tryParse(offset, dateTime_offset)))
		{
			dateTime_offset = 0;
		}
	} else {
		dateTime_offset = 0;
	}

	Poco::DateTimeParser::parse(timeDiff_fmt, time1, dateDiffTime_1, timeDiff_zoneDiff);
	timespan = dateDiffTime_1 - (Poco::DateTime() + Poco::Timespan(dateTime_offset * Poco::Timespan::HOURS));

	if (type == "ALL") {
		result = "[1,[" + Poco::DateTimeFormatter::format(timespan, "%d,%H,%M") + "]]";
	} else if (type == "DAYS") {
		result = "[1," + Poco::NumberFormatter::format(timespan.days())+ "]";
	} else if (type == "HOURS") {
		result = "[1," + Poco::NumberFormatter::format(timespan.totalHours())+ "]";
	} else if (type == "MINUTES") {
		result = "[1," + Poco::NumberFormatter::format(timespan.totalMinutes())+ "]";
	} else if (type == "SECONDS") {
		result = "[1," + Poco::NumberFormatter::format(timespan.totalSeconds())+ "]";
	};
}


void Ext::getTimeDiff(std::string &type, std::string& time1, std::string& time2, std::string &result)
{
	Poco::DateTimeParser::parse(timeDiff_fmt, time1, dateDiffTime_1, timeDiff_zoneDiff);
	Poco::DateTimeParser::parse(timeDiff_fmt, time2, dateDiffTime_2, timeDiff_zoneDiff);
	timespan = dateDiffTime_1 - dateDiffTime_2;

	if (type == "ALL") {
		result = "[1,[" + Poco::DateTimeFormatter::format(timespan, "%d,%H,%M") + "]]";
	} else if (type == "DAYS") {
		result = "[1," + Poco::NumberFormatter::format(timespan.days())+ "]";
	} else if (type == "HOURS") {
		result = "[1," + Poco::NumberFormatter::format(timespan.totalHours())+ "]";
	} else if (type == "MINUTES") {
		result = "[1," + Poco::NumberFormatter::format(timespan.totalMinutes())+ "]";
	} else if (type == "SECONDS") {
		result = "[1," + Poco::NumberFormatter::format(timespan.totalSeconds())+ "]";
	};
}


void Ext::getUniqueString(int &len_of_string, int &num_of_strings, std::string &result)
{
	int num_of_retrys = 0;
	std::string random_string;

	std::lock_guard<std::mutex> lock(mutex_RandomString);
	boost::random::uniform_int_distribution<> index_dist(0, random_chars.size() - 1);

	result = "[";
	int i = 0;
	while (i < num_of_strings)
	{
		std::stringstream random_stream;
		for(int x = 0; x < len_of_string; ++x)
		{
			random_stream << random_chars[index_dist(random_chars_rng)];
		}
		random_string = random_stream.str();

		if (std::find(uniqueRandomVarNames.begin(), uniqueRandomVarNames.end(), random_string) != uniqueRandomVarNames.end())
		{
			if (num_of_retrys >= 10)
			{
				num_of_retrys = 0;
				++len_of_string; // Increase Random String Length if we tried 10 times + failed
			}
			else
			{
				++num_of_retrys;
			}
		}
		else
		{
			if (i == 0)
			{
				result =+ "\"" + random_string + "\"";
			}
			else
			{
				result =+ ",\"" + random_string + "\"";
			}
			uniqueRandomVarNames.push_back(random_string);
			++i;
		}
	}
	result =+ "]";
}


void Ext::connectDatabase(char *output, const std::string &database_conf, const std::string &database_id)
// Connection to Database, database_id used when connecting to multiple different database.
{
	if (!database_conf.empty())
	{
		DBConnectionInfo *database = &ext_connectors_info.databases[database_id];

		bool connected = true;

		// Check if already connected to Database.
		if (!database->type.empty())
		{
			#ifdef DEBUG_TESTING
				console->warn("extDB2: Already Connected to Database");
			#endif
			logger->warn("extDB2: Already Connected to a Database");
			std::strcpy(output, "[0,\"Already Connected to Database\"]");
		}
		else if (pConf->hasOption(database_conf + ".Type"))
		{
			database->type = pConf->getString(database_conf + ".Type");
			#ifdef DEBUG_TESTING
				console->info("extDB2: Database Type: {0}", database->type);
			#endif
			logger->info("extDB2: Database Type: {0}", database->type);


			if ((boost::algorithm::iequals(database->type, std::string("MySQL")) == 1) || (boost::algorithm::iequals(database->type, "SQLite") == 1))
			{
				try
				{
					// Database
					std::string connection_str;
					if (boost::algorithm::iequals(database->type, std::string("MySQL")) == 1)
					{
						database->type = "MySQL";
						if (!(ext_connectors_info.mysql))
						{
							Poco::Data::MySQL::Connector::registerConnector();
							ext_connectors_info.mysql = true;
						}
						connection_str += "host=" + pConf->getString(database_conf + ".IP") + ";";
						connection_str += "port=" + pConf->getString(database_conf + ".Port") + ";";
						connection_str += "user=" + pConf->getString(database_conf + ".Username") + ";";
						connection_str += "password=" + pConf->getString(database_conf + ".Password") + ";";
						connection_str += "db=" + pConf->getString(database_conf + ".Name") + ";";
						connection_str += "auto-reconnect=true";

						if (pConf->getBool(database_conf + ".Compress", false))
						{
							connection_str += ";compress=true";
						}
						if (pConf->getBool(database_conf + ".Secure Auth", false))
						{
							connection_str += ";secure-auth=true";
						}
					}
					else if (boost::algorithm::iequals(database->type, "SQLite") == 1)
					{
						database->type = "SQLite";
						if (!(ext_connectors_info.sqlite))
						{
							Poco::Data::SQLite::Connector::registerConnector();
							ext_connectors_info.sqlite = true;
						}

						boost::filesystem::path sqlite_path(ext_info.path);
						sqlite_path /= "extDB";
						sqlite_path /= "sqlite";
						sqlite_path /= pConf->getString(database_conf + ".Name");
						connection_str = sqlite_path.make_preferred().string();
					}
					database->sql_pool.reset(new Poco::Data::SessionPool(database->type,
																		connection_str,
																		pConf->getInt(database_conf + ".minSessions", 1),
																		pConf->getInt(database_conf + ".maxSessions", ext_info.max_threads),
																		pConf->getInt(database_conf + ".idleTime", 600)));
					if (database->sql_pool->get().isConnected())
					{
						#ifdef DEBUG_TESTING
							console->info("extDB2: Database Session Pool Started");
						#endif
						logger->info("extDB2: Database Session Pool Started");
						std::strcpy(output, "[1]");
					}
					else
					{
						#ifdef DEBUG_TESTING
							console->warn("extDB2: Database Session Pool Failed");
						#endif
						logger->warn("extDB2: Database Session Pool Failed");
						std::strcpy(output, "[0,\"Database Session Pool Failed\"]");
						connected = false;
					}
				}
				catch (Poco::Data::NotConnectedException& e)
				{
					#ifdef DEBUG_TESTING
						console->error("extDB2: Database NotConnectedException Error: {0}", e.displayText());
					#endif
					logger->error("extDB2: Database NotConnectedException Error: {0}", e.displayText());
					std::strcpy(output, "[0,\"Database NotConnectedException Error\"]");
					connected = false;
				}
				catch (Poco::Data::MySQL::ConnectionException& e)
				{
					#ifdef DEBUG_TESTING
						console->error("extDB2: Database ConnectionException Error: {0}", e.displayText());
					#endif
					logger->error("extDB2: Database ConnectionException Error: {0}", e.displayText());
					std::strcpy(output, "[0,\"Database ConnectionException Error\"]");
					connected = false;
				}
				catch (Poco::Exception& e)
				{
					#ifdef DEBUG_TESTING
						console->error("extDB2: Database Exception Error: {0}", e.displayText());
					#endif
					logger->error("extDB2: Database Exception Error: {0}", e.displayText());
					std::strcpy(output, "[0,\"Database Exception Error\"]");
					connected = false;
				}
			}
			else
			{
				#ifdef DEBUG_TESTING
					console->warn("extDB2: No Database Engine Found for {0}", database->type);
				#endif
				logger->warn("extDB2: No Database Engine Found for {0}", database->type);
				std::strcpy(output, "[0,\"Unknown Database Type\"]");
				connected = false;
			}
		}
		else
		{
			#ifdef DEBUG_TESTING
				console->warn("extDB2: No Config Option Found: {0}", database_conf);
			#endif
			logger->warn("extDB2: No Config Option Found: {0}", database_conf);
			std::strcpy(output, "[0,\"No Config Option Found\"]");
			connected = false;
		}

		if (!connected)
		{
			ext_connectors_info.databases.erase(database_id);
		}
	}
	else
	{
		#ifdef DEBUG_TESTING
			console->warn("extDB2: No Config Option Found: {0}", database_conf);
		#endif
		logger->warn("extDB2: No Config Option Found: {0}", database_conf);
		std::strcpy(output, "[0,\"No Config Option Found\"]");
	}
}


void Ext::addProtocol(char *output, const std::string &database_id, const std::string &protocol, const std::string &protocol_name, const std::string &init_data)
{
	std::lock_guard<std::mutex> lock(mutex_unordered_map_protocol);
	if (unordered_map_protocol.find(protocol_name) != unordered_map_protocol.end())
	{
		std::strcpy(output, "[0,\"Error Protocol Name Already Taken\"]");
		logger->warn("extDB2: Error Protocol Name Already Taken: {0}", protocol_name);
	}
	else
	{
		bool status = true;
		if (database_id.empty())
		{
			if (boost::algorithm::iequals(protocol, std::string("LOG")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new LOG());
			}
			else if (boost::algorithm::iequals(protocol, std::string("MISC")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new MISC());
			}
			else if (boost::algorithm::iequals(protocol, std::string("RCON")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new RCON());
			}
			else if (boost::algorithm::iequals(protocol, std::string("STEAM_V2")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new STEAM_V2());
			}
			else
			{
				status = false;
				std::strcpy(output, "[0,\"Error Unknown Protocol\"]");
				logger->warn("extDB2: Failed to Load Unknown Protocol: {0}", protocol);
			}
		}
		else
		{
			if (boost::algorithm::iequals(protocol, std::string("SQL_CUSTOM_V2")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new SQL_CUSTOM_V2());
			}
			else if (boost::algorithm::iequals(protocol, std::string("SQL_RAW_V2")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new SQL_RAW_V2());
			}
			else
			{
				status = false;
				std::strcpy(output, "[0,\"Error Unknown Protocol\"]");
				logger->warn("extDB2: Failed to Load Unknown Protocol: {0}", protocol);
			}
		}

		if (status)
		{
			if (unordered_map_protocol[protocol_name].get()->init(this, database_id, init_data))
			{
				std::strcpy(output, "[1]");
			}
			else
			{
				unordered_map_protocol.erase(protocol_name);
				std::strcpy(output, "[0,\"Failed to Load Protocol\"]");
				logger->warn("extDB2: Failed to Load Protocol: {0}", protocol);
			}
		}
	}
}


void Ext::getSinglePartResult_mutexlock(char *output, const int &output_size, const unsigned int &unique_id)
// Gets Result String from unordered map array -- Result Formt == Single-Message
//   If <=, then sends output to arma, and removes entry from unordered map array
//   If >, sends [5] to indicate MultiPartResult
{
	std::lock_guard<std::mutex> lock(mutex_results);

	auto const_itr = stored_results.find(unique_id);
	if (const_itr == stored_results.end()) // NO UNIQUE ID
	{
		std::strcpy(output, "");
	}
	else // SEND MSG (Part)
	{
		if (const_itr->second.wait) // WAIT
		{
			std::strcpy(output, "[3]");
		}
		else if (const_itr->second.message.length() > output_size)
		{
			std::strcpy(output, "[5]");
		}
		else
		{
			std::strcpy(output, const_itr->second.message.c_str());
			stored_results.erase(const_itr);
		}
	}
}


void Ext::getMultiPartResult_mutexlock(char *output, const int &output_size, const unsigned int &unique_id)
// Gets Result String from unordered map array  -- Result Format = Multi-Message
//   If length of String = 0, sends arma "", and removes entry from unordered map array
//   If <=, then sends output to arma
//   If >, then sends 1 part to arma + stores rest.
{
	std::lock_guard<std::mutex> lock(mutex_results);

	auto const_itr = stored_results.find(unique_id);
	if (const_itr == stored_results.end()) // NO UNIQUE ID or WAIT
	{
		std::strcpy(output, "");
	}
	else if (const_itr->second.wait)
	{
		std::strcpy(output, "[3]");
	}
	else if (const_itr->second.message.empty()) // END of MSG
	{
		stored_results.erase(const_itr);
		std::strcpy(output, "");
	}
	else // SEND MSG (Part)
	{
		if (const_itr->second.message.length() > output_size)
		{
			std::strcpy(output, const_itr->second.message.substr(0, output_size).c_str());
			const_itr->second.message = const_itr->second.message.substr(output_size);
		}
		else
		{
			std::strcpy(output, const_itr->second.message.c_str());
			const_itr->second.message.clear();
		}
	}
}


const unsigned int Ext::saveResult_mutexlock(const resultData &result_data)
// Stores Result String and returns Unique ID, used by SYNC Calls where message > outputsize
{
	std::lock_guard<std::mutex> lock(mutex_results);
	const unsigned int unique_id = unique_id_counter++;
	stored_results[unique_id] = std::move(result_data);
	stored_results[unique_id].wait = false;
	return unique_id;
}


void Ext::saveResult_mutexlock(const unsigned int &unique_id, const resultData &result_data)
// Stores Result String for Unique ID
{
	std::lock_guard<std::mutex> lock(mutex_results);
	stored_results[unique_id] = std::move(result_data);
	stored_results[unique_id].wait = false;
}


void Ext::saveResult_mutexlock(std::vector<unsigned int> &unique_ids, const resultData &result_data)
// Stores Result for multiple Unique IDs (used by Rcon Backend)
{
	std::lock_guard<std::mutex> lock(mutex_results);
	for (auto &unique_id : unique_ids)
	{
		stored_results[unique_id] = result_data;
		stored_results[unique_id].wait = false;
	}
}


void Ext::syncCallProtocol(char *output, const int &output_size, std::string &input_str)
// Sync callPlugin
{
	const std::string::size_type found = input_str.find(":",2);

	if ((found==std::string::npos) || (found == (call_extension_input_str_length - 1)))
	{
		std::strcpy(output, "[0,\"Error Invalid Format\"]");
		logger->error("extDB2: Invalid Format: {0}", input_str);
	}
	else
	{
		auto const_itr = unordered_map_protocol.find(input_str.substr(2,(found-2)));
		if (const_itr == unordered_map_protocol.end())
		{
			std::strcpy(output, "[0,\"Error Unknown Protocol\"]");
		}
		else
		{
			resultData result_data;
			result_data.message.reserve(output_size);

			const_itr->second->callProtocol(input_str.substr(found+1), result_data.message, false);
			if (result_data.message.length() <= output_size)
			{
				std::strcpy(output, result_data.message.c_str());
			}
			else
			{
				const unsigned int unique_id = saveResult_mutexlock(result_data);
				std::strcpy(output, ("[2,\"" + Poco::NumberFormatter::format(unique_id) + "\"]").c_str());
			}
		}
	}
}


void Ext::onewayCallProtocol(std::string &input_str)
// ASync callProtocol
{
	const std::string::size_type found = input_str.find(":",2);
	if ((found==std::string::npos) || (found == (input_str.size() - 1)))
	{
		logger->error("extDB2: Invalid Format: {0}", input_str);
	}
	else
	{
		auto const_itr = unordered_map_protocol.find(input_str.substr(2,(found-2)));
		if (const_itr != unordered_map_protocol.end())
		{
			resultData result_data;
			const_itr->second->callProtocol(input_str.substr(found+1), result_data.message, true);
		}
	}
}


void Ext::asyncCallProtocol(const int &output_size, const std::string &protocol, const std::string &data, const unsigned int unique_id)
// ASync + Save callProtocol
// We check if Protocol exists here, since its a thread (less time spent blocking arma) and it shouldnt happen anyways
{
	resultData result_data;
	result_data.message.reserve(output_size);
	if (unordered_map_protocol[protocol].get()->callProtocol(data, result_data.message, true, unique_id))
	{
		saveResult_mutexlock(unique_id, result_data);
	}
}


void Ext::callExtension(char *output, const int &output_size, const char *function)
{
	try
	{
		#ifdef DEBUG_LOGGING
			logger->info("extDB2: Input from Server: {0}", std::string(function));
		#endif

		std::string input_str(function);
		call_extension_input_str_length = input_str.length();

		if (call_extension_input_str_length <= 2)
		{
			std::strcpy(output, "[0,\"Error Invalid Message\"]");
			logger->info("extDB2: Invalid Message: {0}", input_str);
		}
		else
		{
			// Async / Sync
			switch (input_str[0])
			{
				case '1': //ASYNC
				{
					io_service.post(boost::bind(&Ext::onewayCallProtocol, this, std::move(input_str)));
					break;
				}
				case '2': //ASYNC + SAVE
				{
					// Protocol
					const std::string::size_type found = input_str.find(":", 2);

					if ((found==std::string::npos) || (found == (call_extension_input_str_length - 1)))
					{
						std::strcpy(output, "[0,\"Error Invalid Format\"]");
						logger->error("extDB2: Error Invalid Format: {0}", input_str);
					}
					else
					{
						// Check for Protocol Name Exists...
						// Do this so if someone manages to get server, the error message wont get stored in the result unordered map
						const std::string protocol = input_str.substr(2,(found-2));
						if (unordered_map_protocol.find(protocol) != unordered_map_protocol.end()) //TODO Change to ITER
						{
							unsigned int unique_id;
							{
								std::lock_guard<std::mutex> lock(mutex_results);
								unique_id = unique_id_counter++;
								stored_results[unique_id].wait = true;
							}
							io_service.post(boost::bind(&Ext::asyncCallProtocol, this, output_size, std::move(protocol), input_str.substr(found+1), std::move(unique_id)));
							std::strcpy(output, ("[2,\"" + Poco::NumberFormatter::format(unique_id) + "\"]").c_str());
						}
						else
						{
							std::strcpy(output, "[0,\"Error Unknown Protocol\"]");
							logger->error("extDB2: Error Unknown Protocol: {0}  Input String: {1}", protocol, input_str);
						}
					}
					break;
				}
				case '4': // GET -- Single-Part Message Format
				{
					const unsigned int unique_id = Poco::NumberParser::parse(input_str.substr(2));
					getSinglePartResult_mutexlock(output, output_size, unique_id);
					break;
				}
				case '5': // GET -- Multi-Part Message Format
				{
					const unsigned int unique_id = Poco::NumberParser::parse(input_str.substr(2));
					getMultiPartResult_mutexlock(output, output_size, unique_id);
					break;
				}
				case '0': //SYNC
				{
					syncCallProtocol(output, output_size, input_str);
					break;
				}
				case '9': // SYSTEM CALLS / SETUP
				{
					Poco::StringTokenizer tokens(input_str, ":");
					if (ext_info.extDB_lock)
					{
						switch (tokens.count())
						{
							case 2:
								if (tokens[1] == "VERSION")
								{
									std::strcpy(output, EXTDB_VERSION);
								}
								else if (tokens[1] == "LOCK_STATUS")
								{
									std::strcpy(output, "[1]");
								}
								else if (tokens[1] == "RCON_STATUS")
								{
									if (rcon->status())
									{
										std::strcpy(output, "[1]");
									}
									else
									{
										std::strcpy(output, "[0]");
									}
								}
								else if (tokens[1] == "TIME")
								{
									std::string result;
									getDateTime(std::string(), result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "LOCAL_TIME")
								{
									std::string result;
									getLocalDateTime(result);
									std::strcpy(output, result.c_str());
								}
								else
								{
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Error Invalid Format: {0}", input_str);
								}
								break;
							case 3:
								if (tokens[1] == "TIME")
								{
									std::string result;
									getDateTime(tokens[2], result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "UPTIME")
								{
									std::string result;
									getUPTime(tokens[2], result);
									std::strcpy(output, result.c_str());
								}
								break;
							case 4:
								if (tokens[1] == "TIMEDIFF_CURRENT")
								{
									std::string result;
									getCurrentTimeDiff(tokens[2],tokens[3],result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "TIMEDIFF_CURRENT_LOCAL")
								{
									std::string result;
									getCurrentTimeDiff(tokens[2],tokens[3],result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "DATEADD")
								{
									std::string result;
									getDateAdd(tokens[2],tokens[3],result);
									std::strcpy(output, result.c_str());
								}
								break;
							case 5:
								if (tokens[1] == "TIMEDIFF")
								{
									std::string result;
									getTimeDiff(tokens[2],tokens[3],tokens[4],result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "TIMEDIFF_CURRENT")
								{
									std::string result;
									getCurrentTimeDiff(tokens[2],tokens[3],tokens[4],result);
									std::strcpy(output, result.c_str());
								}
								break;
							default:
								// Invalid Format
								std::strcpy(output, "[0,\"Error Invalid Format\"]");
								logger->error("extDB2: Error Invalid Format: {0}", input_str);
						}
					}
					else
					{
						switch (tokens.count())
						{
							case 2:
								// VAC
								if (tokens[1] == "START_VAC")
								{
									if (!ext_connectors_info.steam)
									{
										ext_connectors_info.steam = true;
										steam_thread.start(steam);
										std::strcpy(output, "[1]");
									}
									else
									{
										std::strcpy(output, ("[0,\"Steam Already Started\"]"));
									}
								}
								// LOCK / VERSION
								else  if (tokens[1] == "VERSION")
								{
									std::strcpy(output, EXTDB_VERSION);
								}
								else if (tokens[1] == "LOCK")
								{
									ext_info.extDB_lock = true;
									std::strcpy(output, ("[1]"));
								}
								else if (tokens[1] == "LOCK_STATUS")
								{
									std::strcpy(output, "[0]");
								}
								else if (tokens[1] == "RCON_STATUS")
								{
									if (rcon->status())
									{
										std::strcpy(output, "[1]");
									}
									else
									{
										std::strcpy(output, "[0]");
									}
								}
								else if (tokens[1] == "OUTPUTSIZE")
								{
									std::string outputsize_str(Poco::NumberFormatter::format(output_size));
									std::strcpy(output, outputsize_str.c_str());
									logger->info("extDB2: Output Size: {0}", outputsize_str);
								}
								else if (tokens[1] == "TIME")
								{
									std::string result;
									getDateTime(std::string(), result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "LOCAL_TIME")
								{
									std::string result;
									getLocalDateTime(result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "VAR")
								{
									std::strcpy(output, ext_info.var.c_str());
									logger->info("Extension Command Line Variable: {0}", ext_info.var);
								}
								else if (tokens[1] == "SHUTDOWN")
								{
									std::strcpy(output, "[1]");
									logger->info("extDB2: Sending Shutdown to Armaserver");
									std::exit(EXIT_SUCCESS);
								}
								else
								{
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Error Invalid Format: {0}", input_str);
								}
								break;
							case 3:
								// DATABASE
								if (tokens[1] == "ADD_DATABASE")
								{
									connectDatabase(output, tokens[2], tokens[2]);
								}
								/*
								// BELOGSCANNER
								else if (tokens[1] == "START_BELOGSCANNER")
								{
									startBELogscanner(output, tokens[2]);
								}
								*/
								// RCON
								else if (tokens[1] == "START_RCON")
								{
									std::vector<std::string> extra_rcon_options;
									startRcon(output, tokens[2], extra_rcon_options);
								}
								else if (tokens[1] == "TIME")
								{
									std::string result;
									getDateTime(tokens[2], result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "UPTIME")
								{
									std::string result;
									getUPTime(tokens[2], result);
									std::strcpy(output, result.c_str());
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Error Invalid Format: {0}", input_str);
								}
								break;
							case 4:
								if (tokens[1] == "TIMEDIFF_CURRENT")
								{
									std::string result;
									getCurrentTimeDiff(tokens[2],tokens[3],result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "TIMEDIFF_CURRENT_LOCAL")
								{
									std::string result;
									getCurrentTimeDiff(tokens[2],tokens[3],result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "DATEADD")
								{
									std::string result;
									getDateAdd(tokens[2],tokens[3],result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "ADD_DATABASE")
								{
									connectDatabase(output, tokens[2], tokens[3]);
								}
								else if (tokens[1] == "ADD_PROTOCOL")
								{
									addProtocol(output, "", tokens[2], tokens[3], "");
								}
								else if (tokens[1] == "START_RCON")
								{
									std::vector<std::string> extra_rcon_options;
									extra_rcon_options.push_back(tokens[3]);
									startRcon(output, tokens[2], extra_rcon_options);
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Error Invalid Format: {0}", input_str);
								}
								break;
						case 5:
								if (tokens[1] == "TIMEDIFF")
								{
									std::string result;
									getTimeDiff(tokens[2],tokens[3],tokens[4],result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "TIMEDIFF_CURRENT")
								{
									std::string result;
									getCurrentTimeDiff(tokens[2],tokens[3],tokens[4],result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "ADD_PROTOCOL")
								{
									addProtocol(output, "", tokens[2], tokens[3], tokens[4]); // ADD + Init Options
								}
								else if (tokens[1] == "ADD_DATABASE_PROTOCOL")
								{
									addProtocol(output, tokens[2], tokens[3], tokens[4], ""); // ADD Database Protocol + No Options
								}
								else if (tokens[1] == "START_RCON")
								{
									std::vector<std::string> extra_rcon_options;
									extra_rcon_options.push_back(tokens[3]);
									extra_rcon_options.push_back(tokens[4]);
									startRcon(output, tokens[2], extra_rcon_options);
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Error Invalid Format: {0}", input_str);
								}
								break;
							case 6:
								if (tokens[1] == "ADD_DATABASE_PROTOCOL")
								{
									addProtocol(output, tokens[2], tokens[3], tokens[4], tokens[5]); // ADD Database Protocol + Options
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Error Invalid Format: {0}", input_str);
								}
								break;
							default:
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Error Invalid Format: {0}", input_str);
								}
						}
					}
					break;
				}
				default:
				{
					std::strcpy(output, "[0,\"Error Invalid Message\"]");
					logger->error("extDB2: Error Invalid Message: {0}", input_str);
				}
			}
		}
		#ifdef DEBUG_LOGGING
			logger->info("extDB2: Output to Server: {0}", output);
		#endif
	}
	catch (spdlog::spdlog_ex& e)
	{
		std::strcpy(output, "[0,\"Error LOGGER\"]");
		std::cout << "SPDLOG ERROR: " <<  e.what() << std::endl;
	}
	catch (Poco::Exception& e)
	{
		std::strcpy(output, "[0,\"Error\"]");
		#ifdef DEBUG_TESTING
			console->critical("extDB2: Error: {0}", e.displayText());
			console->critical("extDB2: Error: Input String {0}", function);
		#endif
		logger->critical("extDB2: Error: {0}", e.displayText());
		logger->critical("extDB2: Error: Input String {0}", function);
	}
}


#if defined(TEST_APP) && defined(DEBUG_TESTING)

	int main(int nNumberofArgs, char* pszArgs[])
	{
		int result_size = 80;
		char result[81] = {0};
		std::string input_str;


		boost::program_options::options_description desc("Options");
		desc.add_options()
			("bepath", boost::program_options::value<std::string>(), "Battleye Path")
			("extDB2_VAR", boost::program_options::value<std::string>(), "extDB2 Variable")
			("extDB2_WORK", boost::program_options::value<std::string>(), "extDB2 Work Directory");

		boost::program_options::variables_map bpo_options;
		boost::program_options::store(boost::program_options::parse_command_line(nNumberofArgs, pszArgs, desc), bpo_options);

		std::unordered_map<std::string, std::string> options;

		if (bpo_options.count("bepath") > 0)
		{
			options["BEPATH"] = bpo_options["bepath"].as<std::string>();
		}
		if (bpo_options.count("extDB2_WORK") > 0)
		{
			options["WORK"] = bpo_options["extDB2_WORK"].as<std::string>();
		}
		if (bpo_options.count("extDB2_VAR") > 0)
		{
			options["VAR"] = bpo_options["extDB2_VAR"].as<std::string>();
		}

		Ext *extension;
		extension = new Ext(std::string(""), options);

		bool test = false;
		int test_counter = 0;
		for (;;)
		{
			result[0] = '\0';
			std::getline(std::cin, input_str);
			if (boost::algorithm::iequals(input_str, "Quit") == 1)
			{
				break;
			}
			else if (boost::algorithm::iequals(input_str, "Test") == 1)
			{
				test = true;
			}
			else
			{
				extension->callExtension(result, result_size, input_str.c_str());
				extension->console->info("extDB2: {0}", result);
			}
			while (test)
			{
				if (test_counter >= 10000)
				{
					test_counter = 0;
					test = false;
					break;
				}
				++test_counter;
				extension->callExtension(result, result_size, std::string("1:SQL:TEST1:testing").c_str());
				extension->callExtension(result, result_size, std::string("1:SQL:TEST2:testing").c_str());
				extension->callExtension(result, result_size, std::string("1:SQL:TEST3:testing").c_str());
				extension->callExtension(result, result_size, std::string("1:SQL:TEST4:testing").c_str());
				extension->callExtension(result, result_size, std::string("1:SQL:TEST5:testing").c_str());
				extension->console->info("extDB2: {0}", result);
			}
		}
		extension->stop();
		return 0;
	}
#endif
