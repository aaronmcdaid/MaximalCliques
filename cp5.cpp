#include "graph/network.hpp"
#include "graph/loading.hpp"
#include "graph/stats.hpp"
#include "macros.hpp"
#include "cliques.hpp"
#include "cmdline.h"

#include <vector>
#include <tr1/unordered_set>
#include <map>
#include <stack>

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

template<typename T>
string thou(T number);
#define PPt(x) PP(thou(x))
#define ELAPSED (double(clock())/CLOCKS_PER_SEC)
#define HOWLONG "(runtime: " << ELAPSED <<"s)"

typedef stack<int32_t, vector<int32_t> > int_stack;

class comp { // which component, if any, is this node in?
private:
	const int32_t N;
	vector<int32_t> com; // initialize with everything in cluster '0'
	int32_t num_components;
public:
	comp(int32_t _N) : N(_N), com(N,0), num_components(1) { // one giant component at first
	}
	int32_t my_component_id(const int32_t node_id) const {
		int32_t id = this->com.at(node_id);
		assert(id >= 0);
		return id;
	}
	int32_t create_empty_component() {
		return this->num_components ++; // must be *post* increment
	}
	void move_node(int32_t node_id, int32_t to, int32_t from) {
		assert(node_id >= 0);
		assert(node_id < this->N);
		assert(to >= 0);
		// PP2(to , this->num_components);
		assert(to < this->num_components);
		assert(from != to);
		assert(this->com.at(node_id) == from);
		this->com.at(node_id) = to;
	}
	const vector<int32_t> & get_com() const {
		return com;
	}
};
class maybe_available { // a stack of node_ids that are available (i.e. in the source_component, but not yet assigned to a community
private:
	int_stack potentially_available;
public:
	void insert(int32_t c) {
		this->potentially_available.push(c);
	}
	int32_t get_next(const comp &current_component, int32_t source_component) { // return -1 if none available
		PP2(__LINE__, ELAPSED);
		// pop items from the stack until one is identified which hasn't yet been assigned in the current_component
		// if called repeatedly, it'll return the same value, at least until the relevant node (i.e. clique) is moved from source_component
		while(1) {
			if(this->potentially_available.empty()){
				PP2(__LINE__, ELAPSED);
				return -1;
			}
			const int32_t node_id = this->potentially_available.top();
			if(current_component.my_component_id(node_id) == source_component) {
				PP2(__LINE__, ELAPSED);
				return node_id;
			}
			this->potentially_available.pop(); // this node is no longer available, let's discard it
		}
	}
};


static void do_clique_percolation_variant_5b(const int32_t min_k, const int32_t max_k, const vector< clique > &the_cliques, const char * output_dir_name, const graph :: NetworkInterfaceConvertedToString *network) ;
static void write_all_communities_for_this_k(const char * output_dir_name
		, const int32_t k
		, const vector<int32_t> &found_communities
		, const comp & current_percolation_level
		, const vector<clique> &the_cliques
		, const graph :: NetworkInterfaceConvertedToString *network
		);
static void create_directory_for_output(const char *dir);
static void source_components_for_the_next_level (
		vector<int32_t> &source_components
		, comp * new_percolation_level
		, const int32_t new_k
		, const vector<int32_t> &found_communities
		, const comp * old_percolation_level
		, const vector<clique> &the_cliques
		) ; // identify candidates for the next level


