/*
	File: init.sqf
	Author: Torndeco

	Description:
	Initializes Loop to check for TCPRemote for sqf code to execute
	Code is not tested yet....

*/

_fetch =
{
	private ["_pipe", "_queryResult"];
	_queryResult = "";
	while{true} do
	{
		_pipe = "extDB2" callExtension format["5:%1", _this];
		if(_pipe isEqualTo "") exitWith {};
		_queryResult = _queryResult + _pipe;
	};
	_queryResult
};

_parse =
{
	private ["_clientID", "_code", "_data", "_result"];
	_clientID = "";
	_code = "";
	_code_length = (count _this) - 1;
	for "_index" from 0 to _code_length
	{
		if ((_this select [_index,1]) isEqualTo ":") exitWith
		{
			_clientID = _this select [0, (_index-1)];
			_code = _this select [_index+1, _code_length];
		};
	};

	_data = [_clientID, _code];
	if (_clientID isEqualTo "") then
	{
		_result = _this call _fetch;
		_data = _this call _parse;
	};
	_data
};


private ["_result", "_data", "_code"];
while {true} do
{
	_result = "extDB2" callExtension "6:0";
	if (_result isEqualTo "") then
	{
		uisleep 60;
	}
	else
	{
		_data = _result call _parse;  		// _data = [_clientID, _data]
		_code = compile (_data select 1);
		(_data select 0) spawn _code;  // Pass ClientID to code so it can return results via extension
		uisleep 5;
	};
};
