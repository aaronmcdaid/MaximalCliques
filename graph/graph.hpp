#ifndef _GRAPH_HPP_
#define _GRAPH_HPP_

#include <utility>
#include <vector>
#include <cassert>
#include <stdint.h>
#include <iterator>

namespace graph {

class VerySimpleGraphInterface;

class VerySimpleGraphInterface { // consecutive ints. No attributes, or directions, or anything
public:
	virtual int numNodes() const = 0;
	virtual int numRels() const = 0;
	virtual const std :: pair<int32_t, int32_t> & EndPoints(int relId) const = 0;
	virtual ~ VerySimpleGraphInterface() {}
	virtual const std :: vector<int32_t> & neighbouring_rels_in_order(const int32_t node_id) const = 0;
	int degree(const int32_t node_id) const { return this->neighbouring_rels_in_order(node_id).size(); }
	bool are_connected(int32_t node_id1, int32_t node_id2) const { // TODO: could be faster, making use of the fact that various structures are sorted (binary_search)
		if(this->degree(node_id2) < this->degree(node_id1))
			std :: swap(node_id2, node_id1);
		// node 1 is the low-degree node. Check through it's neighbouring rels
		const std :: vector<int32_t> & neigh_rels = this->neighbouring_rels_in_order(node_id1); // TODO
		for(size_t x = 0; x < neigh_rels.size(); x++) {
			const int32_t relId = neigh_rels.at(x);
			const std :: pair <int32_t, int32_t> & eps = this->EndPoints(relId);
			if(eps.first == node_id1) {
				if(eps.second == node_id2)
					return true;
			} else {
				assert(eps.second == node_id1);
				if(eps.first == node_id2)
					return true;
			}
		}
		return false;
	}
};

class neighbouring_rel_id_iterator : public std :: iterator<std::input_iterator_tag, const int32_t> { // given a node_id, we can iterate over its neighbours and get the node_id of them
private:
	std :: vector<int32_t> :: const_iterator i; // This will point at the current relationship
	std :: vector<int32_t> :: const_iterator i_end; // the final relationship of this node
public:
	neighbouring_rel_id_iterator(const VerySimpleGraphInterface * vsg, const int32_t node_id) {
		assert(vsg);
		this -> i = vsg->neighbouring_rels_in_order(node_id).begin();
		this -> i_end = vsg->neighbouring_rels_in_order(node_id).end();
	}
	neighbouring_rel_id_iterator & operator++() {
		this->i ++;
		return *this;
	}
	neighbouring_rel_id_iterator   operator++(int) { // postfix. Should return the old iterator, but I'm going to return void :-)
		neighbouring_rel_id_iterator old_value(*this);
		this -> operator++();
		return old_value;
	}
	bool at_end() const {
		return i == i_end;
	}
	value_type operator*() {
		return * this->i;
	}
};

} // namespace graph
#endif
