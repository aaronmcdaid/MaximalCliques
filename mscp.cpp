#include "graph/network.hpp"
#include "graph/loading.hpp"
#include "graph/stats.hpp"
#include "macros.hpp"
#include "cliques.hpp"
#include "cmdline-mscp.h"
#include "comments.hh"

#include <vector>
#include <tr1/unordered_set>
#include <tr1/unordered_map>
#include <map>
#include <set>

#include <algorithm>
#include <tr1/functional>
#include <iostream>
#include <fstream>
#include <sstream>

#include <getopt.h>
#include <libgen.h>
#include <cassert>
#include <cerrno>
#include <ctime>
#include <limits>
#include <sys/stat.h> // for mkdir
#include <sys/types.h> // for mkdir


using namespace std;

typedef vector<int32_t> clique; // the nodes will be in increasing numerical order

static void nonMaxCliques(const graph :: VerySimpleGraphInterface *vsg, const int32_t k);
template<typename T>
string thou(T number);

#define PPt(x) PP(thou(x))
#define ELAPSED (double(clock())/CLOCKS_PER_SEC)
#define HOWLONG "(runtime: " << ELAPSED <<"s)"

string memory_usage() {
	ostringstream mem;
	ifstream proc("/proc/self/status");
	string s;
	while(getline(proc, s), !proc.fail()) {
		if(s.substr(0, 6) == "VmSize") {
			mem << s;
			return mem.str();
		}
	}
	return mem.str();
}


int main(int argc, char **argv) {
	gengetopt_args_info args_info;

	// there shouldn't be any errors in processing args
	if (cmdline_parser (argc, argv, &args_info) != 0)
		exit(1) ;
	// .. and there should be exactly one non-option arg
	if(args_info.inputs_num != 2 || args_info.k_arg < 3) {
		cmdline_parser_print_help();
		exit(1);
	}

	if(args_info.comments_flag)
		cout << commentSlashes;
	const char * edgeListFileName   = args_info.inputs[0];
	const char * output_file_name   = args_info.inputs[1];
	PP3(args_info.k_arg, edgeListFileName, output_file_name);

        std :: auto_ptr<graph :: NetworkInterfaceConvertedToString > network;
	if(args_info.stringIDs_flag) {
		network	= graph :: loading :: make_Network_from_edge_list_string(edgeListFileName, false, false, true);
	} else {
		network	= graph :: loading :: make_Network_from_edge_list_int64(edgeListFileName, false, false, true, 0);
	}

	int32_t maxDegree = graph :: stats :: get_max_degree(network->get_plain_graph());

	cerr << "Network loaded"
	       << " after " << (double(clock()) / CLOCKS_PER_SEC) << " seconds. "
		<< network.get()->numNodes() << " nodes and " << network.get()->numRels() << " edges."
		<< " Max degree is " << maxDegree
	       << endl;

	// finally, call the clique_percolation algorithm proper
	nonMaxCliques(network->get_plain_graph(), args_info.k_arg);
}

template<typename T>
string thou(T number) {
	std::ostringstream ss;
	ss.imbue(std::locale(""));
	ss << number;
	return ss.str();
}


static void readyToTryOneNode(vector<int32_t> &, const vector<int32_t> &cands, const graph :: VerySimpleGraphInterface * vsg, const int32_t k);

static void nonMaxCliques(const graph :: VerySimpleGraphInterface *vsg, const int32_t k) {
	const int32_t N = vsg->numNodes();
	for(int32_t n=0; n<N; n++) {
		/* try cliques around every node, using its *lower-degree* neighbours as candidates.
		 * if same degree, keep higher-id nodes
		 */
		const int32_t degree_of_n = vsg->degree(n);
		if(degree_of_n < k)
			continue;
		vector<int32_t> clique;
		clique.push_back(n);
		vector<int32_t> candidate_nodes;
		const std :: vector<int32_t> &neighs = vsg -> neighbouring_nodes_in_order(n);
		For(neigh, neighs) {
			assert(*neigh != n);
			const int32_t degree_of_neighbour = vsg->degree(*neigh);
			if(degree_of_neighbour < k)
				continue;
			if(degree_of_neighbour > degree_of_n)
				continue;
			if(degree_of_neighbour == degree_of_n && *neigh < n)
				continue;
			candidate_nodes.push_back(*neigh);
		}
		if(1 + candidate_nodes.size() < (size_t)k)
			continue;
		readyToTryOneNode(clique, candidate_nodes, vsg, k);
	}
}

static void find_cliques(vector<int32_t> &clique
		, vector<int32_t> cands
		, const graph :: VerySimpleGraphInterface * vsg
		, const int32_t k);

static void move_node_in( vector<int32_t> &clique
		, const vector<int32_t> &cands
		, const graph :: VerySimpleGraphInterface * vsg
		, const int32_t k
		, const int32_t node_to_move_in
		) {
	clique.push_back(node_to_move_in);
	// delete nodes from cands unless they are connected to node_to_move_in
	const std :: vector<int32_t> &neighs = vsg -> neighbouring_nodes_in_order(node_to_move_in);
	vector<int32_t> new_cands;
	set_intersection (cands.begin(), cands.end()
			, neighs.begin(), neighs.end()
			, back_inserter(new_cands)
			);
	find_cliques(clique, new_cands, vsg, k);
	clique.pop_back();
}


static void find_cliques(vector<int32_t> &clique
		, vector<int32_t> cands
		, const graph :: VerySimpleGraphInterface * vsg
		, const int32_t k) {
	if(clique.size() == (size_t)k) {
		For(clique_node, clique) {
			cerr << *clique_node << " ";
		}
		cerr << endl;
		return;
	}
	assert(clique.size() < (size_t)k);
	if(clique.size() + cands.size() < (size_t)k) // too few candidates left
		return;

		// for each cand, count how many connecconnec

	while(! cands.empty()) {
		const int32_t arbitrary_next_node_to_eject = cands.back();

		/* this is the main bit of the algorithm,
		 * we must try find_cliques twice: once with, and once without, the arbitrary node
		 */
		move_node_in(clique, cands, vsg, k, arbitrary_next_node_to_eject);

		cands.pop_back(); // remove arbitrary_next_node_to_eject from cands
	}
}

static void readyToTryOneNode(vector<int32_t> &clique, const vector<int32_t> &cands, const graph :: VerySimpleGraphInterface * vsg, const int32_t k) {
	assert(clique.size()==1);
	find_cliques(clique, cands, vsg, k);
}
