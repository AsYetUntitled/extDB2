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


#include "sql_custom.h"

#include <algorithm>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/filesystem.hpp>

#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Session.h>

#include <Poco/Data/MySQL/Connector.h>
#include <Poco/Data/MySQL/MySQLException.h>
#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Data/SQLite/SQLiteException.h>

#include <Poco/StringTokenizer.h>
#include <Poco/Util/AbstractConfiguration.h>
#include <Poco/Util/IniFileConfiguration.h>

#include <Poco/DigestEngine.h>
#include <Poco/MD5Engine.h>

#include <Poco/Exception.h>

#include "../sanitize.h"


bool SQL_CUSTOM::init(AbstractExt *extension, const std::string &database_id, const std::string &init_str)
{
	extension_ptr = extension;
	if (extension_ptr->extDB_connectors_info.databases.count(database_id) == 0)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB2: SQL_CUSTOM: No Database Connection ID: {0}", database_id);
		#endif
		extension_ptr->logger->warn("extDB2: SQL_CUSTOM: No Database Connection ID: {0}", database_id);
		return false;
	}

	database_ptr = &extension_ptr->extDB_connectors_info.databases[database_id];
	if ((database_ptr->type != std::string("MySQL")) && (database_ptr->type != std::string("SQLite")))
	{
		// DATABASE NOT SETUP YET
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB2: SQL_CUSTOM: Database Type Not Supported");
		#endif
		extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Database Type Not Supported");
		return false;
	}

	// Check if SQL_CUSTOM Template Filename Given
	if (init_str.empty()) 
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB2: SQL_CUSTOM: Missing Init Parameter");
		#endif
		extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Missing Init Parameter");
		return false;
	}

	Poco::AutoPtr<Poco::Util::IniFileConfiguration> template_ini;
	template_ini = new Poco::Util::IniFileConfiguration();

	boost::filesystem::path sql_custom_path(extension_ptr->extDB_info.path);
	sql_custom_path /= "extDB";
	sql_custom_path /= "sql_custom";
	boost::filesystem::create_directories(sql_custom_path); // Creating Directory if missing

	bool status = false;

	boost::filesystem::path custom_ini_path(sql_custom_path);
	custom_ini_path /= (init_str + ".ini");
	std::string custom_ini_file;
	if (boost::filesystem::exists(custom_ini_path))
	{
		if (boost::filesystem::is_regular_file(custom_ini_path))
		{
			status = true;
			custom_ini_file = custom_ini_path.string();
			template_ini->loadExtra(custom_ini_file);
			#ifdef DEBUG_TESTING
				extension_ptr->console->info("extDB2: SQL_CUSTOM: Loading Template Filename: {0}", custom_ini_file);
			#endif
			extension_ptr->logger->info("extDB2: SQL_CUSTOM: Loading Template Filename: {0}", custom_ini_file);
		}
		else
		{
			#ifdef DEBUG_TESTING
				extension_ptr->console->info("extDB2: SQL_CUSTOM: Loading Template Error: Not Regular File: {0}", custom_ini_file);
			#endif
			extension_ptr->logger->info("extDB2: SQL_CUSTOM: Loading Template Error: Not Regular File: {0}", custom_ini_file);
		}
	}
	else
	{
		custom_ini_path = sql_custom_path;
		custom_ini_path /= init_str;
		if (boost::filesystem::is_directory(custom_ini_path))
		{
			for (boost::filesystem::directory_iterator it(custom_ini_path); it != boost::filesystem::directory_iterator(); ++it)
			{
				if (boost::filesystem::is_regular_file(it->path()))
				{
					status = true;
					custom_ini_file = it->path().string();
					template_ini->loadExtra(custom_ini_file);
					#ifdef DEBUG_TESTING
						extension_ptr->console->info("extDB2: SQL_CUSTOM: Loading Template Filename: {0}", custom_ini_file);
					#endif
					extension_ptr->logger->info("extDB2: SQL_CUSTOM: Loading Template Filename: {0}", custom_ini_file);
				}
			}
		}
	}
	
	// Read Template File
	if (status)
	{		
		std::vector<std::string> custom_calls_list;
		template_ini->keys(custom_calls_list);

		if ((template_ini->getInt("Default.Version", 1)) <= EXTDB_SQL_CUSTOM_LATEST_VERSION)
		{
			extension_ptr->logger->info("extDB2: SQL_CUSTOM: SQL_VERSION_V2 is available");
			extension_ptr->logger->info("extDB2: SQL_CUSTOM: Newer SQL_CUSTOM Version Available");
		}

		if ((template_ini->getInt("Default.Version", 1)) >= EXTDB_SQL_CUSTOM_REQUIRED_VERSION)
		{
			int default_number_of_inputs = template_ini->getInt("Default.Number of Inputs", 0);
			int default_number_of_custom_inputs = template_ini->getInt("Default.Number of Custom Inputs", 0);

			bool default_input_sanitize_value_check = template_ini->getBool("Default.Sanitize Input Value Check", true);
			bool default_output_sanitize_value_check = template_ini->getBool("Default.Sanitize Output Value Check", true);
			bool default_preparedStatement_cache = template_ini->getBool("Default.Prepared Statement Cache", true);
			bool default_returnInsertID = template_ini->getBool("Default.Return InsertID", false);


			bool default_strip = template_ini->getBool("Default.Strip", false);
			std::string default_strip_chars = template_ini->getString("Default.Strip Chars", "");
			std::string default_strip_custom_input_chars = template_ini->getString("Default.Strip Custom Chars", "");
			int default_strip_chars_action = 0;

			std::string strip_chars_action_str = template_ini->getString("Default.Strip Chars Action", "Strip");
			if	(boost::algorithm::iequals(strip_chars_action_str, std::string("Strip")) == 1)
			{
				default_strip_chars_action = 1;
			}
			else if	(boost::algorithm::iequals(strip_chars_action_str, std::string("Strip+Log")) == 1)
			{
				default_strip_chars_action = 2;
			}
			else if	(boost::algorithm::iequals(strip_chars_action_str, std::string("Strip+Error")) == 1)
			{
				default_strip_chars_action = 3;
			}
			else if (boost::algorithm::iequals(strip_chars_action_str, std::string("None")) == 1)
			{
				default_strip_chars_action = 0;
			}
			else
			{
				#ifdef DEBUG_TESTING
					extension_ptr->console->warn("extDB2: SQL_CUSTOM: Invalid Default Strip Chars Action: {0}", strip_chars_action_str);
				#endif
				extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Invalid Default Strip Chars Action: {0}", strip_chars_action_str);
			}

			for(std::string &call_name : custom_calls_list)
			{
				int sql_line_num = 0;
				int sql_part_num = 0;

				std::string sql_line_num_str;
				std::string sql_part_num_str;

				custom_calls[call_name].number_of_inputs = template_ini->getInt(call_name + ".Number of Inputs", default_number_of_inputs);
				custom_calls[call_name].number_of_custom_inputs = template_ini->getInt(call_name + ".Number of Custom Inputs", default_number_of_custom_inputs);
				custom_calls[call_name].preparedStatement_cache = template_ini->getBool(call_name + ".Prepared Statement Cache", default_preparedStatement_cache);
				custom_calls[call_name].returnInsertID = template_ini->getBool(call_name + ".Return InsertID", default_returnInsertID);

				if (template_ini->has(call_name + ".Strip Chars Action"))
				{
					strip_chars_action_str = template_ini->getString(call_name + ".Strip Chars Action", "");
					if	(boost::algorithm::iequals(strip_chars_action_str, std::string("Strip")) == 1)
					{
						custom_calls[call_name].strip_chars_action = 1;
					}
					else if	(boost::algorithm::iequals(strip_chars_action_str, std::string("Strip+Log")) == 1)
					{
						custom_calls[call_name].strip_chars_action = 2;
					}
					else if	(boost::algorithm::iequals(strip_chars_action_str, std::string("Strip+Error")) == 1)
					{
						custom_calls[call_name].strip_chars_action = 3;
					}
					else if (boost::algorithm::iequals(strip_chars_action_str, std::string("None")) == 1)
					{
						custom_calls[call_name].strip_chars_action = 0;
					}
					else
					{
						#ifdef DEBUG_TESTING
							extension_ptr->console->warn("extDB2: SQL_CUSTOM: Invalid Strip Chars Action: {0}", strip_chars_action_str);
						#endif
						extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Invalid Strip Chars Action: {0}", strip_chars_action_str);
						custom_calls[call_name].strip_chars_action = 1;
					}
				}
				else
				{
					custom_calls[call_name].strip_chars_action = default_strip_chars_action;
				}
				
				custom_calls[call_name].strip = template_ini->getBool(call_name + ".Strip", default_strip);
				custom_calls[call_name].strip_custom_input_chars = template_ini->getString(call_name + ".Strip Custom Chars", default_strip_custom_input_chars);
				custom_calls[call_name].strip_chars = template_ini->getString(call_name + ".Strip Chars", default_strip_chars);

				while (true)
				{
					// Prepared Statement
					++sql_line_num;
					sql_line_num_str = Poco::NumberFormatter::format(sql_line_num);

					if (!(template_ini->has(call_name + ".SQL" + sql_line_num_str + "_1")))  // No More SQL Statements
					{
						// Initialize Default Output Options

						// Get Output Options
						Poco::StringTokenizer tokens_output_options((template_ini->getString(call_name + ".OUTPUT", "")), ",", Poco::StringTokenizer::TOK_TRIM);

						for (int i = 0; i < (tokens_output_options.count()); ++i)
						{
							Value_Options outputs_options;
							outputs_options.check =  template_ini->getBool(call_name + ".Sanitize Output Value Check", default_output_sanitize_value_check);

							Poco::StringTokenizer options_tokens(tokens_output_options[i], "-", Poco::StringTokenizer::TOK_TRIM);
							for (int x = 0; x < (options_tokens.count()); ++x)
							{
								int temp_int;
								if (Poco::NumberParser::tryParse(options_tokens[x], temp_int))
								{
									outputs_options.number = temp_int;
								}
								else
								{
									if (boost::algorithm::iequals(options_tokens[x], std::string("String")) == 1)
									{
										outputs_options.string = true;
									}
									else if (boost::algorithm::iequals(options_tokens[x], std::string("String_Escape_Quotes")) == 1)
									{
										outputs_options.string_escape_quotes = true;
									}
									else if (boost::algorithm::iequals(options_tokens[x], std::string("Bool")) == 1)
									{
										outputs_options.boolean = true;
									}
									else if (boost::algorithm::iequals(options_tokens[x], std::string("BeGUID")) == 1)
									{
										outputs_options.beguid = true;
									}
									else if (boost::algorithm::iequals(options_tokens[x], std::string("Check")) == 1)
									{
										outputs_options.check = true;
									}
									else if (boost::algorithm::iequals(options_tokens[x], std::string("NoCheck")) == 1)
									{
										outputs_options.check = false;
									}
									else if (boost::algorithm::iequals(options_tokens[x], std::string("Strip")) == 1)
									{
										outputs_options.strip = true;
									}
									else if (boost::algorithm::iequals(options_tokens[x], std::string("NoStrip")) == 1)
									{
										outputs_options.strip = false;
									}
									else if (boost::algorithm::iequals(options_tokens[x], std::string("Vac_SteamID")) == 1)
									{
										outputs_options.vac_steamID = true;
									}
									else if (boost::algorithm::iequals(options_tokens[x], std::string("DateTime_ISO8601")) == 1)
									{
										outputs_options.datetime_iso8601 = true;
									}
									else
									{
										status = false;
										#ifdef DEBUG_TESTING
											extension_ptr->console->warn("extDB2: SQL_CUSTOM: Invalid Strip Output Option: {0}: {1}", call_name, options_tokens[x]);
										#endif
										extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Invalid Strip Output Option: {0}: {1}", call_name, options_tokens[x]);
									}
								}
							}
							custom_calls[call_name].sql_outputs_options.push_back(std::move(outputs_options));
						}
						break;
					}
					
					std::string sql_str;
					sql_part_num = 0;
					while (true)
					{
						++sql_part_num;
						sql_part_num_str = Poco::NumberFormatter::format(sql_part_num);							
						if (!(template_ini->has(call_name + ".SQL" + sql_line_num_str + "_" + sql_part_num_str)))
						{
							break;
						}
						sql_str += (template_ini->getString(call_name + ".SQL" + sql_line_num_str + "_" + sql_part_num_str)) + " " ;		
					}

					if (sql_part_num > 1) // Remove trailing Whitespace
					{
						sql_str = sql_str.substr(0, sql_str.size()-1);
					}

					custom_calls[call_name].sql_prepared_statements.push_back(std::move(sql_str));
					custom_calls[call_name].sql_inputs_options.push_back(std::vector < Value_Options >());

					// Get Input Options
					Poco::StringTokenizer tokens_input(template_ini->getString((call_name + ".SQL" + sql_line_num_str + "_INPUTS"), ""), ",");
					for (auto &token_input : tokens_input)
					{
						// Initialize Default Input Options
						Value_Options inputs_options;
						inputs_options.check =  template_ini->getBool(call_name + ".Sanitize Input Value Check", default_input_sanitize_value_check);
						
						Poco::StringTokenizer tokens_input_options(token_input, "-");
						for (auto &sub_token_input : tokens_input_options)
						{
							int temp_int;
							if (Poco::NumberParser::tryParse(sub_token_input, temp_int))
							{
								inputs_options.number = temp_int;
							}
							else
							{
								if (boost::algorithm::iequals(sub_token_input, std::string("String")) == 1)
								{
									inputs_options.string = true;
								}
								else if (boost::algorithm::iequals(sub_token_input, std::string("String_Escape_Quotes")) == 1)
								{
									inputs_options.string_escape_quotes = true;
								}
								else if (boost::algorithm::iequals(sub_token_input, std::string("BeGUID")) == 1)
								{
									inputs_options.beguid = true;
								}
								else if (boost::algorithm::iequals(sub_token_input, std::string("Bool")) == 1)
								{
									inputs_options.boolean = true;
								}
								else if (boost::algorithm::iequals(sub_token_input, std::string("Check")) == 1)
								{
									inputs_options.check = true;
								}
								else if (boost::algorithm::iequals(sub_token_input, std::string("Check_Add_Quotes")) == 1)
								{
									inputs_options.check_add_quotes = true;
								}
								else if (boost::algorithm::iequals(sub_token_input, std::string("Check_Add_Escape_Quotes")) == 1)
								{
									inputs_options.check_add_escape_quotes = true;
								}
								else if (boost::algorithm::iequals(sub_token_input, std::string("NoCheck")) == 1)
								{
									inputs_options.check = false;
								}
								else if (boost::algorithm::iequals(sub_token_input, std::string("Strip")) == 1)
								{
									inputs_options.strip = true;
								}
								else if (boost::algorithm::iequals(sub_token_input, std::string("NoStrip")) == 1)
								{
									inputs_options.strip = false;
								}
								else if (boost::algorithm::iequals(sub_token_input, std::string("Vac_SteamID")) == 1)
								{
									inputs_options.vac_steamID = true;
								}
								else
								{
									status = false;
									#ifdef DEBUG_TESTING
										extension_ptr->console->warn("extDB2: SQL_CUSTOM: Invalid Strip Input Option: {0}: {1}", call_name, sub_token_input);
									#endif
									extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Invalid Strip Input Option: {0}: {1}", call_name, sub_token_input);
								}
							}
						}
						custom_calls[call_name].sql_inputs_options[sql_line_num - 1].push_back(std::move(inputs_options));
					}
				}
			}
		}
		else
		{
			status = false;
			#ifdef DEBUG_TESTING
				extension_ptr->console->warn("extDB2: SQL_CUSTOM: Incompatible Version: {0} Required: {1}", (template_ini->getInt("Default.Version", 1)), EXTDB_SQL_CUSTOM_REQUIRED_VERSION);
			#endif
			extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Incompatible Version: {0} Required: {1}", (template_ini->getInt("Default.Version", 1)), EXTDB_SQL_CUSTOM_REQUIRED_VERSION);
		}
	}
	return status;
}


