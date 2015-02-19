//http://stackoverflow.com/questions/2620218/fastest-container-or-algorithm-for-unique-reusable-ids-in-c


#pragma once

#include <boost/numeric/interval.hpp>
#include <limits>
#include <set>
#include <string>


class id_interval 
{
	public:
		id_interval(int ll, int uu) : value_(ll,uu)  {}
		bool operator < (const id_interval&) const;
		int left() const { return value_.lower(); }
		int right() const {  return value_.upper(); }

	private:
		boost::numeric::interval<int> value_;
};

class IdManager 
{
	public:
		IdManager();
		int AllocateId();          // Allocates an id
		void FreeId(int id);       // Frees an id so it can be used again

	private: 
		typedef std::set<id_interval> id_intervals_t;
		id_intervals_t free_;
};