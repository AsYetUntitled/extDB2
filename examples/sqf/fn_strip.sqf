/*
	File: fn_strip.sqf
	Author: Declan Ireland

	Description:
		Strips : from String
			Needed for parser Player Name etc,
			since extDB uses : as seperator character
	Parameters:
		0: ClientID
*/

private["_string","_array"];

_string = (_this select 0);

_array = toArray _string;
{
	if (_x == 58) then
	{
		_array set[_forEachIndex, -1];
	};
} foreach _array;
_array = _array - [-1];
_string = toString _array;
_string