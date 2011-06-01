using namespace std;
#include "graph/network.hpp"
#include "graph/loading.hpp"
#include "graph/stats.hpp"
#include <getopt.h>
#include <unistd.h>
#include <libgen.h>
#include <ctime>

#include "pp.hpp"
#include "cliques.hpp"
#include "cmdline.h"

int option_minCliqueSize = 3;

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
	const int k = args_info.k_arg;

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

	std :: vector< std :: vector<int32_t> > the_cliques;
	cliques :: cliquesToVector(network.get(), k, the_cliques);

}