static string memory_usage() {
	ostringstream mem;
	PP("hi");
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
		cerr << endl << " == Need two args: edge_list and output_dir ==" << endl;
		exit(1);
	}

	const char * edgeListFileName   = args_info.inputs[0];
	const char * output_dir_name   = args_info.inputs[1];
	const int min_k = args_info.k_arg;
	assert(min_k > 2);

        std :: auto_ptr<graph :: NetworkInterfaceConvertedToString > network;
	if(args_info.stringIDs_flag) {
		network	= graph :: loading :: make_Network_from_edge_list_string(edgeListFileName, false, false, true);
	} else {
		network	= graph :: loading :: make_Network_from_edge_list_int64(edgeListFileName, false, false, true, 0);
	}

	PP(memory_usage());

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
	std :: map<size_t, int32_t> cliqueFrequencies;
	for(vector<clique > :: const_iterator i = the_cliques.begin(); i != the_cliques.end(); i++) {
		++cliqueFrequencies[i->size()];
	}
	assert(!cliqueFrequencies.empty());
	int max_clique_size = cliqueFrequencies.rbegin()->first;
	PP(max_clique_size);
	assert(max_clique_size > 0);
	assert(max_clique_size >= min_k);
	for(int k = min_k; k<=max_clique_size; k++) {
		cout << "# " << k << '\t' << cliqueFrequencies[k] << endl;
	}

	// finally, call the clique_percolation algorithm proper

	do_clique_percolation_variant_5b(min_k, max_clique_size, the_cliques, output_dir_name, network.get());
}

