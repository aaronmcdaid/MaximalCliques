#include "components.hpp"
#include <cassert>

using namespace std;

namespace clustering {
	void components :: setN(const int32_t _N) {
		assert (this->N == 0); // setN should only be called *once*
		assert (_N > 0);
		this -> N = _N;
		this -> members.resize(this->N);
		this -> my_iterator.resize(this->N);
		this -> my_component.resize(this->N);
		for(int i=0; i<N; i++) {
			this->my_component.at(i) = i; // every node in a component of its own
			this->members.at(i).push_front(i);
			assert(this->members.at(i).size()==1);
			list <int32_t> :: iterator it = this->members.at(i).begin();
			assert(*it == i);
			this->my_iterator.at(i) = it;
		}
	}
} // namespace clustering
