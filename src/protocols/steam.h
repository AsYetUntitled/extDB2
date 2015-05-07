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

#include "abstract_protocol.h"


class STEAM: public AbstractProtocol
{
	public:
		bool init(AbstractExt *extension, const std::string &database_id, const std::string init_str);
		bool callProtocol(std::string input_str, std::string &result, const bool async_method, const unsigned int unique_id=1);

	private:
		bool isNumber(const std::string &input_str);
};