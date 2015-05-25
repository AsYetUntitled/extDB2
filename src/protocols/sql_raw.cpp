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


#include "sql_raw.h"

#include <boost/algorithm/string.hpp>

#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Session.h>

#include <Poco/Data/MySQL/Connector.h>
#include <Poco/Data/MySQL/MySQLException.h>
#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Data/SQLite/SQLiteException.h>

#include <Poco/Exception.h>


bool SQL_RAW::init(AbstractExt *extension, const std::string &database_id, const std::string &init_str)
{
	extension_ptr = extension;
	if (extension_ptr->ext_connectors_info.databases.count(database_id) == 0)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB2: SQL_RAW: No Database Connection ID: {0}", database_id);
		#endif
		extension_ptr->logger->warn("extDB2: SQL_RAW: No Database Connection ID: {0}", database_id);
		return false;
	}

	database_ptr = &extension_ptr->ext_connectors_info.databases[database_id];

	bool status;
	if (database_ptr->type == "MySQL")
	{
		status = true;
	}
	else if (database_ptr->type == "SQLite")
	{
		status = true;
	}
	else
	{
		// DATABASE NOT SETUP YET
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB2: SQL_RAW: No Database Connection");
		#endif
		extension_ptr->logger->warn("extDB2: SQL_RAW: No Database Connection");
		status = false;
	}

	if (status)
	{
		if (init_str.empty())
		{
			stringDataTypeCheck = false;
			#ifdef DEBUG_TESTING
				extension_ptr->console->info("extDB2: SQL_RAW: Initialized: ADD_QUOTES False");
			#endif
			extension_ptr->logger->info("extDB2: SQL_RAW: Initialized: ADD_QUOTES False");
		}
		else if (boost::algorithm::iequals(init_str, std::string("ADD_QUOTES")))
		{
			stringDataTypeCheck = true;
			#ifdef DEBUG_TESTING
				extension_ptr->console->info("extDB2: SQL_RAW: Initialized: ADD_QUOTES True");
			#endif
			extension_ptr->logger->info("extDB2: SQL_RAW: Initialized: ADD_QUOTES True");
		}
		else
		{
			status = false;
		}
	}
	return status;
}