class bloom { // http://en.wikipedia.org/wiki/Bloom_filter
	vector<bool> data;
public: // make private
	static const int64_t l;
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
const int64_t bloom :: l = 100000000000;
class intersecting_clique_finder { // based on a tree of all cliques, using a bloom filter to cut branch from the search tree
	bloom bl;
public:
	const int32_t power_up; // the next power of two above the number of cliques
	intersecting_clique_finder(const int32_t p) : power_up(p) {
	}
	const bloom & get_bloom_filter(void) const { return this->bl; }
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
};
struct too_many_cliques_exception : public std :: exception { };

struct assigned_branches_t {
	int32_t power_up;
	int32_t number_of_cliques;
	vector<bool> assigned_branches; // the branches where all subleaves have already been assigned. // the recursive search should stop immediately
	assigned_branches_t(int32_t p, int32_t C) : power_up(p), number_of_cliques(C) {
		assigned_branches.resize(2*power_up, false);
		int total_premarked_as_invalid = 0;
		for(int invalid_leaf = power_up + C; invalid_leaf < 2 * power_up; invalid_leaf++ ) {
			int marked_this_time = this->mark_as_done(invalid_leaf);
			total_premarked_as_invalid += marked_this_time;
		}
	}
	int32_t mark_as_done(const int32_t branch_id) {
		assert(branch_id >= 0 && size_t(branch_id) < this->assigned_branches.size());
		int32_t marked_this_time = 0;
		if(this->assigned_branches.at(branch_id) == false) {
			this->assigned_branches.at(branch_id) = true;
			++ marked_this_time;
			if(branch_id > 1) { // all branches, but the root, will have a partner
			                    // .. if the partner is marked, then move up
				if(this->assigned_branches.at(branch_id ^ 1) == true) {
					marked_this_time += this->mark_as_done(branch_id >> 1);
				}
			}
		}
		return marked_this_time;
	}
};

static int32_t actual_overlap(const clique &old_clique, const clique &new_clique) ;

static int64_t calls_to_recursive_search = 0;

static void recursive_search(const intersecting_clique_finder &search_tree
		, const int32_t branch_identifier
		, const int32_t current_clique_id
		, const int32_t t
		, const vector<clique> &the_cliques
		, int32_t &searches_performed
		, vector<int32_t> &cliques_found
		, const comp * current_percolation_level
		, const int32_t component_already_in // i.e. the community we're merging into now
		, const int32_t source_component_id // the component (i.e. k-1-level community we're pulling from. This is just needed for verification
		, assigned_branches_t &assigned_branches
		) {
	++ calls_to_recursive_search;
	assert(calls_to_recursive_search > 0);
	// PRECONDITION:
	//    - we assume that the current branch in the tree already has enough nodes, according to isf and assigned_branches
	//
	// is this a leaf node?
	//   - if at a leaf node
	//     - is it a valid clique (i.e. leaf_node_id < the_cliques.size()?
	//       - if so, ignore it
	//       - if not:
	//         - is it already in the community we're forming? if so, ignore it (but count it)
	//         - perform the validation. (actual_overlap)
	//   - if not at a leaf
	//     - decide which sub branches, if any, to visit. And decide in what order.

	assert(assigned_branches.assigned_branches.at(branch_identifier) == false);

	const clique &current_clique = the_cliques.at(current_clique_id);
	if(branch_identifier >= search_tree.power_up) { // is a leaf node
		const int32_t leaf_clique_id = branch_identifier - search_tree.power_up;
		assert(leaf_clique_id >= 0); // remember, this leaf mightn't really represent a clique (i.e. leaf_clique_id >= the_cliques.size()) 
		if(size_t(leaf_clique_id) >= the_cliques.size()) { // I'm pretty sure this'll never happen, because the assigned_branches will already have marked these as invalid
			// PP2(leaf_clique_id, the_cliques.size()); // TODO: get out of branches earlier
		} else {
			const int32_t component_id_of_leaf = current_percolation_level->my_component_id(leaf_clique_id);
			if(component_id_of_leaf == component_already_in) {
				// We should never come in here either, as it's already in assigned_branches
				assert(1 == 2);
			} else {
				assert(component_id_of_leaf == source_component_id);
				// time to check if this clique really does have a big enough overlap

				assert(size_t(leaf_clique_id) < the_cliques.size());
				const int32_t actual = actual_overlap(the_cliques.at(leaf_clique_id), current_clique) ;
				assert(leaf_clique_id != current_clique_id);
				assert(actual < (int32_t)current_clique.size()); // don't allow it to match with itself, did you forget to include the seed into the community?
				if(actual >= t) {
					assert(leaf_clique_id >= 0 && size_t(leaf_clique_id) < the_cliques.size());
					cliques_found.push_back(leaf_clique_id);
					assigned_branches.mark_as_done(branch_identifier); // this is *critical* for speed (if not accuracy). it stops it checking frontier<>frontier links. Really should consider a DFS now!
				}
			}
		}
	} else { // not a leaf node. check subbranches
		const int32_t left_subnode_id = branch_identifier << 1;
		assert(left_subnode_id >= 0);  // just in case the <<1 made it negative
		const int32_t right_subnode_id = left_subnode_id + 1;

		const int32_t potential_overlap_left  =
			assigned_branches.assigned_branches.at(left_subnode_id) ? 0 :
			search_tree.overlap_estimate(current_clique, left_subnode_id);
		const int32_t potential_overlap_right =
			assigned_branches.assigned_branches.at(right_subnode_id) ? 0 :
			search_tree.overlap_estimate(current_clique, right_subnode_id);
		searches_performed += 2; // checked the left and the right

		if(potential_overlap_left >= t)
			recursive_search(search_tree, left_subnode_id, current_clique_id, t, the_cliques, searches_performed, cliques_found, current_percolation_level, component_already_in, source_component_id, assigned_branches);
		if(potential_overlap_right >= t)
			recursive_search(search_tree, right_subnode_id, current_clique_id, t, the_cliques, searches_performed, cliques_found, current_percolation_level, component_already_in, source_component_id, assigned_branches);
	}
}

static void neighbours_of_one_clique(const vector<clique> &the_cliques
		, const int32_t current_clique_id
		, const comp & components
		, const int32_t t
		, const int32_t current_component_id
		, const int32_t source_component_id
		, const intersecting_clique_finder & search_tree
		, int32_t &searches_performed
		, vector<int32_t> &cliques_found
		, assigned_branches_t &assigned_branches
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
		const int32_t root_node = 1;
		if(assigned_branches.assigned_branches.at(root_node) == false) // otherwise, we've assigned everything and the algorithm can complete
			recursive_search(search_tree
				, root_node
				, current_clique_id
				, t
				, the_cliques
				, searches_performed
				, cliques_found
				, &components
				, current_component_id
				, source_component_id
				, assigned_branches
				);
}

static void one_k (vector<int32_t> & found_communities
		, vector<int32_t> candidate_components
		, vector<maybe_available> members_of_the_source_components
		, comp &current_percolation_level
		, const int32_t t
		, const vector<clique> &the_cliques
		, const int32_t power_up
		, const int32_t C
		, const intersecting_clique_finder &isf
	     );

static void do_clique_percolation_variant_5b(const int32_t min_k, const int32_t max_k, const vector< clique > &the_cliques, const char * output_dir_name, const graph :: NetworkInterfaceConvertedToString *network) {

	assert(network);
	assert(output_dir_name);
	if(the_cliques.size() > static_cast<size_t>(std :: numeric_limits<int32_t> :: max())) {
		throw too_many_cliques_exception();
	}
	const int32_t C = the_cliques.size();
	if(C==1) { // nothing to be done, just one clique. Leave it on its own.
		return;
	}

	PP3(C, min_k, max_k);
	assert(min_k > 0 && min_k <= max_k && C > 1);

	int32_t power_up = 1; // this is to be the smallest power of 2 greater than, or equal to, the number of cliques
	while(power_up < C)
		power_up <<= 1;
	assert(power_up > 0); // make sure it hasn't looped around and become negative!
	PP2(C, power_up);
	create_directory_for_output(output_dir_name);

	/*
	 * The above is generic to all k
	 * The rest is specific for each k
	 */

	/* Here's the main loop of the algorithm:
	 *
	 * - take list of source components as input for a new value of k
	 * - for each source component 
	 *   - do the comm-finding in it, appending the found communities to found_communities,
	 *                                 and recording the detail in a components structure.
	 * - then prepare the next set of candidates, by copying the relevant components
	 *               and cliques into the next value of k
	 * - finally, update current_percolation_level and source_components ready for the next loop
	 */

	// we seed the loop by setting up for k == min_k first
	comp * current_percolation_level = NULL;
	vector<int32_t> source_components;
	vector<maybe_available> members_of_the_source_components; // the ids of the cliques in the source component
	{
		current_percolation_level = new comp(C);
		source_components.push_back(0);
		members_of_the_source_components.push_back( maybe_available() );
		for(int c=0; c<C; c++) {
			members_of_the_source_components.at(0).insert(c);
		}
	}
	assert(source_components.size() == members_of_the_source_components.size());

	/* Going into each loop in this for-loop
	 * - the input is essentially the source_components object, this will be updated at the end of each loop.
	 * - the output will be going into current_percolation_level, which again will be different at each loop.
	 */
	for(int32_t k = min_k; k<=max_k /* we never actually reach this, see the `break` below*/; k++) {
		vector<int32_t> found_communities; // the component_ids of the communities that will be found
		const int32_t t = k-1;

		PP3(__LINE__, k, ELAPSED);
		intersecting_clique_finder isf(power_up);
		{
			for(int c = 0; c < C; c++) {
				if(the_cliques.at(c).size() >= size_t(t))
					isf.add_clique_to_bloom(the_cliques.at(c), c+power_up);
			}
			cout << "isf populated for k = " << k << ". "
				<< " " << thou(isf.get_bloom_filter().occupied)
				<< "/" << thou(isf.get_bloom_filter().l)
				<< HOWLONG << endl;
		}

		PP3(__LINE__, k, ELAPSED);
		one_k(
			found_communities
			, source_components
			, members_of_the_source_components
			, *current_percolation_level
			, t
			, the_cliques
			, power_up
			, C
			, isf
			);
		/* The found communities are now in found_communities. Gotta write them out
		 */
		PP3(__LINE__, k, ELAPSED);
		write_all_communities_for_this_k(output_dir_name, k, found_communities, *current_percolation_level, the_cliques, network);

		PP3(k, found_communities.size(), ELAPSED);

		const int32_t new_k = k + 1;
		if(new_k > max_k) {
			delete current_percolation_level;
			current_percolation_level = NULL;
			break;
		}

		/* Now, to check which communities (and cliques therein) are
		 * suitable for passing up to the next level
		 */
		comp * new_percolation_level = new comp(C);

		source_components.clear();
		source_components_for_the_next_level (
				source_components
				, new_percolation_level
				, new_k
				, found_communities
				, current_percolation_level
				, the_cliques
				);

		swap(new_percolation_level, current_percolation_level);
		delete new_percolation_level; // this is actually deleting the old_level, because of the swap on the immediately preceding line.
		new_percolation_level = NULL;
	}
}

static void one_k (vector<int32_t> & found_communities
		, vector<int32_t> candidate_components
		, vector<maybe_available> members_of_the_source_components
		, comp &current_percolation_level
		, const int32_t t
		, const vector<clique> &the_cliques
		, const int32_t power_up
		, const int32_t C
		, const intersecting_clique_finder &isf
	     ) {
	/* We need a function that,
	 *  - given a list of source components, where the small cliques have been kept out
	 *  - does clique percolation on that, returning the component_ids of the found communities
	 */
	assert(found_communities.empty());

	assigned_branches_t assigned_branches(power_up, C); // the branches where all subleaves have already been assigned.  the recursive search should stop immediately upon reaching one of these
	{ // all the cliques that are too small should be premarked as assigned
		for(int c=0; c<C; c++) {
			const int32_t clique_size = the_cliques.at(c).size();
			if(clique_size <= t) {
				assigned_branches.mark_as_done(power_up + c);
			}
		}
	}


while (!candidate_components.empty()) {
	assert(candidate_components.size() == members_of_the_source_components.size());
	const int32_t source_component = candidate_components.back();
	// PP2(t+1, source_component);
	// cout << HOWLONG << endl;
	candidate_components.pop_back();
	maybe_available & the_cliques_yet_to_be_assigned_in_this_source_component = members_of_the_source_components.back();
	while( -1 != the_cliques_yet_to_be_assigned_in_this_source_component.get_next(current_percolation_level, source_component) ) { // keep pulling out communities from current source-component
		// - find a clique that hasn't yet been assigned to a community
		// - create a new community by:
		//   - make it the first 'frontier' clique
		//   - keep adding it, and all its neighbours, to the community until the frontier is empty

		PP2(__LINE__, ELAPSED);
		const int32_t seed_clique = the_cliques_yet_to_be_assigned_in_this_source_component.get_next(current_percolation_level, source_component);
		// PP(seed_clique);
		assert(assigned_branches.assigned_branches.at(power_up + seed_clique) == false);
		assert(the_cliques.at(seed_clique).size() > size_t(t));

		stack< int32_t, vector<int32_t> > frontier_cliques;
		frontier_cliques.push(seed_clique);
		const int32_t component_to_grow_into = current_percolation_level.create_empty_component();
		int32_t num_cliques_in_this_community = 0;

		current_percolation_level.move_node(seed_clique, component_to_grow_into, source_component);

		PP2(__LINE__, ELAPSED);
		while(!frontier_cliques.empty()) {
			// PP(frontier_cliques.size());
			const int32_t popped_clique = frontier_cliques.top();
			frontier_cliques.pop();

			assigned_branches.mark_as_done(power_up + popped_clique);

			int32_t searches_performed = 0;
			vector<int32_t> fresh_frontier_cliques_found;
			const int32_t current_component_id = current_percolation_level.my_component_id(popped_clique);
			assert(current_component_id == component_to_grow_into);
			neighbours_of_one_clique(the_cliques, popped_clique, current_percolation_level, t, component_to_grow_into, source_component, isf, searches_performed, fresh_frontier_cliques_found, assigned_branches);
			// int32_t search_successes = fresh_frontier_cliques_found.size();
			// const int32_t old_size_of_growing_community = current_percolation_level.get_members(component_to_grow_into).size();
			for(int x = 0; x < (int)fresh_frontier_cliques_found.size(); x++) {
				const int32_t frontier_clique_to_be_moved_in = fresh_frontier_cliques_found.at(x);
				frontier_cliques.push(frontier_clique_to_be_moved_in);
				assert(source_component == current_percolation_level.my_component_id(frontier_clique_to_be_moved_in));
				current_percolation_level.move_node(frontier_clique_to_be_moved_in, component_to_grow_into, source_component);
				++num_cliques_in_this_community;
				static int64_t move_count = 0;
				if(move_count % 100 == 0)
					PP3(move_count, ELAPSED, found_communities.size());
				++ move_count;
			}
			// const int32_t new_size_of_growing_community = current_percolation_level.get_members(component_to_grow_into).size();
			assert(frontier_cliques.size() < the_cliques.size());
		}
		// const int32_t final_size_of_growing_community = current_percolation_level.get_members(component_to_grow_into).size();
		// PP2(t+1, final_size_of_growing_community);
		// PP(calls_to_recursive_search);
		found_communities.push_back(component_to_grow_into);
		PP3(__LINE__, ELAPSED, num_cliques_in_this_community);
	}
} // looping over the source components
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

static void create_directory_for_output(const char *dir) {
	assert(dir);
	{
		const int32_t ret = mkdir(dir, 0777);
		if(ret != 0 && errno != EEXIST) {
			cerr << endl << "Couldn't create directory \"" << dir << "\". Exiting." << endl;
			exit(1);
		}
	}
}
static void write_all_communities_for_this_k(const char * output_dir_name
		, const int32_t k
		, const vector<int32_t> &found_communities
		, const comp & current_percolation_level
		, const vector<clique> &the_cliques
		, const graph :: NetworkInterfaceConvertedToString *network
		) {
			const int32_t C = the_cliques.size();
			map<int32_t, tr1 :: unordered_set<int32_t> > node_ids_in_each_community;
			{
				for(int32_t f = 0; f < (int32_t) found_communities.size(); f++) { // communities
					const int32_t found_community_component_id = found_communities.at(f);
					node_ids_in_each_community[found_community_component_id];
				}
				assert(found_communities.size() == node_ids_in_each_community.size());
				const vector<int32_t> & com = current_percolation_level.get_com();
				assert(com.size() == size_t(C));
				for (int32_t c=0; c<C; c++) {
					const int32_t comp_id_of_clique = com.at(c);
					const clique & the_clique = the_cliques.at(c);
					const int32_t size_of_clique = the_clique.size();
					if(node_ids_in_each_community.count(comp_id_of_clique)) {
						// this clique is in one of the found communities
						assert(size_of_clique >= k);
						tr1 :: unordered_set<int32_t> &node_ids_in_this_community
							= node_ids_in_each_community.at(comp_id_of_clique);
						for(int n=0; n<size_of_clique; n++) {
							node_ids_in_this_community.insert(the_clique.at(n));
						}
					} else {
						assert(size_of_clique < k);
					}
				}
				assert(found_communities.size() == node_ids_in_each_community.size());
			}

			assert(output_dir_name);
			ostringstream output_file_name;
			output_file_name << output_dir_name << "/" << "comm" << k;
			ofstream write_nodes_here(output_file_name.str().c_str());

			for (
	map<int32_t, tr1 :: unordered_set<int32_t> > :: const_iterator i = node_ids_in_each_community.begin();
	i != node_ids_in_each_community.end();
	++i
				)  { // communities
				const tr1 :: unordered_set<int32_t> &node_ids_in_this_community = i->second;
				bool first_node_on_this_line = true;
				for(std :: tr1 :: unordered_set<int32_t> :: const_iterator it = node_ids_in_this_community.begin()
						; it != node_ids_in_this_community.end()
						; ++it ) {
					const int32_t node_id = *it;
					const std :: string node_name = network->node_name_as_string(node_id);
					if(!first_node_on_this_line)
						write_nodes_here << ' ';
					write_nodes_here << node_name;
					first_node_on_this_line = false;
				} // writing nodes
				write_nodes_here << endl;
			} // communities
			write_nodes_here.close();
}

static void source_components_for_the_next_level ( // maybe this should return new_percolation_level ? via auto_ptr ?
		vector<int32_t> &source_components
		,       comp * new_percolation_level
		, const int32_t new_k
		, const vector<int32_t> & // found_communities
		, const comp * // old_percolation_level
		, const vector<clique> &the_cliques
		) { // identify candidates for the next level
	assert(source_components.empty());
	const int32_t new_cand = new_percolation_level->create_empty_component();
	int32_t number_of_cliques_found_that_are_suitably_big = 0;
	for(int c=0; c<(int)the_cliques.size(); c++) {
		if((int)the_cliques.at(c).size() >= new_k) {
			new_percolation_level->move_node(c, new_cand, 0);
			++ number_of_cliques_found_that_are_suitably_big;
		}
	}
	if(number_of_cliques_found_that_are_suitably_big>0)
		source_components.push_back(new_cand);
}
