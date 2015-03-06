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
}

_parse =
{
	private ["_clientID", "_code", "_data", "_result"];
	_clientID = -1;
	_code = "";
	{
		if (_x isEqualTo ":") exitWith
		{
			_clientID = _result select [0, (_forEachIndex - 1)];
			_code = _result select [_forEachIndex +1, (count(_result) -1)]
		}
	} forEach _this;

	_data = [_clientID, _code];
	if (_clientID isEqualTo -1) then
	{
		_result = _this call _fetch;
		_data = _this call _parse;
	};
	_data
}


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
		_code = compile _data select 1;
		(_data select 0) spawn _code;  // Pass ClientID to code so it can return results via extension
		uisleep 5;
	};
};