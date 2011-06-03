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
	int32_t components :: my_component_id(const int32_t n) const {
		assert(n>=0 && n<N);
		return this -> my_component.at(n);
	}
	const misc :: list_with_constant_size <int32_t> & components :: get_members(int32_t component_id) const {
		assert(component_id >= 0 && component_id < this->N);
		return this->members.at(component_id);
	}
	void components :: move_node(const int32_t node_id, const int32_t new_component_id) {
		assert(new_component_id >= 0 && new_component_id < this->N);
		const int32_t old_component_id = this->my_component_id(node_id);
		assert(old_component_id != new_component_id);

		this->my_component.at(node_id) = new_component_id;
		const std :: list<int32_t> :: iterator it = this->my_iterator.at(node_id);
		assert( *it == node_id );
		{
			const int32_t old_size = this->members.at(old_component_id).size();
			this->members.at(old_component_id).erase(it);
			const int32_t new_size = this->members.at(old_component_id).size();
			assert(new_size == old_size-1);
			if(new_size == 0)
				this->empty_components.push(old_component_id);
		}
		{
			const int32_t old_size = this->members.at(new_component_id).size();
			if(old_size == 0) {
				// this *was* empty. We must remove it from the empty_components stack,
				// therefore we insist that it is on the top of that stack
				assert(!this->empty_components.empty() && new_component_id == this->empty_components.top());
				this->empty_components.pop();
			}
			this->members.at(new_component_id).push_front(node_id);
			const std :: list<int32_t> :: iterator it = this->members.at(new_component_id).begin();
			assert( *it == node_id );
			this->my_iterator.at(node_id) = it;
			const int32_t new_size = this->members.at(new_component_id).size();
			assert(new_size == old_size+1);
		}
	}
	int32_t components :: top_empty_component() const {
		assert(!this->empty_components.empty());
		this->empty_components.top();
		return this->empty_components.top();
	}
} // namespace clustering
