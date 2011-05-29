#ifndef _GRAPH_NETWORK_
#define _GRAPH_NETWORK_

#include "graph.hpp"
#include "weights.hpp"
#include <string>
#include <memory>
#include <vector>
#include <cstdlib>
namespace graph {

template <class NodeNameT>
struct NetworkInterface;
struct NetworkInterfaceConvertedToString ; // Any NodeNameT (int or string) should be able to implement this.
struct NetworkInterfaceConvertedToStringWithWeights ;

struct NodeNameIsInt32; // the default node_name type. It's nice to have the names sorted by this, so that "10" comes after "2"
struct NodeNameIsString; // .. but if the user wants to specify arbitrary strings in their edge list, they can do so explicitly.
struct BadlyFormattedNodeName : public std :: exception { // if the text doesn't define an int properly when trying to use NodeNameIsInt32
};

typedef NetworkInterface<NodeNameIsInt32> NetworkInt32;
typedef NetworkInterface<NodeNameIsString> NetworkString;

struct NodeNameIsInt32 {
	typedef int32_t value_type;
	inline static value_type fromString(const std :: string &s) {
		assert(!s.empty());
		value_type i;
		char *end_ptr;
		i = int32_t(strtol(s.c_str(), &end_ptr, 10));
		if(*end_ptr == '\0') // success, according to http://linux.die.net/man/3/strtol
			return i;
		else
			throw BadlyFormattedNodeName();
	}
};
struct NodeNameIsString {
	typedef std :: string value_type;
	inline static value_type fromString(const std :: string &s) {
		assert(!s.empty());
		return s;
	}
};

struct NetworkInterfaceConvertedToString  { // Any NodeNameT (int or string) should be able to implement this.
	virtual std :: string node_name_as_string(int32_t node_id) const = 0;
	virtual const graph :: VerySimpleGraphInterface * get_plain_graph() const = 0;
	virtual int32_t numNodes() const { return this->get_plain_graph()->numNodes(); } // make these pure, and hide the members from this interface
	virtual int32_t numRels()  const { return this->get_plain_graph()->numRels(); }
	virtual int32_t degree(const int32_t node_id) const { return this->get_plain_graph()->degree(node_id); }

	virtual ~NetworkInterfaceConvertedToString();
};
struct NetworkInterfaceConvertedToStringWithWeights : public NetworkInterfaceConvertedToString {
	virtual const graph :: weights :: EdgeDetailsInterface * get_edge_weights() const = 0;
};

template <class NodeNameT>
struct NetworkInterface : public NetworkInterfaceConvertedToStringWithWeights {
	/* A NetworkInterface is a VerySimpleGraphInterface with some extra attributes.
	 * The nodes will have string names, and the edges might have directionality and weights
	 */
	std :: auto_ptr<const graph :: VerySimpleGraphInterface> plain_graph;
	std :: auto_ptr<graph :: weights :: EdgeDetailsInterface> edge_weights;
public: // I should make the above private some time!
	virtual ~ NetworkInterface() throw(); // this forces derivations to declare a destructor, I think.
	NetworkInterface(const bool directed, const bool weighted);
	// virtual NodeNameT name_of_node(const int32_t) const = 0;
	virtual const graph :: VerySimpleGraphInterface * get_plain_graph() const {
		assert(this->plain_graph.get());
		return this->plain_graph.get();
	}
	virtual const graph :: weights :: EdgeDetailsInterface * get_edge_weights() const {
		assert(this->edge_weights.get());
		return this->edge_weights.get();
	}
};

} // namespace graph

#endif
