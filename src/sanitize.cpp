/*
Copyright (C) 2014 Declan Ireland <http://github.com/torndeco/extDB>
Copyright (C) 2009-2012 Rajko Stojadinovic <http://github.com/rajkosto/hive>


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

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>

#include <string>
#include <vector>

#include "sanitize.h"

namespace 
{
	template <typename Iterator, typename Skipper>
		struct SqfValueParser : boost::spirit::qi::grammar<Iterator, Sqf::Value(), Skipper>
	{
		SqfValueParser() : SqfValueParser::base_type(start,"Sqf::Value")
		{
			quoted_string = boost::spirit::qi::lexeme['"' >> *(boost::spirit::ascii::char_ - '"') >> '"'] | boost::spirit::qi::lexeme["'" >> *(boost::spirit::ascii::char_ - "'") >> "'"];
			quoted_string.name("quoted_string");

			start = strict_double |
				(boost::spirit::qi::int_ >> !boost::spirit::qi::digit) |
				boost::spirit::qi::long_long |
				boost::spirit::qi::bool_ |
				quoted_string |
				(boost::spirit::qi::lit("any") >> boost::spirit::qi::attr(static_cast<void*>(nullptr))) |
				(boost::spirit::qi::lit("[") >> -(start % ",") >> boost::spirit::qi::lit("]"));
		}

		boost::spirit::qi::rule<Iterator, std::string()> quoted_string;
		boost::spirit::qi::real_parser< double, boost::spirit::qi::strict_real_policies<double> > strict_double;
		boost::spirit::qi::rule<Iterator, Sqf::Value(), Skipper> start;
	};

	template <typename Iterator, typename Skipper>
	struct SqfParametersParser : boost::spirit::qi::grammar<Iterator, Sqf::Parameters(), Skipper>
	{
		
		SqfValueParser<Iterator,Skipper> val_parser;
		boost::spirit::qi::rule<Iterator,  Sqf::Value(), Skipper> one_value;
		boost::spirit::qi::rule<Iterator, Sqf::Parameters(), Skipper> start;
		
		SqfParametersParser() : SqfParametersParser::base_type(start,"Sqf::Parameters")
		{
			val_parser.name("one_value");
			start = *(val_parser);
		}
	};
}

namespace Sqf
{
	bool check(std::string input_str)
	{
		std::string::iterator first = input_str.begin();
		std::string::iterator last = input_str.end();

		bool r = boost::spirit::qi::phrase_parse(
			first,
			last,
			SqfParametersParser<iter_t,boost::spirit::qi::space_type>(),
			boost::spirit::qi::space_type()
		);
		if (first != last) // fail if we did not get a full match
		{
			return false;
		}
		else
		{
			return r;
		}
	};
}


#ifdef TEST_SANITIZE_APP
int main(int nNumberofArgs, char* pszArgs[])
{
	std::string input_str;
	for (;;) {
		std::getline(std::cin, input_str);
		if (input_str == "quit")
		{
			break;
		}
		else
		{
			if (Sqf::check(input_str))
			{
				std::cout << "extDB: True: " << input_str << std::endl;
			}
			else
			{
				std::cout << "extDB: False: " << input_str << std::endl;
			}
		}
	}
	return 0;
}
#endif
