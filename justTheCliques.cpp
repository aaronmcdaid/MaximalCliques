using namespace std;
#include <getopt.h>
#include <unistd.h>
#include <libgen.h>

#include "aaron_utils.hpp"
#include "shmGraphRaw.hpp"
#include "cliques.hpp"

int option_minCliqueSize = 3;

int main(int argc, char **argv) {
	unless (argc - optind == 1) {
		cerr << "Usage: edge_list" << endl;
		exit(1);
	}
	const char * edgeListFileName = argv[optind];

	auto_ptr<shmGraphRaw::ReadableShmGraphTemplate<shmGraphRaw::PlainMem> > g (shmGraphRaw::loadEdgeList<shmGraphRaw::PlainMem>(edgeListFileName));
	cliques::cliquesToDirectory(g.get(), "acp2_results", 3);

}
