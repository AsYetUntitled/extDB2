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

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/regex.hpp>

#include <Poco/Data/Session.h>
#include <Poco/Data/SessionPool.h>
#include <Poco/Data/MySQL/Connector.h>
#include <Poco/Data/MySQL/MySQLException.h>
#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Data/SQLite/SQLiteException.h>

#include <Poco/AutoPtr.h>
#include <Poco/DateTime.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Exception.h>
#include <Poco/NumberFormatter.h>
#include <Poco/NumberParser.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Util/IniFileConfiguration.h>

#include <cstring>
#include <iostream>
#include <cstdlib>

#include "rconworker.h"
#include "steamworker.h"
#include "uniqueid.h"

#include "protocols/abstract_protocol.h"

#include "protocols/sql_custom.h"
#include "protocols/sql_raw.h"

#include "protocols/log.h"
#include "protocols/misc.h"
#include "protocols/rcon.h"
#include "protocols/steam.h"



Ext::Ext(std::string dll_path)
{
	try
	{
		mgr.reset (new IdManager);

		bool conf_found = false;
		bool conf_randomized = false;

		boost::filesystem::path extDB_config_path(dll_path);

		extDB_config_path = extDB_config_path.parent_path();
	  	extDB_config_path /= "extdb-conf.ini";

		std::string extDB_config_str = extDB_config_path.make_preferred().string();

		if (boost::filesystem::exists(extDB_config_str))
		{
			conf_found = true;
			extDB_info.path = extDB_config_path.parent_path().string();
		}
		else if (boost::filesystem::exists("extdb-conf.ini"))
		{
			conf_found = true;
			extDB_config_path = boost::filesystem::path("extdb-conf.ini");
			extDB_info.path = boost::filesystem::current_path().string();
		}
		else
		{
			#ifdef _WIN32	// WINDOWS ONLY, Linux Arma2 Doesn't have extension Support
				// Search for Randomize Config File -- Legacy Security Support For Arma2Servers
				extDB_config_path = extDB_config_path.parent_path();
				extDB_config_str = extDB_config_path.make_preferred().string();

				boost::regex expression("extdb-conf.*ini");

				if (!extDB_config_str.empty())
				{
					// CHECK DLL PATH FOR CONFIG
					for (boost::filesystem::directory_iterator it(extDB_config_str); it != boost::filesystem::directory_iterator(); ++it)
					{
						if (is_regular_file(it->path()))
						{
							if(boost::regex_search(it->path().string(), expression))
							{
								conf_found = true;
								conf_randomized = true;
								extDB_config_path = boost::filesystem::path(it->path().string());
								extDB_info.path = boost::filesystem::path (extDB_config_str).string();
								break;
							}
						}
					}
				}

				// CHECK ARMA ROOT DIRECTORY FOR CONFIG
				if (!conf_found)
				{
					for(boost::filesystem::directory_iterator it(boost::filesystem::current_path()); it !=  boost::filesystem::directory_iterator(); ++it)
					{
						if (is_regular_file(it->path()))
						{
							if(boost::regex_search(it->path().string(), expression))
							{
								conf_found = true;
								conf_randomized = true;
								extDB_config_path = boost::filesystem::path(it->path().string());
								extDB_info.path = boost::filesystem::current_path().string();
								break;
							}
						}
					}
				}
			#endif
		}

		// Initialize Loggers
		Poco::DateTime now;
		std::string log_filename = Poco::DateTimeFormatter::format(now, "%H-%M-%S.log");

		boost::filesystem::path log_relative_path;
		log_relative_path = boost::filesystem::path(extDB_info.path);
		log_relative_path /= "extDB";
		log_relative_path /= "logs";
		log_relative_path /= Poco::DateTimeFormatter::format(now, "%Y");
		log_relative_path /= Poco::DateTimeFormatter::format(now, "%n");
		log_relative_path /= Poco::DateTimeFormatter::format(now, "%d");
		extDB_info.log_path = log_relative_path.make_preferred().string();
		boost::filesystem::create_directories(log_relative_path);
		log_relative_path /= log_filename;

		boost::filesystem::path vacBans_log_relative_path;
		vacBans_log_relative_path = boost::filesystem::path(extDB_info.path);
		vacBans_log_relative_path /= "extDB";
		vacBans_log_relative_path /= "vacban_logs";
		vacBans_log_relative_path /= Poco::DateTimeFormatter::format(now, "%Y");
		vacBans_log_relative_path /= Poco::DateTimeFormatter::format(now, "%n");
		vacBans_log_relative_path /= Poco::DateTimeFormatter::format(now, "%d");
		boost::filesystem::create_directories(vacBans_log_relative_path);
		vacBans_log_relative_path /= log_filename;

		auto console_temp = spdlog::stdout_logger_mt("extDB Console logger");
		auto logger_temp = spdlog::daily_logger_mt("extDB File Logger", log_relative_path.make_preferred().string(), true);
		auto vacBans_logger_temp = spdlog::daily_logger_mt("extDB vacBans Logger", vacBans_log_relative_path.make_preferred().string(), true);

		console.swap(console_temp);
		logger.swap(logger_temp);
		vacBans_logger.swap(vacBans_logger_temp);

		spdlog::set_level(spdlog::level::info);
		spdlog::set_pattern("[%H:%M:%S %z] [Thread %t] %v");


		logger->info("extDB: Version: {0}", getVersion());
		#ifdef __GNUC__
			#ifndef DEBUG_TESTING
				logger->info("extDB: Linux Version");
			#else
				logger->info("extDB: Linux Debug Version");
			#endif
		#endif

		#ifdef _MSC_VER
			#ifndef DEBUG_TESTING
				logger->info("extDB: Windows Version");
			#else
				logger->info("extDB: Windows Debug Version");
				logger->info();
			#endif
		#endif

		#ifndef TESTING
			logger->info("Message: Arma Linux Servers are using Older Physic Library (than Windows Servers), due to Debian 7 using old version of Glibc");
			logger->info("Message: If you like extDB consider donating or bug BIS to drop support for Debian 7 thanks, so Linux Servers get same Physic Library Version as Windows");
			logger->info("Message: Note currently most/all development for extDB is done on a Linux Server");
			logger->info("Message: Torndeco: 24/01/15");
			logger->info();
		#endif


		if (!conf_found)
		{
			console->critical("extDB: Unable to find extdb-conf.ini");
			logger->critical("extDB: Unable to find extdb-conf.ini");
			// Kill Server no config file found -- Evil
			std::exit(EXIT_SUCCESS);
		}
		else
		{
			// Load extdb config
			pConf = (new Poco::Util::IniFileConfiguration(extDB_config_path.make_preferred().string()));
			#ifdef TESTING
				console->info("extDB: Found extdb-conf.ini");
			#endif
			logger->info("extDB: Found extdb-conf.ini");

			// Start Threads + ASIO
			extDB_info.max_threads = pConf->getInt("Main.Threads", 0);
			int detected_cpu_cores = boost::thread::hardware_concurrency();
			if (extDB_info.max_threads <= 0)
			{
				// Auto-Detect
				if (detected_cpu_cores > 6)
				{
					#ifdef TESTING
						console->info("extDB: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 6);
					#endif
					logger->info("extDB: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 6);
					extDB_info.max_threads = 6;
				}
				else if (detected_cpu_cores <= 2)
				{
					#ifdef TESTING
						console->info("extDB: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 2);
					#endif
					logger->info("extDB: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 2);
					extDB_info.max_threads = 2;
				}
				else
				{
					extDB_info.max_threads = detected_cpu_cores;
					#ifdef TESTING
						console->info("extDB: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, extDB_info.max_threads);
					#endif
					logger->info("extDB: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, extDB_info.max_threads);
				}
			}
			else if (extDB_info.max_threads > 8)  // Sanity Check
			{
				// Manual Config
				#ifdef TESTING
					console->info("extDB: Sanity Check, Setting up {0} Worker Threads (config settings {1})", 8, extDB_info.max_threads);
				#endif
				logger->info("extDB: Sanity Check, Setting up {0} Worker Threads (config settings {1})", 8, extDB_info.max_threads);
				extDB_info.max_threads = 8;
			}
			else
			{
				// Manual Config
				#ifdef TESTING
					console->info("extDB: Detected {0} Cores, Setting up {1} Worker Threads (config settings)", detected_cpu_cores, extDB_info.max_threads);
				#endif
				logger->info("extDB: Detected {0} Cores, Setting up {1} Worker Threads (config settings)", detected_cpu_cores, extDB_info.max_threads);
			}

			// Setup ASIO Worker Pool
			io_work_ptr.reset(new boost::asio::io_service::work(io_service));
			for (int i = 0; i < extDB_info.max_threads; ++i)
			{
				threads.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));
			}

			if (boost::iequals(pConf->getString("Log.Mode", "sync"), "async") == 1)
			{
				std::size_t q_size = 1048576; //queue size must be power of 2
				spdlog::set_async_mode(q_size);
			}

 			// Initialize so have atomic setup correctly
			rcon_worker.init(logger);

			// Initialize so have atomic setup correctly
			steam_worker.init(this);

			#ifdef _WIN32
				if ((pConf->getBool("Main.Randomize Config File", false)) && (!conf_randomized))
				// Only Gonna Randomize Once, Keeps things Simple
				{
					std::string chars("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
									  "1234567890");
					// Skipping Lowercase, this function only for arma2 + extensions only available on windows.
					boost::random::random_device rng;
					boost::random::uniform_int_distribution<> index_dist(0, chars.size() - 1);

					std::string randomized_filename = "extdb-conf-";
					for(int i = 0; i < 8; ++i)
					{
						randomized_filename += chars[index_dist(rng)];
					}
					randomized_filename += ".ini";

					boost::filesystem::path randomize_configfile_path = extDB_config_path.parent_path() /= randomized_filename;
					boost::filesystem::rename(extDB_config_path, randomize_configfile_path);
				}
			#endif
		}
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
	console->info("extDB: Stopping ...");
	logger->info("extDB: Stopping ...");
	io_work_ptr.reset();
	//io_service.stop();
	threads.join_all();
	if (extDB_connectors_info.mysql)
	{
		//Poco::Data::MySQL::Connector::unregisterConnector();
	}
	if (extDB_connectors_info.sqlite)
	{
		Poco::Data::SQLite::Connector::unregisterConnector();
	}
	if (extDB_connectors_info.rcon)
	{
		rcon_worker.disconnect();
		rcon_worker_thread.join();
	}
}



std::string Ext::getVersion() const
{
	return "36";
}


std::string Ext::getExtensionPath()
{
	return extDB_info.path;
}


std::string Ext::getLogPath()
{
	return extDB_info.log_path;
}


Poco::Data::Session Ext::getDBSession_mutexlock(AbstractExt::DBConnectionInfo &database)
// Gets available DB Session (mutex lock)
{
	boost::lock_guard<boost::mutex> lock(database.mutex_pool);
	return database.pool->get();
}


Poco::Data::Session Ext::getDBSession_mutexlock(AbstractExt::DBConnectionInfo &database, Poco::Data::SessionPool::SessionDataPtr &session_data_ptr)
// Gets available DB Session (mutex lock) + Cached Statemetns
{
	boost::lock_guard<boost::mutex> lock(database.mutex_pool);
	return database.pool->get(session_data_ptr);
}


void Ext::steamQuery(const int &unique_id, bool queryFriends, bool queryVacBans, std::vector<std::string> &steamIDs, bool wakeup)
// Adds Query to Steam Protocol, wakeup option is to wakeup steam thread. Note: Steam thread periodically checks every minute anyway.
{
	steam_worker.addQuery(unique_id, queryFriends, queryVacBans, steamIDs);
	if (wakeup)
	{
		steam_worker_thread.wakeUp();
	}
}


void Ext::connectRCon(char *output, const int &output_size, const std::string &rcon_conf)
// Start RCon
{
	if (pConf->hasOption(rcon_conf + ".Port"))
	{
		if (!extDB_connectors_info.rcon)
		{
			rcon_worker.updateLogin(pConf->getString((rcon_conf + ".Address"), "127.0.0.1"), pConf->getInt((rcon_conf + ".Port"), 2302), pConf->getString((rcon_conf + ".Password"), "password"));
			extDB_connectors_info.rcon = true;
			rcon_worker_thread.start(rcon_worker);
			std::strcpy(output, ("[1]"));
		}
		else
		{
			std::strcpy(output, ("[0,\"RCon Already Started\"]"));
		}
	}
	else
	{
		std::strcpy(output, ("[0,\"No Config Option Found\"]"));
	}
}


void Ext::rconCommand(std::string str)
// Adds RCon Command to be sent to Server.
{
	console->warn("extDB: running {0}, rconCommand {1}", extDB_connectors_info.rcon, str);
	if (extDB_connectors_info.rcon) // Check if Rcon enabled
	{
		rcon_worker.addCommand(str);
	}
}


void Ext::connectDatabase(char *output, const int &output_size, const std::string &database_conf, const std::string &database_id)
// Connection to Database, database_id used when connecting to multiple different database.
{
	DBConnectionInfo *database;
	database = &extDB_connectors_info.databases[database_id];

	bool failed = false;
	try
	{
		// Check if already connectted to Database.
		if (!database->type.empty())
		{
			#ifdef TESTING
				console->warn("extDB: Already Connected to Database");
			#endif
			logger->warn("extDB: Already Connected to a Database");
			std::strcpy(output, "[0,\"Already Connected to Database\"]");
		}
		else
		{
			if (pConf->hasOption(database_conf + ".Type"))
			{
				// Database
				database->type = pConf->getString(database_conf + ".Type");
				std::string db_name = pConf->getString(database_conf + ".Name");

				database->min_sessions = pConf->getInt(database_conf + ".minSessions", 1);
				if (database->min_sessions <= 0)
				{
					database->min_sessions = 1;
				}
				database->max_sessions = pConf->getInt(database_conf + ".maxSessions", 1);
				if (database->max_sessions <= 0)
				{
					database->max_sessions = extDB_info.max_threads;
				}

				database->idle_time = pConf->getInt(database_conf + ".idleTime", 600);

				#ifdef TESTING
					console->info("extDB: Database Type: {0}", database->type);
				#endif
				logger->info("extDB: Database Type: {0}", database->type);

				if (boost::iequals(database->type, std::string("MySQL")) == 1)
				{
					database->type = "MySQL";

					if (!(extDB_connectors_info.mysql))
					{
						Poco::Data::MySQL::Connector::registerConnector();
						extDB_connectors_info.mysql = true;
					}			

					database->connection_str += "host=" + pConf->getString(database_conf + ".IP") + ";";
					database->connection_str += "port=" + pConf->getString(database_conf + ".Port") + ";";
					database->connection_str += "user=" + pConf->getString(database_conf + ".Username") + ";";
					database->connection_str += "password=" + pConf->getString(database_conf + ".Password") + ";";
					database->connection_str += "db=" + db_name + ";";
					database->connection_str += "auto-reconnect=true";

					if (pConf->getBool(database_conf + ".Compress", false))
					{
						database->connection_str += ";compress=true";
					}

					if (pConf->getBool(database_conf + ".Secure Auth", false))
					{
						database->connection_str += ";secure-auth=true";	
					}

					database->pool.reset(new Poco::Data::SessionPool(database->type, 
																	 database->connection_str, 
																	 database->min_sessions, 
																	 database->max_sessions, 
																	 database->idle_time));
					if (database->pool->get().isConnected())
					{
						#ifdef TESTING
							console->info("extDB: Database Session Pool Started");
						#endif
						logger->info("extDB: Database Session Pool Started");
						std::strcpy(output, "[1]");
					}
					else
					{
						#ifdef TESTING
							console->warn("extDB: Database Session Pool Failed");
						#endif
						logger->warn("extDB: Database Session Pool Failed");
						std::strcpy(output, "[0,\"Database Session Pool Failed\"]");
						failed = true;
					}
				}
				else if (boost::iequals(database->type, "SQLite") == 1)
				{
					if (!(extDB_connectors_info.sqlite))
					{
						Poco::Data::SQLite::Connector::registerConnector();
						extDB_connectors_info.sqlite = true;
					}

					database->type = "SQLite";

					boost::filesystem::path sqlite_path(getExtensionPath());
					sqlite_path /= "extDB";
					sqlite_path /= "sqlite";
					sqlite_path /= db_name;
					database->connection_str = sqlite_path.make_preferred().string();

					database->pool.reset(new Poco::Data::SessionPool(database->type, 
																	 database->connection_str, 
																	 database->min_sessions, 
																	 database->max_sessions, 
																	 database->idle_time));
					if (database->pool->get().isConnected())
					{
						#ifdef TESTING
							console->info("extDB: Database Session Pool Started");
						#endif
						logger->info("extDB: Database Session Pool Started");
						std::strcpy(output, "[1]");
					}
					else
					{
						#ifdef TESTING
							console->warn("extDB: Database Session Pool Failed");
						#endif
						logger->warn("extDB: Database Session Pool Failed");
						std::strcpy(output, "[0,\"Database Session Pool Failed\"]");
						failed = true;
					}
				}
				else
				{
					#ifdef TESTING
						console->warn("extDB: No Database Engine Found for {0}", db_name);
					#endif
					logger->warn("extDB: No Database Engine Found for {0}", db_name);
					std::strcpy(output, "[0,\"Unknown Database Type\"]");
					failed = true;
				}
			}
			else
			{
				#ifdef TESTING
					console->warn("extDB: No Config Option Found: {0}", database_conf);
				#endif
				logger->warn("extDB: No Config Option Found: {0}", database_conf);
				std::strcpy(output, "[0,\"No Config Option Found\"]");
				failed = true;
			}
		}
	}
	catch (Poco::Data::NotConnectedException& e)
	{
		#ifdef TESTING
			console->error("extDB: Database NotConnectedException Error: {0}", e.displayText());
		#endif
		logger->error("extDB: Database NotConnectedException Error: {0}", e.displayText());
		std::strcpy(output, "[0,\"Database NotConnectedException Error\"]");
		failed = true;
	}
	catch (Poco::Data::MySQL::ConnectionException& e)
	{
		#ifdef TESTING
			console->error("extDB: Database ConnectionException Error: {0}", e.displayText());
		#endif
		logger->error("extDB: Database ConnectionException Error: {0}", e.displayText());
		std::strcpy(output, "[0,\"Database ConnectionException Error\"]");
		failed = true;
	}
	catch (Poco::Exception& e)
	{
		#ifdef TESTING
			console->error("extDB: Database Exception Error: {0}", e.displayText());
		#endif
		logger->error("extDB: Database Exception Error: {0}", e.displayText());
		std::strcpy(output, "[0,\"Database Exception Error\"]");
		failed = true;
	}

	if (failed)
	{
		if (database_id.empty())
		{
			// Default Database
			database->type.clear();
			database->connection_str.clear();
			database->pool.reset();
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
	boost::lock_guard<boost::mutex> lock(mutex_unordered_map_protocol);
	if (unordered_map_protocol.find(protocol_name) != unordered_map_protocol.end())
	{
		std::strcpy(output, "[0,\"Error Protocol Name Already Taken\"]");
		logger->warn("extDB: Error Protocol Name Already Taken: {0}", protocol_name);
	}
	else
	{
		bool status = true;
		if (boost::iequals(protocol, std::string("SQL_CUSTOM")) == 1)
		{
			unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new SQL_CUSTOM());
		}
		else if (boost::iequals(protocol, std::string("SQL_RAW")) == 1)
		{
			unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new SQL_RAW());
		}
		else if (boost::iequals(protocol, std::string("RCON")) == 1)
		{
			unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new RCON());
		}
		else if (boost::iequals(protocol, std::string("VAC")) == 1)
		{
			unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new STEAM());
		}
		else if (boost::iequals(protocol, std::string("MISC")) == 1)
		{
			unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new MISC());
		}
		else if (boost::iequals(protocol, std::string("LOG")) == 1)
		{
			unordered_map_protocol[protocol_name] = std::unique_ptr<AbstractProtocol> (new LOG());
		}
		else
		{
			status = false;
			std::strcpy(output, "[0,\"Error Unknown Protocol\"]");
			logger->warn("extDB: Failed to Load Unknown Protocol: {0}", protocol);
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
				logger->warn("extDB: Failed to Load Protocol: {0}", protocol);
			}
		}
	}
}


void Ext::getSinglePartResult_mutexlock(const int &unique_id, char *output, const int &output_size)
// Gets Result String from unordered map array -- Result Formt == Single-Message
//   If <=, then sends output to arma, and removes entry from unordered map array
//   If >, sends [5] to indicate MultiPartResult
{
	boost::lock_guard<boost::mutex> lock(mutex_results);

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
			mgr.get()->FreeId(unique_id);
		}
	}
}


