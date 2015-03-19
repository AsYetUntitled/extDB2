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



#pragma once

#include <vector>

#include <boost/variant.hpp>
#include <Poco/Types.h>


namespace Sqf
{
	typedef boost::make_recursive_variant< double, int, Poco::Int64, bool, std::string, void*, std::vector<boost::recursive_variant_> >::type Value;
	typedef std::vector<Value> Parameters;
	typedef std::string::iterator iter_t;

	bool check(std::string input_str);
}