using namespace std;
#include "graph/network.hpp"
#include "graph/loading.hpp"
#include <getopt.h>
#include <unistd.h>
#include <libgen.h>
#include <ctime>

#include "aaron_utils.hpp"
#include "cliques.hpp"

int option_minCliqueSize = 3;

int main(int argc, char **argv) {
	unless (argc ==3 || argc == 2 ) {
		cerr << "Usage: edge_list [k]" << endl;
		exit(1);
	}
	const char * edgeListFileName = argv[1];
	const int k = argc == 2 ? 3 : atoi(argv[2]);
	if(k < 3) {
		cerr << "Usage: edge_list [k]       where k > = 3" << endl;
		exit(1);
	}


        std :: auto_ptr<graph :: NetworkInt32 > network = graph :: loading :: make_Network_from_edge_list_int32(edgeListFileName, 0, 0);

	cerr << "Network loaded"
	       << " after " << (double(clock()) / CLOCKS_PER_SEC) << " seconds. "
		<< network.get()->numNodes() << " nodes and " << network.get()->numRels() << " edges."
	       << endl;
	// cliques::cliquesToStdout(g.get(), k);
	cliques :: cliquesToStdout(network.get(), k);

}
