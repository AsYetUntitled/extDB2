//http://stackoverflow.com/questions/2620218/fastest-container-or-algorithm-for-unique-reusable-ids-in-c

//	NOTE Unique ID cant be -1,0 or less, due to extDB code 

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <boost/numeric/interval.hpp>
#include <limits>
#include <set>
#include <string>

#include "uniqueid.h"


IdManager::IdManager()
{
	// Smaller Range of Unique ID 65536 is still loads
	#ifdef TEST_APP
		// Normal ID for Test APP, easier to work with if ID =! random
		free_.insert(id_interval(1, std::numeric_limits<int>::max()));
	#else
		// Randomize Starting Unique ID
		boost::random::mt19937 gen;
		boost::random::uniform_int_distribution<> dist(1, (std::numeric_limits<int>::max()-1024));
		const int seed = dist(gen);
		free_.insert(id_interval(seed, (seed + 1024)));
	#endif
}


int IdManager::AllocateId()
{
	id_interval first = *(free_.begin());
	int free_id = first.left();
	free_.erase(free_.begin());
	if (first.left() + 1 <= first.right()) {
		free_.insert(id_interval(first.left() + 1 , first.right()));
	}
	return free_id;
}


void IdManager::FreeId(int id)
{
	id_intervals_t::iterator it = free_.find(id_interval(id,id));
	if (it != free_.end()  && it->left() <= id && it->right() > id)
	{
		return;
	}

	it = free_.upper_bound(id_interval(id,id));
	if (it == free_.end())
	{
		return;
	}
	else
	{
		id_interval free_interval = *it;

		if (id + 1 != free_interval.left())
		{
			free_.insert(id_interval(id, id));
		}
		else
		{
			if (it != free_.begin())
			{
				id_intervals_t::iterator it_2 = it;
				--it_2;
				if (it_2->right() + 1 == id )
				{
					id_intervals_t::iterator it_3;
					if (it_2 != free_.begin())
					{
						it_3 = it_2;
						--it_3;
					}
					else
					{
						it_3 = free_.begin();
					}
					id_interval free_interval_2 = *it_2;
					free_.erase(it);
					free_.erase(it_2);
					free_.insert(it_3,
						id_interval(free_interval_2.left(), 
									free_interval.right()));
				}
				else
				{
					free_.erase(it);
					++it_2;
					free_.insert(it_2, id_interval(id, free_interval.right()));
				}
			}
			else
			{
				free_.erase(it);
				free_.insert(free_.begin(), id_interval(id, free_interval.right()));
			}
		}
	}
}


bool id_interval::operator < (const id_interval& s) const
{
	return
		(value_.lower() < s.value_.lower()) &&
		(value_.upper() < s.value_.lower());
}