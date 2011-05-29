#include "cliques.hpp"
#include <set>

namespace cliques {

void cliquesWorker(const SimpleIntGraph &g, CliqueFunctionAdaptor &cliquesOut, unsigned int minimumSize, vector<V> & Compsub, list<V> Not, list<V> Candidates);

static const bool verbose = false;

void cliquesForOneNode(const SimpleIntGraph &g, CliqueFunctionAdaptor &cliquesOut, int minimumSize, V v) {
	if(g->degree(v) + 1 < minimumSize)
		return; // Obviously no chance of a clique if the degree is too small.


	vector<V> Compsub;
	list<V> Not, Candidates;
	Compsub.push_back(v);


	// copy those below the split into Not
	// copy those above the split into Candidates
	// there shouldn't ever be a neighbour equal to the split, this'd mean a self-loop
	{
		graph :: neighbouring_node_id_iterator ns(g->get_plain_graph(), v);
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

	//copy(neighbours_of_v.first, split, back_inserter(Not));
	//copy(split, neighbours_of_v.second, back_inserter(Candidates));
	cliquesWorker(g, cliquesOut, minimumSize, Compsub, Not, Candidates);
}

static inline void tryCandidate (const SimpleIntGraph & g, CliqueFunctionAdaptor &cliquesOut, unsigned int minimumSize, vector<V> & Compsub, const list<V> & Not, const list<V> & Candidates, const V selected) {
	Compsub.push_back(selected);

	list<V> CandidatesNew_;
	list<V> NotNew_;
	{
		graph :: neighbouring_node_id_iterator ns(g->get_plain_graph(), selected);
		set_intersection(Candidates.begin()            , Candidates.end()
	                ,ns, ns.end_marker()
			,back_inserter(CandidatesNew_));
		graph :: neighbouring_node_id_iterator ns2(g->get_plain_graph(), selected);
		set_intersection(Not.begin()                 , Not.end()
	                ,ns2, ns2.end_marker()
			,back_inserter(NotNew_));
	}

	cliquesWorker(g, cliquesOut, minimumSize, Compsub, NotNew_, CandidatesNew_);

	Compsub.pop_back(); // we must restore Compsub, it was passed by reference
}
void cliquesWorker(const SimpleIntGraph & g, CliqueFunctionAdaptor &cliquesOut, unsigned int minimumSize, vector<V> & Compsub, list<V> Not, list<V> Candidates) {
	// p2p         511462                   (10)
	// authors000                  (250)    (<4)
	// authors010  212489     5.3s (4.013)


	unless(Candidates.size() + Compsub.size() >= minimumSize) return;

	if(Candidates.empty()) { // No more cliques to be found. This is the (local) maximal clique.
		if(Not.empty() && Compsub.size() >= minimumSize) cliquesOut(Compsub);
		return;
	}

	// We know Candidates is not empty. Must find the element, in Not or in Candidates, that is most connected to the (other) Candidates
	{ //version 2. Count disconnections-to-Candidates
		int fewestDisc = 1+Candidates.size();
		V fewestDiscVertex = Candidates.front();
		bool fewestIsInCands = false;
#define dout dummyOutputStream

		ContainerRange<list<V> > nRange(Not);
		ContainerRange<list<V> > cRange(Candidates);
		ChainedRange<ContainerRange<list<V> > >  frontier(nRange, cRange); // The concatenated range of Not and Candidates
		// TODO: Make use of degree, or something like that, to speed up this counting of disconnects?
		Foreach(V v, frontier) {
			int currentDiscs = 0;
			// dout << v << ": ";
			ContainerRange<list<V> > testThese(Candidates);
			Foreach(V v2, testThese) {
				if(!g->get_plain_graph()->are_connected(v, v2)) {
					// dout << "disconnected: (" << v << ',' << v2 << ") ";
					++currentDiscs;
				}
			}
			// dout << '\n';
			if(currentDiscs < fewestDisc) {
				fewestDisc = currentDiscs;
				fewestDiscVertex = v;
				fewestIsInCands = frontier.firstEmpty();
				if(!fewestIsInCands && fewestDisc==0) return; // something in Not is connected to everything in Cands. Just give up now!
			}
		}
		// dout << (fewestIsInCands ? 'c' : ' ') << ' ' << fewestDiscVertex << '(' << fewestDisc << ')' << '\n';

		{
			list<V> CandidatesCopy(Candidates);
			ContainerRange<list<V> > useTheDisconnected(CandidatesCopy);
			Foreach(V v, useTheDisconnected) {
				unless(Candidates.size() + Compsub.size() >= minimumSize) return;
				if(fewestDisc >0 && v!=fewestDiscVertex && !g->get_plain_graph()->are_connected(v, fewestDiscVertex)) {
					// dout << "Into Not " << v << '\n';
					unless(Candidates.size() + Compsub.size() >= minimumSize) return;
					// forEach(int cand, amd::mk_range(Candidates)) { PP(cand); }
					// PP(v);
					Candidates.erase(lower_bound(Candidates.begin(),Candidates.end(),v));
					tryCandidate(g, cliquesOut, minimumSize, Compsub, Not, Candidates, v);
					Not.insert(lower_bound(Not.begin(), Not.end(), v) ,v); // we MUST keep the list Not in order
					--fewestDisc;
				}
			}
			// dout << "fewestDisc==0  " << fewestDisc << '\n';
		}
		// assert(fewestDisc == 0);
		if(fewestIsInCands) { // The most disconnected node was in the Cands.
			unless(Candidates.size() + Compsub.size() >= minimumSize) return;
			Candidates.erase(lower_bound(Candidates.begin(),Candidates.end(),fewestDiscVertex));
			tryCandidate(g, cliquesOut, minimumSize, Compsub, Not, Candidates, fewestDiscVertex);
			// No need as we're about to return...  Not.insert(lower_bound(Not.begin(), Not.end(), fewestDiscVertex) ,fewestDiscVertex); // we MUST keep the list Not in order
		}
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



void cliquesToStdout(SimpleIntGraph g_, unsigned int minimumSize /* = 3*/ ) {

	CliquesToStdout cliquesOut(g_);

	findCliques<CliquesToStdout>(g_, cliquesOut, minimumSize);

	cerr << cliquesOut.n << " cliques found" << endl;
}

CliquesVector::CliquesVector() /*: CliqueSink(_g, fileName)*/ {
}
void CliquesVector::operator () (Clique Compsub) { // We MUST sort this, the clique percolation relies on sorted cliques to help with checking of overlaps.
		sort(Compsub.begin(), Compsub.end());
		if(Compsub.size() >= 3) {
			all_cliques.push_back(Compsub);
		}
}
bool moreBySize(const vector<V> & l, const vector<V> & r) { return l.size() > r.size(); }
/*virtual */CliqueFunctionAdaptor::~CliqueFunctionAdaptor() {
}

} // namespace cliques
