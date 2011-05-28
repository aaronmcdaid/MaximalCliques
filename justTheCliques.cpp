using namespace std;
#include <getopt.h>
#include <unistd.h>
#include <libgen.h>
#include <ctime>

#include "aaron_utils.hpp"
#include "shmGraphRaw.hpp"
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


	auto_ptr<shmGraphRaw::ReadableShmGraphTemplate<shmGraphRaw::PlainMem> > g (shmGraphRaw::loadEdgeList<shmGraphRaw::PlainMem>(edgeListFileName));
	cerr << "Network loaded"
	       << " after " << (double(clock()) / CLOCKS_PER_SEC) << " seconds. "
		<< g->numNodes() << " nodes and " << g->numRels() << " edges."
	       << endl;
	cliques::cliquesToStdout(g.get(), k);

}
