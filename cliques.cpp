#include "cliques.hpp"
#include <set>
#include <list>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include "Range.hpp"
#include "aaron_utils.hpp"
using namespace std;



namespace cliques {

typedef int32_t V;
typedef list<V> list_of_ints;

struct CliqueReceiver;
static void cliquesWorker(const SimpleIntGraph &g, CliqueReceiver *send_cliques_here, unsigned int minimumSize, vector<V> & Compsub, list_of_ints Not, list_of_ints Candidates);
static void findCliques(const SimpleIntGraph &g, CliqueReceiver *cliquesOut, unsigned int minimumSize);
static void cliquesForOneNode(const SimpleIntGraph &g, CliqueReceiver *send_cliques_here, int minimumSize, V v);
static const bool verbose = false;

struct CliqueReceiver {
	virtual void operator() (const std::vector<V> &clique) = 0;
	virtual ~CliqueReceiver() {}
};

static void cliquesForOneNode(const SimpleIntGraph &g, CliqueReceiver *send_cliques_here, int minimumSize, V v) {
	const int d = g->degree(v);
	if(d + 1 < minimumSize)
		return; // Obviously no chance of a clique if the degree is too small.


	vector<V> Compsub;
	list_of_ints Not, Candidates;
	Compsub.push_back(v);


	// copy those below the split into Not
	// copy those above the split into Candidates
	// there shouldn't ever be a neighbour equal to the split, this'd mean a self-loop
	{
		graph :: neighbouring_node_id_iterator ns(g, v);
		int32_t last_neighbour_id = -1;
		while( !ns.at_end() ) {
			const int neighbour_id = *ns;

			if(neighbour_id < v)
				Not.push_back(neighbour_id);
			if(neighbour_id > v)
				Candidates.push_back(neighbour_id);

			assert(last_neighbour_id < neighbour_id);
			last_neighbour_id = neighbour_id;
			++ ns;
		}
	}

	assert(d == int(Not.size() + Candidates.size()));

	cliquesWorker(g, send_cliques_here, minimumSize, Compsub, Not, Candidates);
}

static inline void tryCandidate (const SimpleIntGraph & g, CliqueReceiver *send_cliques_here, unsigned int minimumSize, vector<V> & Compsub, const list_of_ints & Not, const list_of_ints & Candidates, const V selected) {
	assert(!Compsub.empty());
	Compsub.push_back(selected); // Compsub does *not* have to be ordered. I might try to enforce that in future though.

	list_of_ints CandidatesNew_;
	list_of_ints NotNew_;
	{
		graph :: neighbouring_node_id_iterator ns(g, selected);
		set_intersection(Candidates.begin()            , Candidates.end()
	                ,ns, ns.end_marker()
			,back_inserter(CandidatesNew_));
		graph :: neighbouring_node_id_iterator ns2(g, selected);
		set_intersection(Not.begin()                 , Not.end()
	                ,ns2, ns2.end_marker()
			,back_inserter(NotNew_));
	}

	cliquesWorker(g, send_cliques_here, minimumSize, Compsub, NotNew_, CandidatesNew_);

	Compsub.pop_back(); // we must restore Compsub, it was passed by reference
}
static void cliquesWorker(const SimpleIntGraph &g, CliqueReceiver *send_cliques_here, unsigned int minimumSize, vector<V> & Compsub, list_of_ints Not, list_of_ints Candidates) {
	// p2p         511462                   (10)
	// authors000                  (250)    (<4)
	// authors010  212489     5.3s (4.013)


	unless(Candidates.size() + Compsub.size() >= minimumSize) return;

	if(Candidates.empty()) { // No more cliques to be found. This is the (local) maximal clique.
		if(Not.empty() && Compsub.size() >= minimumSize)
			send_cliques_here->operator()(Compsub);
		return;
	}

	assert(!Candidates.empty());


	/*
	 * version 2. Count disconnections-to-Candidates
	 */

	// We know Candidates is not empty. Must find the element, in Not or in Candidates, that is most connected to the (other) Candidates
	int fewestDisc = INT_MAX;
	V fewestDiscVertex = Candidates.front();
	bool fewestIsInCands = false;
	{
		ContainerRange<list_of_ints > nRange(Not);
		ContainerRange<list_of_ints > cRange(Candidates);
		ChainedRange<ContainerRange<list_of_ints > >  frontier(nRange, cRange); // The concatenated range of Not and Candidates
		// There'll be node in Candidates anyway.
		// TODO: Make use of degree, or something like that, to speed up this counting of disconnects?
		Foreach(V v, frontier) {
			int currentDiscs = 0;
			ContainerRange<list_of_ints > testThese(Candidates);
			Foreach(V v2, testThese) {
				if(!g->are_connected(v, v2)) {
					++currentDiscs;
				}
			}
			{ // count the *connections* from v to Candidates
			}
			if(currentDiscs < fewestDisc) {
				fewestDisc = currentDiscs;
				fewestDiscVertex = v;
				fewestIsInCands = frontier.firstEmpty();
				if(!fewestIsInCands && fewestDisc==0) return; // something in Not is connected to everything in Cands. Just give up now!
			}
		}
		assert(fewestDisc <= int(Candidates.size()));
	}
	{
			list_of_ints CandidatesCopy(Candidates);
			ContainerRange<list_of_ints > useTheDisconnected(CandidatesCopy);
			Foreach(V v, useTheDisconnected) {
				unless(Candidates.size() + Compsub.size() >= minimumSize) return;
				if(fewestDisc >0 && v!=fewestDiscVertex && !g->are_connected(v, fewestDiscVertex)) {
					unless(Candidates.size() + Compsub.size() >= minimumSize) return;
					// forEach(int cand, amd::mk_range(Candidates)) { PP(cand); }
					// PP(v);
					Candidates.erase(lower_bound(Candidates.begin(),Candidates.end(),v));
					tryCandidate(g, send_cliques_here, minimumSize, Compsub, Not, Candidates, v);
					Not.insert(lower_bound(Not.begin(), Not.end(), v) ,v); // we MUST keep the list Not in order
					--fewestDisc;
				}
			}
	}
		// assert(fewestDisc == 0);
	if(fewestIsInCands) { // The most disconnected node was in the Cands.
			unless(Candidates.size() + Compsub.size() >= minimumSize) return;
			Candidates.erase(lower_bound(Candidates.begin(),Candidates.end(),fewestDiscVertex));
			tryCandidate(g, send_cliques_here, minimumSize, Compsub, Not, Candidates, fewestDiscVertex);
			// No need as we're about to return...  Not.insert(lower_bound(Not.begin(), Not.end(), fewestDiscVertex) ,fewestDiscVertex); // we MUST keep the list Not in order
	}

#if 0
	while(!Candidates.empty()) { // This little bit is version 1, really slow on some graphs, but it's correct and easy to understand.
		V selected = Candidates.back();
		Candidates.pop_back();
		tryCandidate(g, cliquesOut, minimumSize, Compsub, Not, Candidates, selected);
		Not.insert(lower_bound(Not.begin(), Not.end(), selected) ,selected); // we MUST keep the list Not in order
	}
#endif
}


struct CliquesToStdout : public CliqueReceiver {
	int n;
	const graph :: NetworkInterfaceConvertedToString *g;
	CliquesToStdout(const graph :: NetworkInterfaceConvertedToString *_g) : n(0), g(_g) {}
	virtual void operator () (const vector<V> & Compsub) {
		vector<V> Compsub_ordered(Compsub);
		sort(Compsub_ordered.begin(), Compsub_ordered.end());
		bool firstField = true;
		if(Compsub_ordered.size() >= 3) {
			ForeachContainer(V v, Compsub_ordered) {
				if(!firstField)
					std :: cout	<< ' ';
				std :: cout <<  g->node_name_as_string(v) ;
				firstField = false;
			}
			std :: cout << endl;
			this -> n++;
		}
	}
};

static void findCliques(const SimpleIntGraph &g, CliqueReceiver *send_cliques_here, unsigned int minimumSize) {
	unless(minimumSize >= 3) throw std::invalid_argument("the minimumSize for findCliques() must be at least 3");

	for(V v = 0; v < (V) g->numNodes(); v++) {
		if(v && v % 100 ==0)
			cerr << "node: " << v << endl;
		cliquesForOneNode(g, send_cliques_here, minimumSize, v);
	}
}
void cliquesToStdout(const graph :: NetworkInterfaceConvertedToString * net, unsigned int minimumSize /* = 3*/ ) {

	CliquesToStdout send_cliques_here(net);
	findCliques(net->get_plain_graph(), & send_cliques_here, minimumSize);
	cerr << send_cliques_here.n << " cliques found" << endl;
}

} // namespace cliques
