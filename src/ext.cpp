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
*/


#include "ext.h"

#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <regex>
#include <thread>

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
#include "backends/http.h"
#include "backends/rcon.h"
#include "backends/remoteserver.h"
#include "backends/steam.h"

#include "protocols/abstract_protocol.h"
#include "protocols/http_raw.h"
#include "protocols/sql_custom.h"
#include "protocols/sql_custom_v2.h"
#include "protocols/sql_raw.h"
#include "protocols/sql_raw_v2.h"
#include "protocols/log.h"
#include "protocols/misc.h"
#include "protocols/rcon.h"
#include "protocols/steam.h"
#include "protocols/steam_v2.h"


Ext::Ext(std::string dll_path, std::unordered_map<std::string, std::string> options, bool status)
{
	try
	{
		bool conf_found = false;
		#ifdef _WIN32
			bool conf_randomized = false;
		#endif
		
		boost::filesystem::path extDB_config_path;

		if (options.count("WORK") > 0)
		{
			// Override extDB2 Location
			extDB_config_path = options["WORK"];
			extDB_config_path /= "extdb-conf.ini";
			if (boost::filesystem::exists(extDB_config_path))
			{
				conf_found = true;
				extDB_info.path = extDB_config_path.parent_path().string();
			}
			else
			{
				// Override extDB2 Location -- Randomized Search
				#ifdef _WIN32
					extDB_config_path = extDB_config_path.parent_path();
					search(extDB_config_path, conf_found, conf_randomized);
				#endif
			}
		}
		else
		{
			// extDB2 DLL Location   This fails on Windows why ????
			extDB_config_path = dll_path;
			extDB_config_path = extDB_config_path.parent_path();
			extDB_config_path /= "extdb-conf.ini";
			if (boost::filesystem::is_regular_file(extDB_config_path))
			{
				conf_found = true;
				extDB_info.path = extDB_config_path.parent_path().string();
			}
			// extDB2 Arma3 Location
			else if (boost::filesystem::is_regular_file("extdb-conf.ini"))
			{
				conf_found = true;
				extDB_config_path = boost::filesystem::path("extdb-conf.ini");
				extDB_info.path = extDB_config_path.parent_path().string();
			}
			else
			{
				#ifdef _WIN32	// Windows Only, Linux Arma2 Doesn't have extension Support
					// Search for Randomize Config File -- Legacy Security Support For Arma2Servers		

					extDB_config_path = extDB_config_path.parent_path();
					// CHECK DLL PATH FOR CONFIG)
					if (!extDB_config_path.string().empty())
					{
						search(extDB_config_path, conf_found, conf_randomized);
					}

					// CHECK ARMA ROOT DIRECTORY FOR CONFIG
					if (!conf_found)
					{
						extDB_config_path = boost::filesystem::current_path().string();
						search(extDB_config_path, conf_found, conf_randomized);
					}
				#endif
			}
		}

		if (conf_found)
		{
			pConf = new Poco::Util::IniFileConfiguration(extDB_config_path.make_preferred().string());
			extDB_info.logger_flush = pConf->getBool("Log.Flush", true);

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

					boost::filesystem::path randomize_configfile_path = extDB_config_path.parent_path() /= randomized_filename;
					boost::filesystem::rename(extDB_config_path, randomize_configfile_path);
				}
			#endif
		}

		// Initialize Loggers
		//		Console Logger
		#ifdef DEBUG_TESTING
			auto console_temp = spdlog::stdout_logger_mt("extDB Console logger");
			console.swap(console_temp);
		#endif

		//		File Logger
		Poco::DateTime current_dateTime;

		boost::filesystem::path log_relative_path;
		log_relative_path = boost::filesystem::path(extDB_info.path);
		log_relative_path /= "extDB";
		log_relative_path /= "logs";
		log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%Y");
		log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%n");
		log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%d");

		extDB_info.log_path = log_relative_path.make_preferred().string();
		boost::filesystem::create_directories(log_relative_path);

		log_relative_path /= Poco::DateTimeFormatter::format(current_dateTime, "%H-%M-%S");

		auto logger_temp = spdlog::rotating_logger_mt("extDB2 File Logger", log_relative_path.make_preferred().string(), 1048576 * 100, 3, extDB_info.logger_flush);
		logger.swap(logger_temp);

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
			console->info("Welcome to extDB Test Application");
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
			logger->info("Message: If you would like to Donate to extDB2 Develeopment");
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
			extDB_info.max_threads = pConf->getInt("Main.Threads", 0);
			int detected_cpu_cores = boost::thread::hardware_concurrency();
			if (extDB_info.max_threads <= 0)
			{
				// Auto-Detect
				if (detected_cpu_cores > 6)
				{
					#ifdef DEBUG_TESTING
						console->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 6);
					#endif
					logger->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 6);
					extDB_info.max_threads = 6;
				}
				else if (detected_cpu_cores <= 2)
				{
					#ifdef DEBUG_TESTING
						console->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 2);
					#endif
					logger->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 2);
					extDB_info.max_threads = 2;
				}
				else
				{
					extDB_info.max_threads = detected_cpu_cores;
					#ifdef DEBUG_TESTING
						console->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, extDB_info.max_threads);
					#endif
					logger->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, extDB_info.max_threads);
				}
			}
			else if (extDB_info.max_threads > 8)  // Sanity Check
			{
				// Manual Config
				#ifdef DEBUG_TESTING
					console->info("extDB2: Sanity Check, Setting up {0} Worker Threads (config settings {1})", 8, extDB_info.max_threads);
				#endif
				logger->info("extDB2: Sanity Check, Setting up {0} Worker Threads (config settings {1})", 8, extDB_info.max_threads);
				extDB_info.max_threads = 8;
			}
			else
			{
				// Manual Config
				#ifdef DEBUG_TESTING
					console->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads (config settings)", detected_cpu_cores, extDB_info.max_threads);
				#endif
				logger->info("extDB2: Detected {0} Cores, Setting up {1} Worker Threads (config settings)", detected_cpu_cores, extDB_info.max_threads);
			}

			// Save -extDB_VAR value for retreiving later
			extDB_info.var = "\"" + options["VAR"] + "\"";

			// Setup ASIO Worker Pool
			io_work_ptr.reset(new boost::asio::io_service::work(io_service));
			for (int i = 0; i < extDB_info.max_threads; ++i)
			{
				threads.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));
			}

 			// Initialize so have atomic setup correctly
 			rcon.reset(new Rcon(io_service, logger));
 			rcon->extInit(this);
			remote_server.init(this);

			// Initialize so have atomic setup correctly + Setup VAC Ban Logger
			steam.init(this, extDB_info.path, current_dateTime);
		}

		logger->info();
		logger->info();
		spdlog::set_pattern("[%H:%M:%S %z] [Thread %t] %v");
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
	io_work_ptr.reset();
	logger->info("extDB2: IO Worker Killed");
	if (extDB_connectors_info.rcon)
	{
		rcon->disconnect();
		logger->info("extDB2: Rcon Stopped");
	}
	threads.join_all();
	logger->info("extDB2: Threads Stopped");
	io_service.stop();
	logger->info("extDB2: IO Service Stopped");
	if (extDB_connectors_info.mysql)
	{
		//Poco::Data::MySQL::Connector::unregisterConnector();
	}
	if (extDB_connectors_info.sqlite)
	{
		//Poco::Data::SQLite::Connector::unregisterConnector();
	}
}

