#pragma once

/*
A better implementation could explicitly check for 0 and -1
that havespecial meaning in extdb.

and then make sure the id was not still in use... but 2**31 is a big number.
*/

class IdManager
{
	public:
		IdManager();
		int AllocateId() {
			return current++;
		};
		void FreeId(int) {};
	private: 
		long current = 9816;
};

/*
The previous version, based on
http://stackoverflow.com/questions/2620218/fastest-container-or-algorithm-for-unique-reusable-ids-in-c
is appropriate for cases where those additional requirements are present.

Also, I'm setting the start explicitly rather than ask an unseeded PRNG.
The exact number is different.
*/
