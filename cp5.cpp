#include "graph/network.hpp"
#include "graph/loading.hpp"
#include "graph/stats.hpp"
#include "clustering/components.hpp"
#include "pp.hpp"
#include "cliques.hpp"
#include "cmdline.h"

#include <getopt.h>
#include <unistd.h>
#include <libgen.h>
#include <ctime>
#include <algorithm>
#include <vector>
#include <tr1/functional>
#include <cassert>
#include <limits>


using namespace std;

typedef vector<int32_t> clique; // the nodes will be in increasing numerical order

static void do_clique_percolation_variant_5(vector<clustering :: components> &all_percolation_levels, const int32_t min_k, const vector< clique > &the_cliques) ;
static void do_clique_percolation_variant_6(vector<clustering :: components> &all_percolation_levels, const int32_t min_k, const vector< clique > &the_cliques) ;

template<typename T>
string thou(T number);
#define PPt(x) PP(thou(x))
#define ELAPSED (double(clock())/CLOCKS_PER_SEC)
#define HOWLONG "(runtime: " << ELAPSED <<"s)"


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

	vector< clique > the_cliques;
	cliques :: cliquesToVector(network.get(), min_k, the_cliques);

	const int32_t C = the_cliques.size();
	if(C==0) {
		cerr << endl << "Error: you don't have any cliques of at least size " << min_k << ". Exiting." << endl;
		exit(1);
	}
	int max_clique_size = 0; // to store the size of the biggest clique
	for(vector<clique > :: const_iterator i = the_cliques.begin(); i != the_cliques.end(); i++) {
		if(max_clique_size < (int)i->size())
			max_clique_size = i->size();
	}
	PP(max_clique_size);
	assert(max_clique_size > 0);

	vector<clustering :: components> all_percolation_levels(max_clique_size+1);
	for(int32_t i = min_k; i <= max_clique_size; i++)
		all_percolation_levels.at(i).setN(C);

	// finally, call the clique_percolation algorithm proper

	if(0)
		do_clique_percolation_variant_5(all_percolation_levels, min_k, the_cliques);
	else
		do_clique_percolation_variant_6(all_percolation_levels, min_k, the_cliques);
}

class bloom { // http://en.wikipedia.org/wiki/Bloom_filter
	vector<bool> data;
public: // make private
	static const int64_t l = 10000000000;
	int64_t occupied;
	int64_t calls_to_set;
	std :: tr1 :: hash<int64_t> h;
public:
	bloom() : data(this->l), occupied(0), calls_to_set(0) {  // 10 giga-bits 1.25 GB
	}
	bool test(const int64_t a) const {
		const int64_t b = h(a) % l;
		return this->data.at(b);
	}
	void set(const int64_t a)  {
		++ this->calls_to_set;
		const int64_t b = h(a) % l;
		bool pre = this->data.at(b);
		this->data.at(b) = true;
		if(!pre) {
			++ this->occupied;
		}
	}
};
class intersecting_clique_finder { // based on a tree of all cliques, using a bloom filter to cut branch from the search tree
	bloom bl;
	const int32_t power_up;
public:
	intersecting_clique_finder(const int32_t p) : power_up(p) {
	}
	GET(bl, get_bloom_filter);
	int32_t overlap_estimate(const clique &new_clique, const int32_t branch_identifier) const {
		int32_t potential_overlap = 0;
		for(size_t n = 0; n < new_clique.size(); n++) {
			const int32_t node_id = new_clique.at(n);
			const int64_t a = (int64_t(branch_identifier) << 32) + node_id;
			potential_overlap += this->bl.test(a) ? 1 : 0;
		}
		return potential_overlap;
	}
	void add_clique_to_bloom(const clique &new_clique, int32_t branch_identifier) {
		while(branch_identifier) {
			for(size_t n = 0; n < new_clique.size(); n++) {
				const int32_t node_id = new_clique.at(n);
				const int64_t a = (int64_t(branch_identifier) << 32) + node_id;
				this->bl.set(a);
			}
			branch_identifier >>= 1;
		}
	}
	bool is_leaf_node(const int32_t branch_identifier) const {
		if(branch_identifier >= this->power_up) {
			return true;
		} else {
			return false;
		}
	}
	int32_t to_leaf_id(const int32_t branch_identifier) const {
		assert(branch_identifier >= this->power_up);
		const int32_t leaf_id = branch_identifier - this->power_up;
		return leaf_id;
	}
};

