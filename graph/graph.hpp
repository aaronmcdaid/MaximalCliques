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
	virtual const std :: pair<int, int> & EndPoints(int relId) const = 0;
	virtual ~ VerySimpleGraphInterface() {}
	virtual const std :: vector<int32_t> & neighbouring_rels_in_order(const int32_t node_id) const = 0;
	int degree(const int32_t node_id) const { return this->neighbouring_rels_in_order(node_id).size(); }
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