#ifdef _WIN32
	void Ext::search(boost::filesystem::path &extDB_config_path, bool &conf_found, bool &conf_randomized)
	{
		std::regex expression("extdb-conf.*ini");
		for (boost::filesystem::directory_iterator it(extDB_config_path); it != boost::filesystem::directory_iterator(); ++it)
		{
			if (boost::filesystem::is_regular_file(it->path()))
			{
				if(std::regex_search(it->path().string(), expression))
				{
					conf_found = true;
					conf_randomized = true;
					extDB_config_path = boost::filesystem::path(it->path().string());
					extDB_info.path = extDB_config_path.parent_path().string();
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


void Ext::connectRemote(char *output, const int &output_size, const std::string &remote_conf)
// Start RCon
{
	if (pConf->getBool(remote_conf + ".Enable", false))
	{
		if (!extDB_connectors_info.remote)
		{
			remote_server.setup(remote_conf);
			extDB_connectors_info.remote = true;
			std::strcpy(output, ("[1]"));
		}
		else
		{
			std::strcpy(output, ("[0,\"RemoteAccess Already Started\"]"));
		}
	}
	else
	{
		std::strcpy(output, ("[0,\"RemoteAccess Disabled in Config\"]"));
	}
}


void Ext::connectRcon(char *output, const int &output_size, const std::string &rcon_conf, std::string player_info_returned)
// Start RCon
{
	if (boost::iequals(player_info_returned, "FULL") == 1)
	{
		player_info_returned = "FULL";
	}
	else
	{
		player_info_returned = "PARTIAL";
	}
	if (extDB_connectors_info.rcon)
	{
		std::strcpy(output, ("[0,\"Rcon is Already Running\"]"));
	}
	else
	{
		if (pConf->hasOption(rcon_conf + ".Port"))
		{
			std::vector<std::string> bad_playername_strings; 
			bad_playername_strings.push_back(":");

			std::string bad_playername_kick_message;

			std::vector<std::string> regrex_rules;

			bool enable_check_playername = pConf->getBool((rcon_conf + ".BadPlayerNameChecks"), false);
			if (enable_check_playername)
			{
				bad_playername_kick_message = pConf->getString(((rcon_conf) + ".BadPlayerNameKickMessage"), "");

				std::string temp_str;
				temp_str = pConf->getString(((rcon_conf) + ".BadPlayerStrings"), "");
				Poco::StringTokenizer tokens(temp_str, ":", Poco::StringTokenizer::TOK_TRIM);
				for (auto &token : tokens)
				{
					bad_playername_strings.push_back(token);
				}

				std::string regex_rule_num_str;
				int regrex_rule_num = 0;
				while (true)
				{
					++regrex_rule_num;
					regex_rule_num_str = Poco::NumberFormatter::format(regrex_rule_num);
					if (!(pConf->has(rcon_conf + ".BadPlayerStringsRegrex_" + regex_rule_num_str)))
					{
						break;
					}
					else
					{
						regrex_rules.push_back(pConf->getString(rcon_conf + ".BadPlayerStringsRegrex_" + regex_rule_num_str));
					}
				}
			}

			rcon->start(pConf->getString((rcon_conf + ".IP"), "127.0.0.1"), pConf->getInt((rcon_conf + ".Port"), 2302), 
						pConf->getString((rcon_conf + ".Password"), "password"), 
						player_info_returned,
						bad_playername_strings, regrex_rules, bad_playername_kick_message, enable_check_playername);

			std::strcpy(output, "[1]");
			extDB_connectors_info.rcon = true;
		}
		else
		{
			std::strcpy(output, ("[0,\"No Config Option Found\"]"));
		}
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


void Ext::connectDatabase(char *output, const int &output_size, const std::string &database_conf, const std::string &database_id)
// Connection to Database, database_id used when connecting to multiple different database.
{
	DBConnectionInfo *database = &extDB_connectors_info.databases[database_id];

	bool connected = true;

	// Check if already connectted to Database.
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


		if ((boost::iequals(database->type, std::string("MySQL")) == 1) || (boost::iequals(database->type, "SQLite") == 1))
		{
			try
			{
				// Database
				std::string connection_str;
				if (boost::iequals(database->type, std::string("MySQL")) == 1)
				{
					database->type = "MySQL";
					if (!(extDB_connectors_info.mysql))
					{
						Poco::Data::MySQL::Connector::registerConnector();
						extDB_connectors_info.mysql = true;
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
				else if (boost::iequals(database->type, "SQLite") == 1)
				{
					database->type = "SQLite";
					if (!(extDB_connectors_info.sqlite))
					{
						Poco::Data::SQLite::Connector::registerConnector();
						extDB_connectors_info.sqlite = true;
					}

					boost::filesystem::path sqlite_path(extDB_info.path);
					sqlite_path /= "extDB";
					sqlite_path /= "sqlite";
					sqlite_path /= pConf->getString(database_conf + ".Name");
					connection_str = sqlite_path.make_preferred().string();
				}
				database->sql_pool.reset(new Poco::Data::SessionPool(database->type,
																 	connection_str,
																 	pConf->getInt(database_conf + ".minSessions", 1),
																 	pConf->getInt(database_conf + ".maxSessions", extDB_info.max_threads),
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
		if (database_id.empty())
		{
			// Default Database
			database->type.clear();
			database->sql_pool.reset();
		}
		else
		{
			// Extra Database
			extDB_connectors_info.databases.erase(database_id);
		}
	}
}


void Ext::addProtocol(char *output, const int &output_size, const std::string &database_id, const std::string &protocol, const std::string &protocol_name, const std::string &init_data)
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
			if (boost::iequals(protocol, std::string("LOG")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new LOG());
			}
			else if (boost::iequals(protocol, std::string("MISC")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new MISC());
			}
			else if (boost::iequals(protocol, std::string("RCON")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new RCON());
			}
			else if (boost::iequals(protocol, std::string("STEAM")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new STEAM());
			}
			else if (boost::iequals(protocol, std::string("STEAM_V2")) == 1)
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
			if (boost::iequals(protocol, std::string("HTTP_RAW")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new HTTP_RAW());
			}
			else if (boost::iequals(protocol, std::string("SQL_CUSTOM")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new SQL_CUSTOM());
			}
			else if (boost::iequals(protocol, std::string("SQL_CUSTOM_V2")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new SQL_CUSTOM_V2());
			}
			else if (boost::iequals(protocol, std::string("SQL_RAW")) == 1)
			{
				unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new SQL_RAW());
			}
			else if (boost::iequals(protocol, std::string("SQL_RAW_V2")) == 1)
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
// Stores Result String  in a unordered map array.
//   Used when string > arma output char
{
	std::lock_guard<std::mutex> lock(mutex_results);
	stored_results[unique_id] = std::move(result_data);
	stored_results[unique_id].wait = false;
}


void Ext::getTCPRemote_mutexlock(char *output, const int &output_size)
{
	std::string result;
	{
		std::lock_guard<std::mutex> lock(remote_server.inputs_mutex);
		if (!remote_server.inputs.empty())
		{
			result = remote_server.inputs[0];
			remote_server.inputs.erase(remote_server.inputs.begin());
			if (remote_server.inputs.empty())
			{
				*(remote_server.inputs_flag) = false;
			}
		}
	}

	if (result.length() <= output_size)
	{
		std::strcpy(output, result.c_str());
	}
	else
	{
		resultData result_data;
		result_data.message = std::move(result);
		const unsigned int unique_id = saveResult_mutexlock(result_data);
		std::strcpy(output, Poco::NumberFormatter::format(unique_id).c_str());
	}
}


void Ext::sendTCPRemote_mutexlock(std::string &input_str)
{
	const std::string::size_type found = input_str.find(":", 2);

	if ((found==std::string::npos) || (found == (input_str.size() - 1)))
	{
		logger->error("extDB2: Invalid Format: sendTCPRemote: {0}", input_str);
	}
	else
	{
		int unique_client_id;
		if (Poco::NumberParser::tryParse(input_str.substr(2,(found-2)), unique_client_id))
		{
			std::lock_guard<std::mutex> lock(remote_server.clients_data_mutex);
			if (remote_server.clients_data.count(unique_client_id) != 0)
			{
				remote_server.clients_data[unique_client_id].outputs.push_back(input_str.substr(found+1));
			}
		}
		else
		{
			logger->error("extDB2: Invalid Format: sendTCPRemote: {0}", input_str);
		}
	}
}


void Ext::syncCallProtocol(char *output, const int &output_size, std::string &input_str, std::string::size_type &input_str_length)
// Sync callPlugin
{
	const std::string::size_type found = input_str.find(":",2);

	if ((found==std::string::npos) || (found == (input_str_length - 1)))
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


void Ext::onewayCallProtocol(const int &output_size, std::string &input_str)
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
			result_data.message.reserve(output_size);
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
			logger->info("extDB2: Extension Input from Server: {0}", std::string(function));
		#endif

		std::string input_str(function);
		std::string::size_type input_str_length = input_str.length();

		if (input_str_length <= 2)
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
					io_service.post(boost::bind(&Ext::onewayCallProtocol, this, output_size, std::move(input_str)));
					std::strcpy(output, "[1]");
					break;
				}
				case '2': //ASYNC + SAVE
				{
					// Protocol
					const std::string::size_type found = input_str.find(":",2);

					if ((found==std::string::npos) || (found == (input_str_length - 1)))
					{
						std::strcpy(output, "[0,\"Error Invalid Format\"]");
						logger->error("extDB2: Invalid Format: {0}", input_str);
					}
					else
					{
						// Check for Protocol Name Exists...
						// Do this so if someone manages to get server, the error message wont get stored in the result unordered map
						const std::string protocol = input_str.substr(2,(found-2));
						if (unordered_map_protocol.find(protocol) != unordered_map_protocol.end())
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
							logger->error("extDB2: Unknown Protocol: {0}", protocol);
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
				case '6': // GET -- TCPRemoteCode
				{
					if (*(remote_server.inputs_flag))
					{
						getTCPRemote_mutexlock(output, output_size);
					}
					break;
				}
				case '7': // SEND -- TCPRemoteCode
				{
					io_service.post(boost::bind(&Ext::sendTCPRemote_mutexlock, this, std::move(input_str)));
					break;
				}
				case '0': //SYNC
				{
					syncCallProtocol(output, output_size, input_str, input_str_length);
					break;
				}
				case '9': // SYSTEM CALLS / SETUP
				{
					Poco::StringTokenizer tokens(input_str, ":");
					if (extDB_info.extDB_lock)
					{
						if (tokens.count() == 2)
						{
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
							else
							{
								// Invalid Format
								std::strcpy(output, "[0,\"Error Invalid Format\"]");
								logger->error("extDB2: Invalid Format: {0}", input_str);
							}
						}
						else
						{
							// Invalid Format
							std::strcpy(output, "[0,\"Error Invalid Format\"]");
							logger->error("extDB2: Invalid Format: {0}", input_str);
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
									if (!extDB_connectors_info.steam)
									{
										extDB_connectors_info.steam = true;
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
									extDB_info.extDB_lock = true;
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
									logger->info("Extension Output Size: {0}", outputsize_str);
									std::strcpy(output, outputsize_str.c_str());
								}
								else if (tokens[1] == "VAR")
								{
									logger->info("Extension Output Size: {0}", extDB_info.var);
									std::strcpy(output, extDB_info.var.c_str());
								}
								else
								{
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Invalid Format: {0}", input_str);
								}
								break;
							case 3:
								// RCON
								if (tokens[1] == "START_RCON")
								{
									connectRcon(output, output_size, tokens[2], std::string("PARTIAL"));
								}
								else if (tokens[1] == "START_REMOTE")
								{
									connectRemote(output, output_size, tokens[2]);
								}
								else if (tokens[1] == "ADD_DATABASE")
								{
									connectDatabase(output, output_size, tokens[2], tokens[2]);
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Invalid Format: {0}", input_str);
								}
								break;
							case 4:
								// DATABASE
								if (tokens[1] == "ADD_DATABASE")
								{
									connectDatabase(output, output_size, tokens[2], tokens[3]);
								}
								else if (tokens[1] == "ADD_PROTOCOL")
								{
									addProtocol(output, output_size, "", tokens[2], tokens[3], ""); // ADD No Options
								}
								else if (tokens[1] == "START_RCON")
								{
									connectRcon(output, output_size, tokens[2], tokens[3]);
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Invalid Format: {0}", input_str);
								}
								break;
							case 5:
								//ADD PROTOCOL
								if (tokens[1] == "ADD_PROTOCOL")
								{
									addProtocol(output, output_size, "", tokens[2], tokens[3], tokens[4]); // ADD + Init Options
								}
								else if (tokens[1] == "ADD_DATABASE_PROTOCOL")
								{
									addProtocol(output, output_size, tokens[2], tokens[3], tokens[4], ""); // ADD Database Protocol + No Options
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Invalid Format: {0}", input_str);
								}
								break;
							case 6:
								if (tokens[1] == "ADD_DATABASE_PROTOCOL")
								{
									addProtocol(output, output_size, tokens[2], tokens[3], tokens[4], tokens[5]); // ADD Database Protocol + Options
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Invalid Format: {0}", input_str);
								}
								break;
							default:
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB2: Invalid Format: {0}", input_str);
								}
						}
					}
					break;
				}
				default:
				{
					std::strcpy(output, "[0,\"Error Invalid Message\"]");
					logger->error("extDB2: Invalid Message: {0}", input_str);
				}
			}
		}
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
			("extDB2_VAR", boost::program_options::value<std::string>(), "extDB2 Variable")
			("extDB2_WORK", boost::program_options::value<std::string>(), "extDB2 Work Directory");

		boost::program_options::variables_map bpo_options;
		boost::program_options::store(boost::program_options::parse_command_line(nNumberofArgs, pszArgs, desc), bpo_options);

		std::unordered_map<std::string, std::string> options;

		if (bpo_options.count("extDB2_WORK") > 0)
		{
			options["WORK"] = bpo_options["extDB2_WORK"].as<std::string>();
		}
		if (bpo_options.count("extDB2_VAR") > 0)
		{
			options["VAR"] = bpo_options["extDB2_VAR"].as<std::string>();
		}
		
		Ext *extension;
		extension = new Ext(std::string(""), options, true);

		bool test = false;
		int test_counter = 0;
		for (;;)
		{
			result[0] = '\0';
			std::getline(std::cin, input_str);
			if (boost::iequals(input_str, "Quit") == 1)
			{
			    break;
			}
			else if (boost::iequals(input_str, "Test") == 1)
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