static int32_t actual_overlap(const clique &old_clique, const clique &new_clique) ;
static void search_for_candidate_matches(const intersecting_clique_finder &bl, const vector<clique> &the_cliques, const int32_t new_clique_id, const int32_t power_up, const int32_t branch_identifier, int64_t &count_searches, int64_t &count_successes, const int32_t min_k) ;


static void search_for_candidate_matches(const intersecting_clique_finder &isf, const vector<clique> &the_cliques, const int32_t new_clique_id, const int32_t power_up, const int32_t branch_identifier, int64_t &count_searches, int64_t &count_successes, const int32_t min_k) {
	// before we all this new clique, let's see which existing cliques it matches with
	// we branch from the top down this time
	const clique &new_clique = the_cliques.at(new_clique_id);
	int32_t potential_overlap = 0;
	if(branch_identifier != 1) { // we won't bother checking the root
		++ count_searches;
		potential_overlap = isf.overlap_estimate(new_clique, branch_identifier);
		if(potential_overlap < min_k-1)
			return;
	}
	if(branch_identifier >= power_up) {
		// branch_identifier - power_up is a candidate clique
		const int32_t cand_clique_id = branch_identifier - power_up; // the clique we think has sufficient nodes
		if(cand_clique_id < new_clique_id) {
			const int32_t actual = actual_overlap(the_cliques.at(cand_clique_id), new_clique);
			// cout << ELAPSED << '\t';
			// PP4( cand_clique_id, new_clique_id, potential_overlap, actual);
			assert(actual <= potential_overlap); // Princeton is the first file I found that catches this. Good to see it happens so rarely
			if(actual == potential_overlap) {
				++ count_successes;
			}
		}
	} else {
		search_for_candidate_matches(isf, the_cliques, new_clique_id, power_up, 2*branch_identifier   ,count_searches,count_successes, min_k);
		search_for_candidate_matches(isf, the_cliques, new_clique_id, power_up, 2*branch_identifier+1 ,count_searches,count_successes, min_k);
	}
}

static void recursive_search(const intersecting_clique_finder &search_tree
		, const int32_t branch_identifier
		, const clique &current_clique
		, const int32_t t
		, const vector<clique> &the_cliques
		, int32_t &searches_performed
		, vector<int32_t> &cliques_found
		) {
	// PRECONDITION:
	//    - we assume that the current branch in the tree already has enough nodes
	//
	// is this a leaf node?
	//   - if at a leaf node
	//     - perform the validation. (actual_overlap)
	//   - if not at a leaf
	//     - decide which sub branches, if any, to visit. And decide in what order.

	if(search_tree.is_leaf_node(branch_identifier)) {
		// time to check if this clique really does have a big enough overlap
		const int32_t leaf_clique_id = search_tree.to_leaf_id(branch_identifier);
		assert(leaf_clique_id >= 0); // remember, this leaf mightn't really represent a clique (i.e. leaf_clique_id >= the_cliques.size()) 

		const int32_t actual = size_t(leaf_clique_id) < the_cliques.size()
			? actual_overlap(the_cliques.at(leaf_clique_id), current_clique)
			: 0
			;
		if(actual >= t) {
			assert(leaf_clique_id >= 0 && size_t(leaf_clique_id) < the_cliques.size());
			cliques_found.push_back(leaf_clique_id);
		}
	} else {
		const int32_t left_subnode_id = branch_identifier << 1;
		assert(left_subnode_id >= 0);  // just in case the <<1 made it negative
		const int32_t right_subnode_id = left_subnode_id + 1;
#define branch_cut
#ifdef branch_cut
		const int32_t potential_overlap_left  = search_tree.overlap_estimate(current_clique, left_subnode_id);
		const int32_t potential_overlap_right = search_tree.overlap_estimate(current_clique, right_subnode_id);
#endif
		searches_performed += 2; // checked the left and the right
#ifdef branch_cut
		if(potential_overlap_left >= t)
#endif
			recursive_search(search_tree, left_subnode_id, current_clique, t, the_cliques, searches_performed, cliques_found);
#ifdef branch_cut
		if(potential_overlap_right >= t)
#endif
			recursive_search(search_tree, right_subnode_id, current_clique, t, the_cliques, searches_performed, cliques_found);
	}
}

static void neighbours_of_one_clique(const vector<clique> &the_cliques
		, const int32_t current_clique_id
		, const clustering :: components & components
		, const int32_t t
		, const int32_t current_component_id
		, const intersecting_clique_finder & search_tree
		, int32_t &searches_performed
		, vector<int32_t> &cliques_found
		) {
	// given:
	//    - one clique,
	//    - a component object describing all the components for the current value of k,
	//    - the intersection threshold (t)   { e.g. t = k-1 }
	//    - the component the clique is currently in (it's probably just been recently added)
	// return:
	//    - the list of cliques adjacent to the clique (at least t nodes in common),
	//    - BUT without the cliques that are already in the current component
		assert(current_component_id == components.my_component_id(current_clique_id));
		const clique &current_clique = the_cliques.at(current_clique_id);
		const int32_t root_node = 1;
		recursive_search(search_tree, root_node, current_clique, t, the_cliques, searches_performed, cliques_found);
}