void SQL_CUSTOM::getBEGUID(std::string &input_str, std::string &result)
// From Frank https://gist.github.com/Fank/11127158
// Modified to use lib poco
{
	if (input_str.empty())
	{
		result = "Invalid SteamID";
	}
	else
	{
		Poco::Int64 steamID;
		if (Poco::NumberParser::tryParse64(input_str, steamID))
		{
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

			std::lock_guard<std::mutex> lock(mutex_md5);
			md5.update(bestring.str());
			result = Poco::DigestEngine::digestToHex(md5.digest());
		}
		else
		{
			result = "Invalid SteamID";			
		}
	}
}


void SQL_CUSTOM::getResult(std::string &input_str, Custom_Call_UnorderedMap::const_iterator &custom_calls_itr, Poco::Data::Session &session, Poco::Data::Statement &sql_statement, std::string &result, bool &status)
{
	try
	{
		if (custom_calls_itr->second.returnInsertID)
		{
			if (!session.isConnected())
			{
				result = "[1,[-1,["; // Return -1 If Session Died
			}
			else
			{
				Poco::UInt64 insertID;
				std::string insertID_str;
				//insertID = Poco::AnyCast<Poco::UInt64>(session.getProperty("insertId"));

				// Workaround
				bool status = true;
				Poco::Data::Statement sql(session);
				sql << "SELECT LAST_INSERT_ID()", Poco::Data::Keywords::into(insertID);
				executeSQL(sql, result, status);
				// End of Workaround

				insertID_str = Poco::NumberFormatter::format(insertID);
				if (status)
				{
					result = "[1,[" + insertID_str + ",[";
				}
				else
				{
					result = "[1,[0,["; // Return 0 if insertID fails
				}
			}
		}
		else
		{
			result = "[1,[";
		}

		bool sanitize_value_check = true;
		Poco::Data::RecordSet rs(sql_statement);

		std::size_t cols = rs.columnCount();
		if (cols >= 1)
		{
			std::string temp_str;
			temp_str.reserve(result.capacity()); // Default temp_str Size is same capacity of Result which is same size of outputsize for callExtension

			result += "[";
			bool more = rs.moveFirst();
			while (more)
			{
				std::size_t sql_output_options_size = custom_calls_itr->second.sql_outputs_options.size();

				for (std::size_t col = 0; col < cols; ++col)
				{
					if (rs[col].isEmpty())
					{
						temp_str.clear();
					}
					else
					{
						temp_str = rs[col].convert<std::string>();
					}
					
					// NO OUTPUT OPTIONS 
					if (col >= sql_output_options_size)
					{
						// DEFAULT BEHAVIOUR
						if (temp_str.empty())
						{
							result += "\"\"";
						}
						else
						{
							result += temp_str;
						}
					}
					else
					{
						// STEAM ID + QUERYS
						if (custom_calls_itr->second.sql_outputs_options[col].vac_steamID)
						{
							// QUERY STEAM
							extension_ptr->steamQuery(-1, false, true, temp_str, true);
						}
						if (custom_calls_itr->second.sql_outputs_options[col].beguid)
						{
							// GENERATE BEGUID
							getBEGUID(temp_str, temp_str);
						}

						// STRING
						if (custom_calls_itr->second.sql_outputs_options[col].string)
						{
							if (temp_str.empty())
							{
								temp_str = "\"\"";
							}
							else
							{
								boost::erase_all(temp_str, "\"");
								boost::erase_all(temp_str, "'");
								temp_str = "\"" + temp_str + "\"";
							}
						}
						else if (custom_calls_itr->second.sql_outputs_options[col].string_escape_quotes)
						{
							if (temp_str.empty())
							{
								temp_str = "\"\"";
							}
							else
							{
								boost::replace_all(temp_str, "\"", "\"\"");
								boost::replace_all(temp_str, "'", "''");
								temp_str = "\"" + temp_str + "\"";
							}
						}
						// DateTime_ISO8601
						else if (custom_calls_itr->second.sql_outputs_options[col].datetime_iso8601)
						{
							if (temp_str.empty())
							{
								temp_str = "[]";
							}
							else
							{
								int tzd = 0;
								Poco::DateTime dt = Poco::DateTimeParser::parse(Poco::DateTimeFormat::ISO8601_FRAC_FORMAT, temp_str, tzd);
								temp_str = "[" + Poco::NumberFormatter::format(dt.year()) + ","
									+ Poco::NumberFormatter::format(dt.month()) + ","
									+ Poco::NumberFormatter::format(dt.day()) + ","
									+ Poco::NumberFormatter::format(dt.hour()) + ","
									+ Poco::NumberFormatter::format(dt.minute()) + ","
									+ Poco::NumberFormatter::format(dt.second()) + "]";
							}
						}

						// BOOL
						else if (custom_calls_itr->second.sql_outputs_options[col].boolean)
						{
							if (temp_str.empty())
							{
								temp_str = "false";
							}
							else
							{
								if (rs[col].isInteger())
								{
									if (rs[col].convert<int>() > 0)
									{
										temp_str = "true";
									}
									else
									{
										temp_str = "false";
									}
								}
								else
								{
									temp_str = "false";
								}
							}
						}
						else if (temp_str.empty())
						{
							temp_str = "\"\"";
						}

						// SANITIZE CHECK
						if (custom_calls_itr->second.sql_outputs_options[col].check)
						{
							if (!(Sqf::check(temp_str)))
							{
								extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Sanitize Check Error: Input: {0}", input_str);
								extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Sanitize Check Error: Value: {0}", temp_str);
								sanitize_value_check = false;
								break;
							}
						}
						result += temp_str;
					}

					if (col < (cols - 1))
					{
						result += ",";
					}
				}
				more = rs.moveNext();
				if (more)
				{
					result += "],[";
				}
			}
			result += "]";
		}
		if (!(sanitize_value_check))
		{
			result = "[0,\"Error Value Failed Sanitize Check\"]";
		}
		else
		{
			if (custom_calls_itr->second.returnInsertID)
			{
				result += "]]]";
			}
			else
			{
				result += "]]";
			}
		}
	}
	catch (Poco::NotImplementedException& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error NotImplementedException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error NotImplementedException: {0}", e.displayText());
		result = "[0,\"Error NotImplemented Exception\"]";
	}
	catch (Poco::Exception& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error Exception: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error Exception: {0}", e.displayText());
		result = "[0,\"Error Exception\"]";
	}
}


