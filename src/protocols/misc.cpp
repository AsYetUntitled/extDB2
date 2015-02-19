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
 
getGUID --
Code to Convert SteamID -> BEGUID 
From Frank https://gist.github.com/Fank/11127158

*/

#include <boost/crc.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/thread/thread.hpp>

#include <Poco/DateTime.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DigestEngine.h>
#include <Poco/NumberFormatter.h>
#include <Poco/NumberParser.h>
#include <Poco/MD4Engine.h>
#include <Poco/MD5Engine.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Timespan.h>

#include <cstdlib>

#include "abstract_ext.h"
#include "misc.h"


bool MISC::init(AbstractExt *extension, const std::string &database_id, const std::string init_str)
{
	extension_ptr = extension;
	return true;
}


void MISC::getDateTime(std::string &result)
{
	Poco::DateTime now;
	result = "[1,[" + Poco::DateTimeFormatter::format(now, "%Y, %n, %d, %H, %M") + "]]";
}


void MISC::getDateTime(int hours, std::string &result)
{
	Poco::DateTime now;
	Poco::Timespan span(hours*Poco::Timespan::HOURS);
	Poco::DateTime newtime = now + span;

	result = "[1,[" + Poco::DateTimeFormatter::format(newtime, "%Y, %n, %d, %H, %M") + "]]";
}


void MISC::getCrc32(std::string &input_str, std::string &result)
{
	boost::lock_guard<boost::mutex> lock(mutex_crc32);
	crc32.reset();
	crc32.process_bytes(input_str.data(), input_str.length());
	result = "[1,\"" + Poco::NumberFormatter::format(crc32.checksum()) + "\"]";
}


void MISC::getMD4(std::string &input_str, std::string &result)
{
	boost::lock_guard<boost::mutex> lock(mutex_md4);
	md4.update(input_str);
	result = "[1,\"" + Poco::DigestEngine::digestToHex(md4.digest()) + "\"]";
}


void MISC::getMD5(std::string &input_str, std::string &result)
{
	boost::lock_guard<boost::mutex> lock(mutex_md5);
	md5.update(input_str);
	result = "[1,\"" + Poco::DigestEngine::digestToHex(md5.digest()) + "\"]";
}


void MISC::getBEGUID(std::string &input_str, std::string &result)
// From Frank https://gist.github.com/Fank/11127158
// Modified to use libpoco
{
	bool status = true;

	if (input_str.empty())
	{
		status = false;
		result = "[0,\"Invalid SteamID\"";
	}
	else
	{
		for (unsigned int index=0; index < input_str.length(); index++)
		{
			if (!std::isdigit(input_str[index]))
			{
				status = false;
				result = "[0,\"Invalid SteamID\"";
				break;
			}
		}
	}
	
	if (status)
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
		result = "[1,\"" + Poco::DigestEngine::digestToHex(md5.digest()) + "\"]";
	}
}


void MISC::getRandomString(std::string &input_str, bool uniqueString, std::string &result)
{
	Poco::StringTokenizer tokens(input_str, ":");
	if (tokens.count() != 2)
	{
		result = "[0,\"Error Syntax\"]";
	}
	else
	{
		int numberOfVariables;
		int stringLength;
		if (!((Poco::NumberParser::tryParse(tokens[0], numberOfVariables)) && (Poco::NumberParser::tryParse(tokens[1], stringLength))))
		{
			result = "[0,\"Error Invalid Number\"]";
		}
		else
		{
			if (numberOfVariables <= 0)
			{
				result = "[0,\"Error Number of Variable <= 0\"]";
			}
			else
			{
				boost::lock_guard<boost::mutex> lock(mutex_RandomString);
				std::string chars(
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					//"1234567890"  Arma Variable Names cant start with a number
					);

				boost::random::random_device rng;
				boost::random::uniform_int_distribution<> index_dist(0, chars.size() - 1);

				result = "[1,[";

				int numOfRetrys = 0;

				for(int i = 0; i < numberOfVariables; ++i)
				{
					std::stringstream randomStringStream;
					for(int x = 0; x < stringLength; ++x)
					{
						randomStringStream << chars[index_dist(rng)];
					}

					std::string randomString = randomStringStream.str();

					if (uniqueString)
					{
						if (std::find(uniqueRandomVarNames.begin(), uniqueRandomVarNames.end(), randomString)!=uniqueRandomVarNames.end())
						{
							numberOfVariables = numberOfVariables - 1;
							numOfRetrys = numOfRetrys + 1;
							if (numOfRetrys > 11)
							{
								if (numberOfVariables == 0)
								{
									result = "[1,[";
								}
								// Break outof Loop if already tried 10 times
								--i;
								break;
							}
						}
						else
						{
							if (i != 0)
							{
								result = result + "," + "\"" + randomString + "\"";
							}
							else
							{
								result = result + "\"" + randomString + "\"";
							}
							uniqueRandomVarNames.push_back(randomString);
							numOfRetrys = 0;
						}
					}
				}
				result =+ "]]";
			}
		}
	}
}


bool MISC::callProtocol(std::string input_str, std::string &result, const int unique_id)
{
	// Protocol
	std::string command;
	std::string data;

	const std::string::size_type found = input_str.find(":");

	if (found==std::string::npos)  // Check Invalid Format
	{
		command = input_str;
	}
	else
	{
		command = input_str.substr(0,found);
		data = input_str.substr(found+1);
	}
	if (command == "TIME")
	{
		if (data.empty())
		{
			getDateTime(result);
		}
		else
		{
			getDateTime(Poco::NumberParser::parse(data), result); //TODO try catch or insert number checker function
		}
	}
	else if (command == "BEGUID")
	{
		getBEGUID(data, result);
	}
	else if (command == "CRC32")
	{
		getCrc32(data, result);
	}
	else if (command == "MD4")
	{
		getMD4(data, result);
	}
	else if (command == "MD5")
	{
		getMD5(data, result);
	}
	else if (command == "RANDOM_UNIQUE_STRING")
	{
		getRandomString(data, true, result);
	}
	else if (command == "RANDOM_STRING")
	{
		getRandomString(data, false, result);
	}
	else if (command == "TEST")
	{
		result = data;
	}
	else
	{
		result = "[0,\"Error Invalid Format\"]";
		extension_ptr->logger->error("extDB: Misc Invalid Command: {0}", command);
	}
	return true;
}