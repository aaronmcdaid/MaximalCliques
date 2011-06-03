#ifndef _CLUSTERING_COMPONENTS_
#define _CLUSTERING_COMPONENTS_

#include "../misc/list_with_constant_size.hpp"

#include <vector>
#include <stack>
#include <cstdlib>
#include <cassert>

namespace clustering {

class components {
	int32_t N;
	std :: vector< misc :: list_with_constant_size <int32_t> > members; // this will include a lot of empty lists
	std :: vector< misc :: list_with_constant_size <int32_t> :: iterator > my_iterator; // for each node, the iterator to its position in all_members.
	std :: vector< int32_t                           > my_component; // for each node, the id of the cluster it is in.
	std :: stack< int32_t, std :: vector< int32_t > > empty_components;
public:
	components() : N(0) {}
	void setN(const int32_t N);
	void merge_components(const int32_t cl1, const int32_t cl2); // merge two non-empty components
	int32_t my_component_id(const int32_t n) const;
	const misc :: list_with_constant_size <int32_t> & get_members(int32_t component_id) const;
	void move_node(const int32_t node_id, const int32_t new_component_id);
	int32_t top_empty_component() const;
};
	
} // namespace clustering
#endif