void SQL_CUSTOM::executeSQL(Poco::Data::Statement &sql_statement, std::string &result, bool &status)
{
	try
	{
		sql_statement.execute();
	}
	catch (Poco::InvalidAccessException& e)
	{
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error NotConnectedException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error NotConnectedException: {0}", e.displayText());
		result = "[0,\"Error NotConnected Exception\"]";
	}
	catch (Poco::Data::NotConnectedException& e)
	{
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error NotConnectedException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error NotConnectedException: {0}", e.displayText());
		result = "[0,\"Error NotConnected Exception\"]";
	}
	catch (Poco::NotImplementedException& e)
	{
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error NotImplementedException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error NotImplementedException: {0}", e.displayText());
		result = "[0,\"Error NotImplemented Exception\"]";
	}
	catch (Poco::Data::SQLite::DBLockedException& e)
	{
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error DBLockedException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error DBLockedException: {0}", e.displayText());
		result = "[0,\"Error DBLocked Exception\"]";
	}
	catch (Poco::Data::MySQL::ConnectionException& e)
	{
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error ConnectionException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error ConnectionException: {0}", e.displayText());
		result = "[0,\"Error Connection Exception\"]";
	}
	catch(Poco::Data::MySQL::StatementException& e)
	{
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error StatementException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error StatementException: {0}", e.displayText());
		result = "[0,\"Error Statement Exception\"]";
	}
	catch (Poco::Data::ConnectionFailedException& e)
	{
		// Error
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error ConnectionFailedException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error ConnectionFailedException: {0}", e.displayText());
		result = "[0,\"Error ConnectionFailedException\"]";
	}
	catch (Poco::Data::DataException& e)
	{
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error DataException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error DataException: {0}", e.displayText());
		result = "[0,\"Error Data Exception\"]";
	}
	catch (Poco::Exception& e)
	{
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error Exception: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error Exception: {0}", e.displayText());
		result = "[0,\"Error Exception\"]";
	}
}