void Ext::getMultiPartResult_mutexlock(const int &unique_id, char *output, const int &output_size)
// Gets Result String from unordered map array  -- Result Format = Multi-Message 
//   If length of String = 0, sends arma "", and removes entry from unordered map array
//   If <=, then sends output to arma
//   If >, then sends 1 part to arma + stores rest.
{
	boost::lock_guard<boost::mutex> lock(mutex_results);

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
		mgr.get()->FreeId(unique_id);
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


const int Ext::saveResult_mutexlock(const resultData &result_data)
// Stores Result String and returns Unique ID, used by SYNC Calls where message > outputsize
{
	boost::lock_guard<boost::mutex> lock(mutex_results);
	const int unique_id = mgr.get()->AllocateId();
	stored_results[unique_id] = std::move(result_data);
	stored_results[unique_id].wait = false;
	return unique_id;
}


void Ext::saveResult_mutexlock(const int &unique_id, const resultData &result_data)
// Stores Result String  in a unordered map array.
//   Used when string > arma output char
{
	boost::lock_guard<boost::mutex> lock(mutex_results);
	stored_results[unique_id] = std::move(result_data);
	stored_results[unique_id].wait = false;
}


void Ext::syncCallProtocol(char *output, const int &output_size, const std::string &protocol, const std::string &data)
// Sync callPlugin
{
	auto const_itr = unordered_map_protocol.find(protocol);
	if (const_itr == unordered_map_protocol.end())
	{
		std::strcpy(output, "[0,\"Error Unknown Protocol\"]");
	}
	else
	{
		// Checks if Result String will fit into arma output char
		//   If <=, then sends output to arma
		//   if >, then sends ID Message arma + stores rest. (mutex locks)
		resultData result_data;
		result_data.message.reserve(output_size);
		const_itr->second->callProtocol(data, result_data.message);
		if (result_data.message.length() <= (output_size-6))
		{
			std::strcpy(output, result_data.message.c_str());
		}
		else
		{
			const int unique_id = saveResult_mutexlock(result_data);
			std::strcpy(output, ("[2,\"" + Poco::NumberFormatter::format(unique_id) + "\"]").c_str());
		}
	}
}


void Ext::onewayCallProtocol(const int &output_size, const std::string protocol, const std::string data)
// ASync callProtocol
{
	auto const_itr = unordered_map_protocol.find(protocol);
	if (const_itr != unordered_map_protocol.end())
	{
		resultData result_data;
		result_data.message.reserve(output_size);
		const_itr->second->callProtocol(data, result_data.message, 0);
	}
}


void Ext::asyncCallProtocol(const int &output_size, const std::string protocol, const std::string data, const int unique_id)
// ASync + Save callProtocol
// We check if Protocol exists here, since its a thread (less time spent blocking arma) and it shouldnt happen anyways
{
	resultData result_data;
	result_data.message.reserve(output_size);
	if (unordered_map_protocol[protocol].get()->callProtocol(data, result_data.message, unique_id))
	{
		saveResult_mutexlock(unique_id, result_data);
	};
}


void Ext::callExtenion(char *output, const int &output_size, const char *function)
// Arma CallExtension
{
	try
	{
		#ifdef DEBUG_LOGGING
			logger->info("extDB: Extension Input from Server: {0}", std::string(function));
		#endif

		const std::string input_str(function);
		std::string::size_type input_str_length = input_str.length();

		if (input_str_length <= 2)
		{
			std::strcpy(output, "[0,\"Error Invalid Message\"]");
			logger->info("extDB: Invalid Message: {0}", input_str);
		}
		else
		{
			// Async / Sync
			const int async = Poco::NumberParser::parse(input_str.substr(0, 1));
			switch (async)
			{
				case 2: //ASYNC + SAVE
				{
					// Protocol
					const std::string::size_type found = input_str.find(":",2);

					if (found==std::string::npos)  // Check Invalid Format
					{
						std::strcpy(output, "[0,\"Error Invalid Format\"]");
						logger->error("extDB: Invalid Format: {0}", input_str);
					}
					else
					{
						const std::string protocol = input_str.substr(2,(found-2));
						bool found_procotol = false;
						int unique_id;

						if (found == (input_str_length - 1))
						{
							std::strcpy(output, "[0,\"Error Invalid Format\"]");
							logger->error("extDB: Invalid Format: {0}", input_str);
						}
						else
						{
							// Check for Protocol Name Exists...
							// Do this so if someone manages to get server, the error message wont get stored in the result unordered map
							if (unordered_map_protocol.find(protocol) != unordered_map_protocol.end())
							{
								boost::lock_guard<boost::mutex> lock(mutex_results);

								unique_id = mgr.get()->AllocateId();
								stored_results[unique_id].wait = true;
								found_procotol = true;
							}
							else
							{
								std::strcpy(output, "[0,\"Error Unknown Protocol\"]");
								logger->error("extDB: Unknown Protocol: {0}", protocol);
							}
						}
						// Only Add Job to Work Queue + Return ID if Protocol Name exists.
						if (found_procotol)
						{
							io_service.post(boost::bind(&Ext::asyncCallProtocol, this, output_size, protocol, input_str.substr(found+1), unique_id));
							std::strcpy(output, ("[2,\"" + Poco::NumberFormatter::format(unique_id) + "\"]").c_str());
						}
					}
					break;
				}
				case 4: // GET -- Single-Part Message Format
				{
					const int unique_id = Poco::NumberParser::parse(input_str.substr(2));
					getSinglePartResult_mutexlock(unique_id, output, output_size);
					break;
				}
				case 5: // GET -- Multi-Part Message Format
				{
					const int unique_id = Poco::NumberParser::parse(input_str.substr(2));
					getMultiPartResult_mutexlock(unique_id, output, output_size);
					break;
				}
				case 1: //ASYNC
				{
					// Protocol
					const std::string::size_type found = input_str.find(":",2);

					if (found==std::string::npos)  // Check Invalid Format
					{
						std::strcpy(output, "[0,\"Error Invalid Format\"]");
						logger->error("extDB: Invalid Format: {0}", input_str);
					}
					else if (found == (input_str_length - 1))
					{
						std::strcpy(output, "[0,\"Error Invalid Format\"]");
						logger->error("extDB: Invalid Format: {0}", input_str);
					}
					else
					{
						const std::string protocol = input_str.substr(2,(found-2));
						// Data
						const std::string data = input_str.substr(found+1);
						io_service.post(boost::bind(&Ext::onewayCallProtocol, this, output_size, protocol, data));
						std::strcpy(output, "[1]");
					}
					break;
				}
				case 0: //SYNC
				{
					// Protocol
					const std::string::size_type found = input_str.find(":",2);

					if (found==std::string::npos)  // Check Invalid Format
					{
						std::strcpy(output, "[0,\"Error Invalid Format\"]");
						logger->error("extDB: Invalid Format: {0}", input_str);
					}
					else if (found == (input_str_length - 1))
					{
						std::strcpy(output, "[0,\"Error Invalid Format\"]");
						logger->error("extDB: Invalid Format: {0}", input_str);
					}
					else
					{
						syncCallProtocol(output, output_size, input_str.substr(2,(found-2)), input_str.substr(found+1));
					}
					break;
				}
				case 9: // SYSTEM CALLS / SETUP
				{
					Poco::StringTokenizer tokens(input_str, ":");
					if (extDB_info.extDB_lock)
					{
						if (tokens.count() == 2)
						{
							if (tokens[1] == "VERSION")
							{
								std::strcpy(output, getVersion().c_str());
							}
							else if (tokens[1] == "LOCK_STATUS")
							{
								std::strcpy(output, ("[1]"));
							}
							else
							{
								// Invalid Format
								std::strcpy(output, "[0,\"Error Invalid Format\"]");
								logger->error("extDB: Invalid Format: {0}", input_str);
							}
						}
						else
						{
							// Invalid Format
							std::strcpy(output, "[0,\"Error Invalid Format\"]");
							logger->error("extDB: Invalid Format: {0}", input_str);
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
										steam_worker_thread.start(steam_worker);
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
									std::strcpy(output, getVersion().c_str());
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
								else if (tokens[1] == "OUTPUTSIZE")
								{
									std::string outputsize_str(Poco::NumberFormatter::format(output_size));
									logger->info("Extension Output Size: {0}", outputsize_str);
									std::strcpy(output, outputsize_str.c_str());
								}
								else
								{
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB: Invalid Format: {0}", input_str);
								}
								break;
							case 3:
								// RCON
								if (tokens[1] == "START_RCON")
								{
									connectRCon(output, output_size, tokens[2]);
								}
								else if (tokens[1] == "DATABASE")
								{
									connectDatabase(output, output_size, tokens[2], tokens[2]);
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB: Invalid Format: {0}", input_str);
								}
								break;
							case 4:
								// DATABASE
								if (tokens[1] == "DATABASE")
								{
									connectDatabase(output, output_size, tokens[2], tokens[3]);
								}
								else if (tokens[1] == "ADD")
								{
									addProtocol(output, output_size, "", tokens[2], tokens[3], "");	
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB: Invalid Format: {0}", input_str);
								}
								break;
							case 5:
								//ADD PROTOCOL
								if (tokens[1] == "ADD")
								{
									addProtocol(output, output_size, tokens[2], tokens[3], tokens[4], "");
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB: Invalid Format: {0}", input_str);
								}
								break;
							case 6:
								if (tokens[1] == "ADD")
								{
									addProtocol(output, output_size, tokens[2], tokens[3], tokens[4], tokens[5]);
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB: Invalid Format: {0}", input_str);
								}
								break;
							default:
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB: Invalid Format: {0}", input_str);
								}
						}
					}
					break;
				}
				default:
				{
					std::strcpy(output, "[0,\"Error Invalid Message\"]");
					logger->error("extDB: Invalid Message: {0}", input_str);
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
		#ifdef TESTING
			console->critical("extDB: Error: {0}", e.displayText());
		#endif
		logger->critical("extDB: Error: {0}", e.displayText());
	}
}


#ifdef TEST_APP
int main(int nNumberofArgs, char* pszArgs[])
{
	char result[80];
	std::string input_str;

	Ext *extension;
	std::string current_path;
	extension = (new Ext(current_path));

	extension->console->info("Welcome to extDB Test Application : v{0}", extension->getVersion());
	extension->console->info("OutputSize is set to 80 for Test Application, just so it is readable ");
	extension->console->info("OutputSize for Arma3 is more like 10k in size ");
	extension->console->info("To exit type 'quit'");

	bool test = false;
	int test_counter = 0;
	for (;;)
	{
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
			extension->callExtenion(result, 80, input_str.c_str());
			extension->console->info("extDB: {0}", result);
		}
		while (test)
		{
			if (test_counter >= 10000)
			{
				test_counter = 0;
				test = false;
				break;
			}
			test_counter = test_counter + 1;
			extension->callExtenion(result, 80, std::string("1:SQL:TEST:testing").c_str());
			extension->callExtenion(result, 80, std::string("1:SQL:TEST2:testing").c_str());
			extension->callExtenion(result, 80, std::string("1:SQL:TEST3:testing").c_str());
			extension->callExtenion(result, 80, std::string("1:SQL:TEST4:testing").c_str());
			extension->callExtenion(result, 80, std::string("1:SQL:TEST5:testing").c_str());
			extension->console->info("extDB: {0}", result);			
		}
	}
	extension->stop();
	return 0;
}
#endif
