#include"loading.hpp"

#include"saving.hpp"
#include"../pp.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <limits>
#include <algorithm>

using namespace std;

namespace graph {
namespace loading {

template <class NodeNameT>
struct ModifiableNetwork;
template <class NodeNameT>
static void read_edge_list_from_file(ModifiableNetwork<NodeNameT> *network, const string file_name);
template
static void read_edge_list_from_file(ModifiableNetwork<NodeNameIsInt32> *network, const string file_name);
template
static void read_edge_list_from_file(ModifiableNetwork<NodeNameIsString> *network, const string file_name);
typedef pair< pair<string, string> , string> ThreeStrings;
static ThreeStrings parseLine(const string &lineOrig);
class MyVSG;

/* try to put all the  forward-declares above this line */

struct MyVSG : public VerySimpleGraphInterface {
	int N, R;
	vector< pair<int32_t,int32_t> > ordered_relationships; // the relationships, to be ordered by the node_ids inside them. .first <= .second
	vector< vector<int32_t> > node_to_relationships_map; // for each node, the *relationships* it is in. In order
	int numNodes() const { return this->N; }
	int numRels() const { return this->R; }
	virtual const std :: pair<int32_t, int32_t> & EndPoints(int32_t rel_id) const { assert(rel_id >= 0 && rel_id < this->R); return this->ordered_relationships.at(rel_id); }
	virtual const std :: vector<int32_t> & neighbouring_rels_in_order(const int32_t node_id) const {
		return this->node_to_relationships_map.at(node_id);
	}
};

template <class NodeNameT>
struct ModifiableNetwork : public NetworkInterface<NodeNameT> { // NetworkInterface is the read-only interface we want to expose, but this is the derived class that will do the heavy lifting
	typedef typename NodeNameT :: value_type t;
	vector< t > ordered_node_names;
	ModifiableNetwork(const bool directed, const bool weighted) : NetworkInterface<NodeNameT>(directed, weighted) {
	}
	virtual ~ ModifiableNetwork() throw() {
	}
	int32_t find_ordered_node_names_offset(const t &node_name) {
		// node_name must be in this->ordered_node_names. Now to find *where* it is.
		const int32_t offset = lower_bound(this->ordered_node_names.begin(), this->ordered_node_names.end(), node_name) - this->ordered_node_names.begin();
		assert( this->ordered_node_names.at(offset) == node_name);
		return offset;
	}
	virtual std :: string node_name_as_string(int32_t node_id) const;
};
template<>
std :: string ModifiableNetwork<NodeNameIsString> :: node_name_as_string(int32_t node_id) const {
	return this->ordered_node_names.at(node_id);
}
template<>
std :: string ModifiableNetwork<NodeNameIsInt32> :: node_name_as_string(int32_t node_id) const {
	std :: ostringstream o;
	o << this->ordered_node_names.at(node_id);
	return o.str();
}

template struct ModifiableNetwork<graph :: NodeNameIsInt32>;
template struct ModifiableNetwork<graph :: NodeNameIsString>;

static ThreeStrings parseLine(const string &lineOrig) {
	PP(lineOrig);
	string line(lineOrig); // copy the line. We want to keep the original in order to print error messages
	// line will not have any newlines in it
	// So I'll convert all the delimeters (space, tab, pipe) to newlines
	for(string :: iterator i = line.begin(); i != line.end(); i++) {
		assert(*i != '\n');
		if(*i == ',' || *i == ' ' || *i == '\t' || *i == '|')
			*i = '\n';
	}
	istringstream fields(line);
	string sourceNodeName;
	string targetNodeName;
	string weightAsString;

	getline(fields, sourceNodeName); 
	if( fields.fail() ) throw BadlyFormattedLine( 5, line);
	getline(fields, targetNodeName); 
	if( fields.fail() ) throw BadlyFormattedLine( 5, line);
	getline(fields, weightAsString); 
	// it's OK if this last one fails, the user doesn't have to specify a weight.

	ThreeStrings t;
	t.first.first = sourceNodeName;
	t.first.second = targetNodeName;
	t.second = weightAsString;
	return t;
}

template <class NodeNameT>
static void read_edge_list_from_file(ModifiableNetwork<NodeNameT> *modifiable_network, const string file_name) {
	assert(modifiable_network && modifiable_network->ordered_node_names.empty());
	/*
	 * This will make *three* passes:
	 * - One pass to identify all the node names (strings in the text file) and then to sort them so that consecutive-integer IDs can be assigned to them (respecting the order of the strings)
	 * - A second pass to identify all the unique relationships that exist, then they will be sorted
	 * - Finally, now that the node_names, node_ids and relationship_ids are sorted, read the network into the prepared datastructures, including the weights
	 */
	PP(file_name);
	typedef typename NodeNameT :: value_type t;
	{ // first pass: just store the node names
		ifstream f(file_name.c_str(), ios_base :: in | ios_base :: binary);
		string line;
		set<t> set_of_node_names; // This will store all the node names.
		while( getline(f, line) ) {
			// There might be a '\r' at the end of this line (dammit!)
			if(!line.empty() && *line.rbegin() == '\r') { line.erase( line.length()-1, 1); }
			ThreeStrings t = parseLine(line);
			set_of_node_names.insert( NodeNameT :: fromString(t.first.first) );
			set_of_node_names.insert( NodeNameT :: fromString(t.first.second) );
		}
		for( typename set<t> :: const_iterator i = set_of_node_names.begin() ; i != set_of_node_names.end(); i++) {
			modifiable_network->ordered_node_names.push_back(*i);
		}
		assert(modifiable_network->ordered_node_names.size() == set_of_node_names.size());
	}

	vector< pair<int32_t,int32_t> > tmp_ordered_relationships;
	{ // second pass. Find all the distinct relationships (node_id_1, node_id_2; where node_id_1 <= node_id_2)
		ifstream f(file_name.c_str(), ios_base :: in | ios_base :: binary);
		string line;
		set< pair<int32_t,int32_t> > set_of_relationships; // every edge, ignoring direction, will be included here
		while( getline(f, line) ) {
			if(!line.empty() && *line.rbegin() == '\r') { line.erase( line.length()-1, 1); }
			ThreeStrings t = parseLine(line);
			int32_t one_node_id = modifiable_network->find_ordered_node_names_offset( NodeNameT :: fromString(t.first.first)  );
			int32_t another_node_id = modifiable_network->find_ordered_node_names_offset( NodeNameT :: fromString(t.first.second) );
			if(one_node_id > another_node_id)
				swap(one_node_id, another_node_id);
			set_of_relationships.insert( make_pair(one_node_id, another_node_id) );
			PP2(one_node_id, another_node_id);
		}
		for( typename set< pair<int32_t,int32_t> > :: const_iterator i = set_of_relationships.begin() ; i != set_of_relationships.end(); i++) {
			tmp_ordered_relationships.push_back(*i);
		}
		assert(set_of_relationships.size() == tmp_ordered_relationships.size());
	}

	const int32_t N = modifiable_network->ordered_node_names.size();
	const int32_t R = tmp_ordered_relationships.size();

	vector< vector<int32_t> > tmp_node_to_relationships_map(N);
	{ // before the third phase (reading weights), we able to complete the VSG object, by populating the list of relationships.
		for(int r = 0; r < R; r++) {
			const pair<int32_t, int32_t> rel = tmp_ordered_relationships.at(r);
			tmp_node_to_relationships_map.at(rel.first).push_back(rel.second);
			if(rel.first != rel.second)
				tmp_node_to_relationships_map.at(rel.second).push_back(rel.first);
		}
		// each individual vector should, I think, now be already sorted. I will now check that to be sure!
		for(int n=0; n<N; n++) {
			const vector<int32_t> & one_nodes_rels = tmp_node_to_relationships_map.at(n);
			int x = std :: numeric_limits<int>().min()  ;
			for (size_t i = 0; i < one_nodes_rels.size(); i++) {
				assert(x < one_nodes_rels.at(i));
				x = one_nodes_rels.at(i);
			}
		}

	}

	{ // third pass. Assigning weights to the edges
		assert(modifiable_network->edge_weights.get());
		ifstream f(file_name.c_str(), ios_base :: in | ios_base :: binary);
		string line;
		while( getline(f, line) ) {
			if(!line.empty() && *line.rbegin() == '\r') { line.erase( line.length()-1, 1); }
			PP(line);
			ThreeStrings t = parseLine(line);
			PP3(t.first.first, t.first.second, t.second);
			const int32_t source_node_id = modifiable_network->find_ordered_node_names_offset( NodeNameT :: fromString(t.first.first)  );
			const int32_t target_node_id = modifiable_network->find_ordered_node_names_offset( NodeNameT :: fromString(t.first.second) );
			int32_t one_node_id = source_node_id;
			int32_t another_node_id = target_node_id;
			if(one_node_id > another_node_id)
				swap(one_node_id, another_node_id);
			int32_t relationship_id = lower_bound(tmp_ordered_relationships.begin(), tmp_ordered_relationships.end(), make_pair(one_node_id, another_node_id)) - tmp_ordered_relationships.begin();
			assert( tmp_ordered_relationships.at(relationship_id).first  == one_node_id );
			assert( tmp_ordered_relationships.at(relationship_id).second == another_node_id );
			PP3(relationship_id, source_node_id, target_node_id);
			modifiable_network->edge_weights->new_rel(relationship_id, make_pair(source_node_id, target_node_id), t.second);
		}
	}

	MyVSG * tmp_plain_graph = new MyVSG();
	tmp_plain_graph->ordered_relationships.swap(tmp_ordered_relationships);
	tmp_plain_graph->node_to_relationships_map.swap(tmp_node_to_relationships_map);
	tmp_plain_graph->N = N;
	tmp_plain_graph->R = R;
	modifiable_network->plain_graph.reset(tmp_plain_graph);
	assert(tmp_plain_graph);
	tmp_plain_graph = NULL;
	assert(!tmp_plain_graph);

	PP2(modifiable_network->numNodes(), modifiable_network->numRels());

	graph :: saving :: print_Network_to_screen(modifiable_network);
}

std :: auto_ptr< graph :: NetworkInt32 > make_Network_from_edge_list_int32 (const std :: string file_name, const bool directed, const bool weighted) throw(BadlyFormattedLine) { ModifiableNetwork<NodeNameIsInt32> *network = new ModifiableNetwork<NodeNameIsInt32>(directed, weighted);
	read_edge_list_from_file<NodeNameIsInt32> (network, file_name);
	return auto_ptr<NetworkInt32 >(network);
}
std :: auto_ptr< graph :: NetworkString > make_Network_from_edge_list_string (const std :: string file_name, const bool directed, const bool weighted) throw(BadlyFormattedLine) {
	ModifiableNetwork<NodeNameIsString> *network = new ModifiableNetwork<NodeNameIsString>(directed, weighted);
	read_edge_list_from_file<NodeNameIsString> (network, file_name);
	return auto_ptr<NetworkString >(network);
}

BadlyFormattedLine :: BadlyFormattedLine(int32_t _line_number, std :: string _bad_line) : line_number(_line_number), bad_line(_bad_line) {
}
const char* BadlyFormattedLine :: what() const throw() {
	ostringstream s;
	s << "Error: badly formatted line in edge list at line " << this->line_number << ". ExitinG. \"" << this->bad_line << "\"" << endl;
	return s.str().c_str();
}
BadlyFormattedLine :: ~ BadlyFormattedLine() throw() {
}

} // namespace loading
} // namespace graph
