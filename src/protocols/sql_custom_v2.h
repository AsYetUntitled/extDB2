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


#pragma once

#include <thread>
#include <unordered_map>

#include <Poco/DynamicAny.h>
#include <Poco/MD5Engine.h>
#include <Poco/StringTokenizer.h>

#include "abstract_protocol.h"

#define EXTDB_SQL_CUSTOM_V2_REQUIRED_VERSION 8
#define EXTDB_SQL_CUSTOM_V2_LATEST_VERSION 10


class SQL_CUSTOM_V2: public AbstractProtocol
{
	public:
		bool init(AbstractExt *extension, const std::string &database_id, const std::string init_str);
		bool callProtocol(std::string input_str, std::string &result, const bool async_method, const unsigned int unique_id=1);
		
	private:
		AbstractExt::DBConnectionInfo *database_ptr;
		
		Poco::MD5Engine md5;
		std::mutex mutex_md5;

		struct Value_Options
		{
			int number = -1;

			bool check;
			bool check_add_quotes = false;
			bool check_add_escape_quotes = false;

			bool boolean = false;
			bool beguid = false;

			bool vac_steamID = false;

			bool string = false;
			bool string_escape_quotes = false;

			bool datetime_iso8601 = false;

			bool strip = false;
		};
		
		struct customCall
		{
			bool strip;

			bool preparedStatement_cache;
			bool returnInsertID;

			int number_of_inputs;
			int number_of_custom_inputs;
			
			int strip_chars_action;
			std::string strip_chars;
			std::string strip_custom_input_chars;

			std::vector< std::string> sql_prepared_statements;

			std::vector< std::vector< Value_Options > > sql_inputs_options;
			std::vector< Value_Options > sql_outputs_options;
		};

		typedef std::unordered_map<std::string, customCall> Custom_Call_UnorderedMap;

		Custom_Call_UnorderedMap custom_calls;

		void callPreparedStatement(std::string &input_str, std::string call_name, std::unordered_map<std::string, customCall>::const_iterator custom_protocol_itr, std::vector< std::vector< std::string > > &all_processed_inputs, bool &status, std::string &result);
		void callPreparedStatement(std::string &input_str, std::string call_name, std::unordered_map<std::string, customCall>::const_iterator custom_protocol_itr, std::vector< std::vector< std::string > > &all_processed_inputs, std::vector<std::string> &custom_inputs, bool &status, std::string &result);
		void executeSQL(Poco::Data::Statement &sql_statement, std::string &result, bool &status);

		void getBEGUID(std::string &input_str, std::string &result);
		void getResult(std::string &input_str, std::unordered_map<std::string, customCall>::const_iterator &custom_protocol_itr, Poco::Data::Session &session, Poco::Data::Statement &sql_statement, std::string &result, bool &status);
};