struct too_many_cliques_exception : public std :: exception { };

static void do_clique_percolation_variant_5(vector<clustering :: components> &all_percolation_levels, const int32_t min_k, const vector< clique > &the_cliques) {
	if(the_cliques.size() > static_cast<size_t>(std :: numeric_limits<int32_t> :: max())) {
		throw too_many_cliques_exception();
	}
	const int32_t C = the_cliques.size();
	const int32_t max_k = all_percolation_levels.size()-1;
	PP3(C, min_k, max_k);
	assert(min_k <= max_k && C > 0);

	int32_t power_up = 1; // this is to be the smallest power of 2 greater than, or equal to, the number of cliques
	while(power_up < C)
		power_up <<= 1;
	assert(power_up > 0); // make sure it hasn't looped around and become negative!
	PP2(C, power_up);

	// go through each clique, from the 'earlier' to 'later' cliques.
	// for each clique, find all the 'earlier' cliques with which it overlaps by at least min_k-1
	// feed those results into the components
	intersecting_clique_finder isf(power_up);
	for(int c = 0; c < C; c++) {
		// cout << endl;
		int64_t searches_performed = 0;
		int64_t search_successes = 0;
		search_for_candidate_matches(isf, the_cliques, c, power_up, 1, searches_performed, search_successes, min_k);
		if(c%1000==0) {
			PP3(c, the_cliques.size(), double(clock())/CLOCKS_PER_SEC);
			PP2(searches_performed, search_successes);
			PP3(thou(isf.get_bloom_filter().l), thou(isf.get_bloom_filter().calls_to_set), thou(isf.get_bloom_filter().occupied));
		}
		isf.add_clique_to_bloom(the_cliques.at(c), c+power_up);
		// PP(bl.occupied);
	}

	PPt(isf.get_bloom_filter().l);
	PPt(isf.get_bloom_filter().calls_to_set);
	PPt(isf.get_bloom_filter().occupied);
}

static void do_clique_percolation_variant_6(vector<clustering :: components> &all_percolation_levels, const int32_t min_k, const vector< clique > &the_cliques) {
	if(the_cliques.size() > static_cast<size_t>(std :: numeric_limits<int32_t> :: max())) {
		throw too_many_cliques_exception();
	}
	const int32_t C = the_cliques.size();
	const int32_t max_k = all_percolation_levels.size()-1;
	PP3(C, min_k, max_k);
	assert(min_k <= max_k && C > 0);

	int32_t power_up = 1; // this is to be the smallest power of 2 greater than, or equal to, the number of cliques
	while(power_up < C)
		power_up <<= 1;
	assert(power_up > 0); // make sure it hasn't looped around and become negative!
	PP2(C, power_up);

	intersecting_clique_finder isf(power_up);
	for(int c = 0; c < C; c++) {
		isf.add_clique_to_bloom(the_cliques.at(c), c+power_up);
	}
	cout << "isf populated. " << HOWLONG << endl;
	PPt(isf.get_bloom_filter().l);
	PPt(isf.get_bloom_filter().calls_to_set);
	PPt(isf.get_bloom_filter().occupied);
	for(int c = 0; c < 1; c++) {
		const int32_t t = min_k-1; // at first, just for min_k-clique-percolation, we'll sort out the other levels later
		// at first, just one clique
		int32_t searches_performed = 0;
		vector<int32_t> cliques_found;
		const int32_t current_component_id = all_percolation_levels.at(min_k).my_component_id(c);
		neighbours_of_one_clique(the_cliques, c, all_percolation_levels.at(min_k), t, current_component_id, isf, searches_performed, cliques_found);
		int32_t search_successes = cliques_found.size();
		PP3(c, searches_performed, search_successes);
	}
}

template<typename T>
string thou(T number) {
	std::ostringstream ss;
	ss.imbue(std::locale(""));
	ss << number;
	return ss.str();
}


static int32_t actual_overlap(const clique &old_clique, const clique &new_clique) {
	vector<int32_t> intersection;
	set_intersection( old_clique.begin(), old_clique.end()
	                 ,new_clique.begin(), new_clique.end()
			 , back_inserter(intersection));
	return intersection.size();
}
