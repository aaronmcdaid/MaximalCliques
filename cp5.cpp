#include "graph/network.hpp"
#include "graph/loading.hpp"
#include "graph/stats.hpp"
#include "clustering/components.hpp"
#include <getopt.h>
#include <unistd.h>
#include <libgen.h>
#include <ctime>
#include <vector>

#include "pp.hpp"
#include "cliques.hpp"
#include "cmdline.h"

using namespace std;

int option_minCliqueSize = 3;

static void do_clique_percolation_variant_5(vector<clustering :: components> &all_percolation_levels, const int32_t min_k, const vector< vector<int32_t> > &the_cliques) ;

int main(int argc, char **argv) {
	gengetopt_args_info args_info;

	// there shouldn't be any errors in processing args
	if (cmdline_parser (argc, argv, &args_info) != 0)
		exit(1) ;
	// .. and there should be exactly one non-option arg
	if(args_info.inputs_num != 1 || args_info.k_arg < 3) {
		cmdline_parser_print_help();
		exit(1);
	}

	const char * edgeListFileName   = args_info.inputs[0];
	const int min_k = args_info.k_arg;

        std :: auto_ptr<graph :: NetworkInterfaceConvertedToString > network;
	if(args_info.stringIDs_flag) {
		network	= graph :: loading :: make_Network_from_edge_list_string(edgeListFileName, 0, 0);
	} else {
		network	= graph :: loading :: make_Network_from_edge_list_int64(edgeListFileName, 0, 0);
	}

	int32_t maxDegree = graph :: stats :: get_max_degree(network->get_plain_graph());

	cerr << "Network loaded"
	       << " after " << (double(clock()) / CLOCKS_PER_SEC) << " seconds. "
		<< network.get()->numNodes() << " nodes and " << network.get()->numRels() << " edges."
		<< " Max degree is " << maxDegree
	       << endl;

	vector< vector<int32_t> > the_cliques;
	cliques :: cliquesToVector(network.get(), min_k, the_cliques);

	const int32_t C = the_cliques.size();
	if(C==0) {
		cerr << endl << "Error: you don't have any cliques of at least size " << min_k << " Exiting." << endl;
		exit(1);
	}
	int max_clique_size = 0; // to store the size of the biggest clique
	for(vector<vector<int32_t> > :: const_iterator i = the_cliques.begin(); i != the_cliques.end(); i++) {
		if(max_clique_size < (int)i->size())
			max_clique_size = i->size();
	}
	PP(max_clique_size);
	assert(max_clique_size > 0);

	vector<clustering :: components> all_percolation_levels(max_clique_size+1);
	for(int32_t i = min_k; i <= max_clique_size; i++)
		all_percolation_levels.at(i).setN(C);

	// finally, call the clique_percolation algorithm proper

	do_clique_percolation_variant_5(all_percolation_levels, min_k, the_cliques);
}

class bloom { // http://en.wikipedia.org/wiki/Bloom_filter
	static const int64_t l = 10000000000;
	vector<bool> data;
	int64_t occupied;
public:
	bloom() : data(this->l), occupied(0) {  // 10 giga-bits 1.25 GB
	}
	bool test(const int64_t a) const {
		const int64_t b = a % l;
		return this->data.at(b);
	}
};

static void do_clique_percolation_variant_5(vector<clustering :: components> &all_percolation_levels, const int32_t min_k, const vector< vector<int32_t> > &the_cliques) {
	const int32_t C = the_cliques.size();
	const int32_t max_k = all_percolation_levels.size()-1;
	PP3(C, min_k, max_k);
	assert(min_k <= max_k && C > 0);

	// go through each clique, from the 'earlier' to 'later' cliques.
	// for each clique, find all the 'earlier' cliques with which it overlaps by at least min_k-1
	// feed those results into the components
	bloom bl;
	for(int c = 0; c < C; c++) {
		PP2(c, the_cliques.at(c).size());
	}
}