void SQL_CUSTOM::callPreparedStatement(std::string &input_str, std::string &call_name, Custom_Call_UnorderedMap::const_iterator custom_calls_itr, std::vector< std::vector< std::string > > &all_processed_inputs, bool &status, std::string &result)
{
	Poco::Data::SessionPool::SessionDataPtr session_data_ptr;
	try
	{
		Poco::Data::Session session = extension_ptr->getDBSession_mutexlock(*database_ptr, session_data_ptr);

		std::unordered_map <std::string, Poco::Data::SessionPool::StatementCache>::iterator statement_cache_itr = session_data_ptr->statements_map.find(call_name);
		if (statement_cache_itr != session_data_ptr->statements_map.end())
		{
			// CACHE
			for (std::vector<int>::size_type i = 0; i != statement_cache_itr->second.size(); ++i)
			{
				statement_cache_itr->second[i].bindClear();
				for (auto &processed_input : all_processed_inputs[i])
				{
					statement_cache_itr->second[i], Poco::Data::Keywords::use(processed_input);
				}
				statement_cache_itr->second[i].bindFixup();

				executeSQL(statement_cache_itr->second[i], result, status);
				if (!status)
				{
					break;
				}
				else 
				{
					if (i == (statement_cache_itr->second.size() - 1))
					{
						getResult(input_str, custom_calls_itr, session, statement_cache_itr->second[i], result, status);
					}
				}
			}
		}
		else
		{
			// NO CACHE
			int i = -1;
			for (std::vector< std::string >::const_iterator it_sql_prepared_statements_vector = custom_calls_itr->second.sql_prepared_statements.begin(); it_sql_prepared_statements_vector != custom_calls_itr->second.sql_prepared_statements.end(); ++it_sql_prepared_statements_vector)
			{
				++i;

				Poco::Data::Statement sql_statement(session);
				sql_statement << *it_sql_prepared_statements_vector;

				for (auto &processed_input : all_processed_inputs[i])
				{
					sql_statement, Poco::Data::Keywords::use(processed_input);
				}

				executeSQL(sql_statement, result, status);
				if (!status)
				{
					break;
				}
				else
				{
					if ( it_sql_prepared_statements_vector+1 == custom_calls_itr->second.sql_prepared_statements.end() )
					{
						getResult(input_str, custom_calls_itr, session, sql_statement, result, status);
					}
					if (custom_calls_itr->second.preparedStatement_cache)
					{
						session_data_ptr->statements_map[call_name].push_back(std::move(sql_statement));
					}
				}
			}
		}
		if (!status)
		{
			#ifdef DEBUG_TESTING
				extension_ptr->console->error("extDB2: SQL_CUSTOM: Wiping Statements + Session");
			#endif
			extension_ptr->logger->error("extDB2: SQL_CUSTOM: Wiping Statements + Session");
			session_data_ptr->statements_map.clear();
		}
	}
	catch (Poco::Data::MySQL::ConnectionException& e)
	{
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error ConnectionException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error ConnectionException: {0}", e.displayText());
		result = "[0,\"Error Connection Exception\"]";
		if (!session_data_ptr.isNull())
		{
 			session_data_ptr->statements_map.clear();
		}
	}
	catch (Poco::Data::ConnectionFailedException& e)
	{
		// Error
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error ConnectionFailedException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error ConnectionFailedException: {0}", e.displayText());
		result = "[0,\"Error ConnectionFailedException\"]";
		if (!session_data_ptr.isNull())
		{
 			session_data_ptr->statements_map.clear();
		}
	}
}


