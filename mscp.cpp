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


static void readyToTryOneNode(vector<int32_t> &clique, const set<int32_t> &cands, const graph :: VerySimpleGraphInterface * vsg);

static void nonMaxCliques(const graph :: VerySimpleGraphInterface *vsg, const int32_t k) {
	const int32_t N = vsg->numNodes();
	PP2(N, k);
	for(int32_t n=0; n<N; n++) {
		// try cliques around every node, using its *lower-degree* neighbours as candidates
		const int32_t degree_of_n = vsg->degree(n);
		if(degree_of_n < k)
			continue;
		vector<int32_t> clique;
		clique.push_back(n);
		set<int32_t> candidate_nodes;
		const std :: vector<int32_t> &neighs = vsg -> neighbouring_nodes_in_order(n);
		For(neigh, neighs) {
			const int32_t degree_of_neighbour = vsg->degree(*neigh);
			if(degree_of_neighbour < k)
				continue;
			candidate_nodes.insert(*neigh);
		}
		if(1 + candidate_nodes.size() < (size_t)k)
			continue;
		PP3(n, degree_of_n, candidate_nodes.size());
		readyToTryOneNode(clique, candidate_nodes, vsg);
	}
}

static void readyToTryOneNode(vector<int32_t> &, const set<int32_t> &cands, const graph :: VerySimpleGraphInterface * vsg) {
	/* currently all the cands values are zero. We gotta populate them correctly
	 * before getting into the clique-finding proper
	 */
	std :: tr1 :: unordered_map<int32_t, int32_t> cand_internal_degrees;
	For(cand, cands) {
		// for each candidate, how many other candidates does it connect to ?
		const std :: vector<int32_t> &neighs_of_this_cand = vsg -> neighbouring_nodes_in_order(*cand);
		vector<int32_t> neighs_in_cand;
		set_intersection ( neighs_of_this_cand.begin(), neighs_of_this_cand.end()
				, cands.begin(), cands.end()
				, back_inserter(neighs_in_cand)
				);
		cand_internal_degrees[*cand, neighs_in_cand.size()];
		PP2(*cand, neighs_in_cand.size());
	}
	assert(cand_internal_degrees.size() == cands.size());
}