bool SQL_RAW::callProtocol(std::string input_str, std::string &result, const bool async_method, const unsigned int unique_id)
{
	try
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->info("extDB2: SQL_RAW: Trace: Input: {0}", input_str);
		#endif
		#ifdef DEBUG_LOGGING
			extension_ptr->logger->info("extDB2: SQL_RAW: Trace: Input: {0}", input_str);
		#endif

		Poco::Data::Session session = extension_ptr->getDBSession_mutexlock(*database_ptr);
		Poco::Data::RecordSet rs(session, input_str);

		result = "[1,[";
		std::string temp_str;
		temp_str.reserve(result.capacity());

		std::size_t cols = rs.columnCount();
		if (cols >= 1)
		{
			result += "[";
			bool more = rs.moveFirst();
			while (more)
			{
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

					auto datatype = rs.columnType(col);
					if ((datatype == Poco::Data::MetaColumn::FDT_DATE) || (datatype == Poco::Data::MetaColumn::FDT_TIME) || (datatype == Poco::Data::MetaColumn::FDT_TIMESTAMP))
					{
						if (temp_str.empty())
						{
							result += "\"\"";
						}
						else
						{
							boost::erase_all(temp_str, "\"");
							result += "\"" + temp_str + "\"";
						}
					}
					else if ((stringDataTypeCheck) && (rs.columnType(col) == Poco::Data::MetaColumn::FDT_STRING))
					{
						if (temp_str.empty())
						{
							result += ("\"\"");
						}
						else
						{
							boost::erase_all(temp_str, "\"");
							result += "\"" + temp_str + "\"";
						}
					}
					else
					{
						if (temp_str.empty())
						{
							result += "\"\"";
						}
						else
						{
							result += temp_str;
						}
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
		result += "]]";
		#ifdef DEBUG_TESTING
			extension_ptr->console->info("extDB2: SQL_RAW: Trace: Result: {0}", result);
		#endif
		#ifdef DEBUG_LOGGING
			extension_ptr->logger->info("extDB2: SQL_RAW: Trace: Result: {0}", result);
		#endif
	}
	catch (Poco::InvalidAccessException& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_RAW: Error InvalidAccessException: {0}", e.displayText());
			extension_ptr->console->error("extDB2: SQL_RAW: Error InvalidAccessException: SQL: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB2: SQL_RAW: Error InvalidAccessException: {0}", e.displayText());
		extension_ptr->logger->error("extDB2: SQL_RAW: Error InvalidAccessException: SQL: {0}", input_str);
		result = "[0,\"Error DBLocked Exception\"]";
	}
	catch (Poco::Data::NotConnectedException& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_RAW: Error NotConnectedException: {0}", e.displayText());
			extension_ptr->console->error("extDB2: SQL_RAW: Error NotConnectedException: SQL: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB2: SQL_RAW: Error NotConnectedException: {0}", e.displayText());
		extension_ptr->logger->error("extDB2: SQL_RAW: Error NotConnectedException: SQL: {0}", input_str);
		result = "[0,\"Error DBLocked Exception\"]";
	}
	catch (Poco::NotImplementedException& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_RAW: Error NotImplementedException: {0}", e.displayText());
			extension_ptr->console->error("extDB2: SQL_RAW: Error NotImplementedException: SQL: {0}", input_str);

		#endif
		extension_ptr->logger->error("extDB2: SQL_RAW: Error NotImplementedException: {0}", e.displayText());
		extension_ptr->logger->error("extDB2: SQL_RAW: Error NotImplementedException: SQL: {0}", input_str);
		result = "[0,\"Error DBLocked Exception\"]";
	}
	catch (Poco::Data::SQLite::DBLockedException& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_RAW: Error DBLockedException: {0}", e.displayText());
			extension_ptr->logger->error("extDB2: SQL_RAW: Error DBLockedException: SQL: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB2: SQL_RAW: Error DBLockedException: {0}", e.displayText());
		extension_ptr->logger->error("extDB2: SQL_RAW: Error DBLockedException: SQL: {0}", input_str);
		result = "[0,\"Error DBLocked Exception\"]";
	}
	catch (Poco::Data::MySQL::ConnectionException& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_RAW: Error ConnectionException: {0}", e.displayText());
			extension_ptr->logger->error("extDB2: SQL_RAW: Error ConnectionException: SQL: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB2: SQL_RAW: Error ConnectionException: {0}", e.displayText());
		extension_ptr->logger->error("extDB2: SQL_RAW: Error ConnectionException: SQL: {0}", input_str);
		result = "[0,\"Error Connection Exception\"]";
	}
	catch(Poco::Data::MySQL::StatementException& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_RAW: Error StatementException: {0}", e.displayText());
			extension_ptr->logger->error("extDB2: SQL_RAW: Error StatementException: SQL: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB2: SQL_RAW: Error StatementException: {0}", e.displayText());
		extension_ptr->logger->error("extDB2: SQL_RAW: Error StatementException: SQL: {0}", input_str);
		result = "[0,\"Error Statement Exception\"]";
	}
	catch (Poco::Data::ConnectionFailedException& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_RAW: Error ConnectionFailedException: {0}", e.displayText());
			extension_ptr->console->error("extDB2: SQL_RAW: Error ConnectionFailedException: SQL {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB2: SQL_RAW: Error ConnectionFailedException: {0}", e.displayText());
		extension_ptr->logger->error("extDB2: SQL_RAW: Error ConnectionFailedException: SQL {0}", input_str);
		result = "[0,\"Error ConnectionFailedException\"]";
	}
	catch (Poco::Data::DataException& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_RAW: Error DataException: {0}", e.displayText());
			extension_ptr->logger->error("extDB2: SQL_RAW: Error DataException: SQL: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB2: SQL_RAW: Error DataException: {0}", e.displayText());
		extension_ptr->logger->error("extDB2: SQL_RAW: Error DataException: SQL: {0}", input_str);
		result = "[0,\"Error Data Exception\"]";
	}
	catch (Poco::Exception& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB2: SQL_RAW: Error Exception: {0}", e.displayText());
			extension_ptr->console->error("extDB2: SQL_RAW: Error Exception: SQL: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB2: SQL_RAW: Error Exception: {0}", e.displayText());
		extension_ptr->logger->error("extDB2: SQL_RAW: Error Exception: SQL: {0}", input_str);
		result = "[0,\"Error Exception\"]";
	}
	return true;
}