void SQL_CUSTOM::callPreparedStatement(std::string &input_str, std::string &call_name, Custom_Call_UnorderedMap::const_iterator custom_calls_itr, std::vector< std::vector<std::string> > &all_processed_inputs, std::vector<std::string> &custom_inputs, bool &status, std::string &result)
{
	Poco::Data::SessionPool::SessionDataPtr session_data_ptr;
	try
	{
		Poco::Data::Session session = extension_ptr->getDBSession_mutexlock(*database_ptr, session_data_ptr);

		std::string sql_str;
		int i = -1;
		for (std::vector< std::string >::const_iterator it_sql_prepared_statements_vector = custom_calls_itr->second.sql_prepared_statements.begin(); it_sql_prepared_statements_vector != custom_calls_itr->second.sql_prepared_statements.end(); ++it_sql_prepared_statements_vector)
		{
			++i;
			Poco::Data::Statement sql_statement(session);
			sql_str = *it_sql_prepared_statements_vector;
			int x = 0;
			for (auto replace_str : custom_inputs)
			{
				++x;
				boost::replace_all(sql_str, ("$CUSTOM_" + Poco::NumberFormatter::format(x) + "$"), replace_str); 
			}
			sql_statement << sql_str;

			for (auto &processed_input : all_processed_inputs[i])
			{
				sql_statement, Poco::Data::Keywords::use(processed_input);
			}

			executeSQL(sql_statement, result, status);
			if (status && (it_sql_prepared_statements_vector + 1 == custom_calls_itr->second.sql_prepared_statements.end()))
			{
				getResult(input_str, custom_calls_itr, session, sql_statement, result, status);
			}
		}
		if (!status)
		{
			#ifdef DEBUG_TESTING
				extension_ptr->console->error("extDB2: SQL_CUSTOM: Clearing Any Cached Statements");
			#endif
			extension_ptr->logger->error("extDB2: SQL_CUSTOM: Clearing Any Cached Statements");
			session_data_ptr->statements_map.clear();
		}
	}
	catch (Poco::Data::MySQL::ConnectionException& e)
	{
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error ConnectionException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error ConnectionException: {0}", e.displayText());
		result = "[0,\"Error Connection Exception\"]";
		if (!session_data_ptr.isNull())
		{
 			session_data_ptr->statements_map.clear();
		}
	}
	catch (Poco::Data::ConnectionFailedException& e)
	{
		// Error
		status = false;
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_CUSTOM: Error ConnectionFailedException: {0}", e.displayText());
		#endif
		extension_ptr->logger->error("extDB2: SQL_CUSTOM: Error ConnectionFailedException: {0}", e.displayText());
		result = "[0,\"Error ConnectionFailedException\"]";
		if (!session_data_ptr.isNull())
		{
 			session_data_ptr->statements_map.clear();
		}
	}
}


bool SQL_CUSTOM::callProtocol(std::string input_str, std::string &result, const bool async_method, const unsigned int unique_id)
{
	#ifdef DEBUG_TESTING
		extension_ptr->console->info("extDB2: SQL_CUSTOM: Trace: UniqueID: {0} Input: {1}", unique_id, input_str);
	#endif
	#ifdef DEBUG_LOGGING
		extension_ptr->logger->info("extDB2: SQL_CUSTOM: Trace: UniqueID: {0} Input: {1}", unique_id, input_str);
	#endif

	Poco::StringTokenizer tokens(input_str, ":");
	auto custom_calls_const_itr = custom_calls.find(tokens[0]);
	if (custom_calls_const_itr == custom_calls.end())
	{
		// NO CALLNAME FOUND IN PROTOCOL
		result = "[0,\"Error No Custom Call Not Found\"]";
		extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Error No Custom Call Not Found: Input String {0}", input_str);
		extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Error No Custom Call Not Found: Callname {0}", tokens[0]);
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB2: SQL_CUSTOM: Error No Custom Call Not Found: Input String {0}", input_str);
			extension_ptr->console->warn("extDB2: SQL_CUSTOM: Error No Custom Call Not Found: Callname {0}", tokens[0]);
		#endif
	}
	else
	{
		if ((custom_calls_const_itr->second.number_of_inputs + custom_calls_const_itr->second.number_of_custom_inputs) != (tokens.count() - 1))
		{
			// BAD Number of Inputs
			result = "[0,\"Error Incorrect Number of Inputs\"]";
			extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Incorrect Number of Inputs: Input String {0}", input_str);
			extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Incorrect Number of Inputs: Expected: {0} Got: {1}", (custom_calls_const_itr->second.number_of_inputs + custom_calls_const_itr->second.number_of_custom_inputs), (tokens.count() - 1));
			#ifdef DEBUG_TESTING
				extension_ptr->console->warn("extDB2: SQL_CUSTOM: Incorrect Number of Inputs: Input String {0}", input_str);
				extension_ptr->console->warn("extDB2: SQL_CUSTOM: Incorrect Number of Inputs: Expected: {0} Got: {1}", (custom_calls_const_itr->second.number_of_inputs + custom_calls_const_itr->second.number_of_custom_inputs), (tokens.count() - 1));
			#endif
		}
		else
		{
			// GOOD Number of Inputs
			bool status = true;
			bool strip_chars_detected = false;

			std::vector<std::string> inputs;
			std::vector<std::string> custom_inputs;

			if (custom_calls_const_itr->second.number_of_custom_inputs == 0)
			{
				inputs.insert(inputs.begin(), tokens.begin(), tokens.end());
			}
			else
			{
				auto itr = tokens.begin();
				std::advance(itr, custom_calls_const_itr->second.number_of_inputs + 1);
				inputs.insert(inputs.begin(), tokens.begin(), itr);

				//std::advance(itr, 1);
				custom_inputs.insert(custom_inputs.begin(), itr, tokens.end());

				for (auto &custom_input : custom_inputs)
				{
					for (auto &strip_char : custom_calls_const_itr->second.strip_custom_input_chars)
					{
						boost::erase_all(custom_input, std::string(1, strip_char));
					}
				}
			}
			
			// Multiple INPUT Lines
			std::vector<std::vector<std::string> > all_processed_inputs;
			all_processed_inputs.reserve(custom_calls_const_itr->second.sql_inputs_options.size());

			std::string sanitize_str;

			for(auto &sql_inputs_options : custom_calls_const_itr->second.sql_inputs_options)
			{
				std::vector< std::string > processed_inputs;
				for(auto &sql_input_option : sql_inputs_options)
				{
					std::string temp_str = inputs[sql_input_option.number];
					// INPUT Options

					// Strip
					if (sql_input_option.strip)
					{
						for (auto &strip_char : custom_calls_const_itr->second.strip_chars)
						{
							boost::erase_all(temp_str, std::string(1, strip_char));
						}
						if (temp_str != inputs[sql_input_option.number])
						{
							strip_chars_detected = true;
							switch (custom_calls_const_itr->second.strip_chars_action)
							{
								case 3: // Strip + Log + Error
									status = false;
								case 2: // Strip + Log
									extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Error Bad Char Detected: Input: {0}", input_str);
									extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Error Bad Char Detected: Token: {0}", sql_input_option.number);
								case 1: // Strip
									result = "[0,\"Error Strip Char Found\"]";
									break;
							}
						}
					}

					// STEAM ID + QUERYS
					if (sql_input_option.vac_steamID)
					{
						// QUERY STEAM
						extension_ptr->steamQuery(-1, false, true, temp_str, true);
					}
					if (sql_input_option.beguid)				
					{
						// GENERATE BEGUID
						getBEGUID(temp_str, temp_str);
					}

					// STRING
					if (sql_input_option.string)
					{
						if (temp_str.empty())
						{
							temp_str = "\"\"";
						}
						else
						{
							boost::erase_all(temp_str, "\"");
							boost::erase_all(temp_str, "'");
							temp_str = "\"" + temp_str + "\"";
						}
					}
					else if (sql_input_option.string_escape_quotes)
					{
						if (temp_str.empty())
						{
							temp_str = "\"\"";
						}
						else
						{
							boost::replace_all(temp_str, "\"", "\"\"");
							boost::replace_all(temp_str, "'", "''");
							temp_str = "\"" + temp_str + "\"";
						}
					}

					// BOOL
					else if (sql_input_option.boolean)
					{
						if (boost::algorithm::iequals(temp_str, std::string("True")) == 1)
						{
							temp_str = "1";
						}
						else
						{
							temp_str = "0";
						}
					}

					// SANITIZE CHECK
					if (sql_input_option.check)
					{
						sanitize_str = temp_str;
						if (sql_input_option.check_add_quotes)
						{
							sanitize_str = "\"" + sanitize_str + "\"";
						}
						else if (sql_input_option.check_add_escape_quotes)
						{
							boost::replace_all(temp_str, "\"", "\"\"");
							boost::replace_all(temp_str, "'", "''");
							sanitize_str = "\"" + temp_str + "\"";
						}

						if (!(Sqf::check(sanitize_str)))
						{
							status = false;
							extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Sanitize Check Error: Input: {0}", input_str);
							extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Sanitize Check Error: Value: {0}", sanitize_str);
							result = "[0,\"Error Input Value is not sanitized\"]";
						}
					}
					processed_inputs.push_back(std::move(temp_str));
				}
				all_processed_inputs.push_back(std::move(processed_inputs));
			}


			if (status)
			{
				if (custom_calls_const_itr->second.number_of_custom_inputs == 0)
				{
					callPreparedStatement(input_str, tokens[0], custom_calls_const_itr, all_processed_inputs, status, result);
				}
				else
				{
					callPreparedStatement(input_str, tokens[0], custom_calls_const_itr, all_processed_inputs, custom_inputs, status, result);
				}
				if (status)
				{
					#ifdef DEBUG_TESTING
						extension_ptr->console->info("extDB2: SQL_CUSTOM: Trace: UniqueID: {0} Result: {1}", unique_id, result);
					#endif
					#ifdef DEBUG_LOGGING
						extension_ptr->logger->info("extDB2: SQL_CUSTOM: Trace: UniqueID: {0} Result: {1}", unique_id, result);
					#endif
				}
			}
			if (!status)
			{
				extension_ptr->logger->warn("extDB2: SQL_CUSTOM: Error Exception: UniqueID: {0} SQL: {1}", unique_id, input_str);
			}
		}
	}
	return true;
}
