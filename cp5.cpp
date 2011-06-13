#include "graph/network.hpp"
#include "graph/loading.hpp"
#include "graph/stats.hpp"
#include "macros.hpp"
#include "cliques.hpp"
#include "cmdline-cp5.h"
#include "comments.hh"

#include <vector>
#include <tr1/unordered_set>
#include <map>
#include <stack>
#include <bitset>

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
	int32_t component_count() const {
		return this->num_components;
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
	vector<int32_t> potentially_available; // will be used as a stack; push_back and pop_back
public:
	const vector<int32_t> & get_all_members() const {
		return this->potentially_available;
	}
	void insert(int32_t c) {
		this->potentially_available.push_back(c);
	}
	size_t size() const {
		return this->potentially_available.size();
	}
	int32_t get_next(const comp &current_component, int32_t source_component) { // return -1 if none available
		//PP2(__LINE__, ELAPSED);
		// pop items from the stack until one is identified which hasn't yet been assigned in the current_component
		// if called repeatedly, it'll return the same value, at least until the relevant node (i.e. clique) is moved from source_component
		while(1) {
			if(this->potentially_available.empty()){
				//PP2(__LINE__, ELAPSED);
				return -1;
			}
			const int32_t node_id = this->potentially_available.back();
			if(current_component.my_component_id(node_id) == source_component) {
				//PP2(__LINE__, ELAPSED);
				return node_id;
			}
			this->potentially_available.pop_back(); // this node is no longer available, let's discard it
		}
	}
};


static void do_clique_percolation_variant_5b(const int32_t min_k, const int32_t max_k, const int32_t max_k_to_percolate, const vector< clique > &the_cliques, const char * output_dir_name, const graph :: NetworkInterfaceConvertedToString *network) ;
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
		, vector<maybe_available>  & members_of_the_source_components
		, comp * new_percolation_level
		, const int32_t new_k
		, const vector<int32_t> &found_communities
		, const comp * old_percolation_level
		, const vector<clique> &the_cliques
		) ; // identify candidates for the next level


static string memory_usage() {
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

static bool global_rebuild_occasionally = false; 

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
	PP(args_info.rebuild_bloom_flag);
	global_rebuild_occasionally = args_info.rebuild_bloom_flag;
	const char * edgeListFileName   = args_info.inputs[0];
	const char * output_dir_name   = args_info.inputs[1];
	const int min_k = args_info.k_arg;
	int max_k_to_percolate = args_info.K_arg;
	if(args_info.K_arg == -1) // default to using as many as possible
		max_k_to_percolate = numeric_limits<int32_t> :: max();
	assert(min_k > 2);
	assert(max_k_to_percolate >= min_k);

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

	vector< clique > the_cliques;
	cliques :: cliquesToVector(network.get(), min_k, the_cliques);

	// sort 'em here? By size? lexicographically? Graclus?

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
	if(max_k_to_percolate > max_clique_size)
		max_k_to_percolate = max_clique_size;
	PP3(min_k, max_k_to_percolate, max_clique_size);
	assert(max_clique_size > 0);
	assert(max_clique_size >= min_k);
	for(int k = min_k; k<=max_clique_size; k++) {
		cout << "# " << k << '\t' << cliqueFrequencies[k] << endl;
	}

	// finally, call the clique_percolation algorithm proper

	do_clique_percolation_variant_5b(min_k, max_clique_size, max_k_to_percolate, the_cliques, output_dir_name, network.get());
}

#define BLOOM_BITS 10000000000 /// 1.25 GB
class bloom { // http://en.wikipedia.org/wiki/Bloom_filter
	vector<bool> data;
public: // make private
	static const int64_t l;
	int64_t occupied;
	int64_t calls_to_set;
	std :: tr1 :: hash<int64_t> h;
public:
	void clear() {
		this->data.clear();
		this->data.resize(this->l);
		this->occupied = 0;
		this->calls_to_set = 0;
	}
	bloom() {  // 10 giga-bits 1.25 GB
		this->clear();
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
const int64_t bloom :: l = BLOOM_BITS;
// const int64_t bloom :: l = 40000000000; // 5 GB
class intersecting_clique_finder { // based on a tree of all cliques, using a bloom filter to cut branch from the search tree
	bloom bl;
	int32_t num_cliques_in_here;
public:
	const int32_t power_up; // the next power of two above the number of cliques
	double build_time; // seconds to construct
	void rebuild(const vector<clique> &the_cliques
			, const vector<int32_t> &the_clique_ids
			, const comp & current_percolation_level
			, const int32_t source_component_id)
	{
		const double pre_constructed = ELAPSED;
		bl.clear();
		this->num_cliques_in_here = 0;
		// initialize with the cliques that have at least t members in them.
		for(size_t x = 0; x < the_clique_ids.size(); x++) {
			const int c = the_clique_ids.at(x);
			if(current_percolation_level.my_component_id(c) == source_component_id) {
				// we need this if during rebuilding, as sometimes the_clique_ids is obsolete (too large)
				++ num_cliques_in_here;
				this->add_clique_to_bloom(the_cliques.at(c), c+power_up);
			}
		}
		const double post_constructed = ELAPSED;
		this->build_time = post_constructed - pre_constructed;
	}
	intersecting_clique_finder(const int32_t p, const vector<clique> &the_cliques, const vector<int32_t> &the_clique_ids, const comp & current_percolation_level, const int32_t source_component_id) : power_up(p) {
		this->rebuild(the_cliques, the_clique_ids, current_percolation_level, source_component_id);
	}
	int32_t get_num_cliques_in_here() const {
		return num_cliques_in_here;
	}
	void dump_state(const int32_t k) const {
		cout << "isf populated for k = " << k << ". "
			<< " " << thou(this->get_bloom_filter().occupied)
			<< "/" << thou(this->get_bloom_filter().l)
			<< "  " << this->num_cliques_in_here << " cliques "
			<< HOWLONG
			<< "(" << memory_usage() << ")"
			<< " construct_time=" << this->build_time << "s."
			<< endl;
	}
	const bloom & get_bloom_filter(void) const { return this->bl; }
	int32_t overlap_estimate(const clique &new_clique, const int32_t branch_identifier) const {
		assert(branch_identifier > 1); // never call this on the root node, it hasn't been populated
		int32_t potential_overlap = 0;
		for(size_t n = 0; n < new_clique.size(); n++) {
			const int32_t node_id = new_clique.at(n);
			const int64_t a = (int64_t(branch_identifier) << 32) + node_id;
			potential_overlap += this->bl.test(a) ? 1 : 0;
		}
		return potential_overlap;
	}
	int32_t overlap_estimate(const clique &new_clique, const int32_t branch_identifier, int32_t t) const {
		assert(branch_identifier > 1); // never call this on the root node, it hasn't been populated
		// we're interested *only* in whether the overlap is >= t. We'll short-circuit once the answer is known.
		int32_t potential_overlap = 0;
		const size_t sz = new_clique.size();
		for(size_t n = 0; n < sz; n++) {
			const int32_t node_id = new_clique.at(n);
			const int64_t a = (int64_t(branch_identifier) << 32) + node_id;
			potential_overlap += this->bl.test(a) ? 1 : 0;
			if(potential_overlap >= t)
				return t;
			if(potential_overlap + int32_t(sz - n - 1) < t)
				return 0;
		}
		return potential_overlap;
	}
	void add_clique_to_bloom(const clique &new_clique, int32_t branch_identifier) {
		while(branch_identifier > 1) { // we shouldn't bother populating the root node
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

class assigned_branches_t_private_data_members {
public:
	int32_t power_up;
	int32_t number_of_cliques;
	int32_t total_marked;
	vector<bool> assigned_branches; // the branches where all subleaves have already been assigned. // the recursive search should stop immediately
};
struct assigned_branches_t : private assigned_branches_t_private_data_members {
public:
	int32_t num_valid_leaf_assigns; // this is public, to let us reset when we feel like it.
	int32_t C2;
	explicit assigned_branches_t(int32_t p, int32_t C) {
		this->num_valid_leaf_assigns = 0;
		this->C2 = 0;
		this->power_up = p;
		this->number_of_cliques = C;
		this->total_marked = 0;
		assigned_branches.resize(2*power_up, false);
		int total_premarked_as_invalid = 0;
		for(int invalid_leaf = power_up + C; invalid_leaf < 2 * power_up; invalid_leaf++ ) {
			int marked_this_time = this->mark_as_done(invalid_leaf);
			total_premarked_as_invalid += marked_this_time;
		}
		this->num_valid_leaf_assigns = 0;
	}
	const assigned_branches_t_private_data_members & get() const {
		return *this;
	}
	int32_t mark_as_done(const int32_t branch_id) {
		assert(branch_id >= this->power_up);
		assert(this->assigned_branches.at(branch_id) == false);
		if(this->assigned_branches.at(branch_id) == false) {
			++ this->num_valid_leaf_assigns;
		}
		return this->mark_as_done_(branch_id);
	}
private:
	int32_t mark_as_done_(const int32_t branch_id) {
		assert(branch_id >= 0 && size_t(branch_id) < this->assigned_branches.size());
		int32_t marked_this_time = 0;
		if(this->assigned_branches.at(branch_id) == false) {
			++ total_marked;
			this->assigned_branches.at(branch_id) = true;
			++ marked_this_time;
			if(branch_id > 1) { // all branches, but the root, will have a partner
			                    // .. if the partner is marked, then move up
				if(this->assigned_branches.at(branch_id ^ 1) == true) {
					marked_this_time += this->mark_as_done_(branch_id >> 1);
				}
			}
		}
		return marked_this_time;
	}
};

static int32_t actual_overlap(const clique &old_clique, const clique &new_clique) ;

struct args_to_recursive_search { // many of the args never change. May aswell just pass them once
	const intersecting_clique_finder &search_tree;
	const int32_t current_clique_id;
	const int32_t t;
	const vector<clique> &the_cliques;
	const clique &current_clique;
	const comp * current_percolation_level;
	const int32_t component_already_in; // i.e. the community we're merging into now
	const int32_t source_component_id; // the component (i.e. k-1-level community we're pulling from. This is just needed for verification
};
static void recursive_search(
		int32_t branch_identifier
		, const args_to_recursive_search &args
		, vector<int32_t> &cliques_found
		, assigned_branches_t &assigned_branches
		) { /// args_to_ new recursive_search
restart:
	// PRECONDITION:
	//    - we NO LONGER assume that the current branch in the tree already has enough nodes, according to isf and assigned_branches
	//
	// is this a leaf node?
	//   - if at a leaf node
	//     - is it a valid clique (i.e. leaf_node_id < the_cliques.size()?
	//       - if not, ignore it
	//       - if so:
	//         - is it already in the community we're forming? if so, ignore it (but count it)
	//         - perform the validation. (actual_overlap)
	//   - if not at a leaf
	//     - decide which sub branches, if any, to visit. And decide in what order.

	if(assigned_branches.get().assigned_branches.at(branch_identifier) == true) {
		return; // this clique is no longer available
	}
	if(branch_identifier >= args.search_tree.power_up) { // is a leaf node, but might be invalid
		const int32_t leaf_clique_id = branch_identifier - args.search_tree.power_up;
		assert(size_t(leaf_clique_id) < args.the_cliques.size()); // the invalid leaves should be marked assigned already
	}

	assert(assigned_branches.get().assigned_branches.at(branch_identifier) == false);
	{ //optional optimization. If exactly one subbranch is unassigned, then skip directly to it
		if(branch_identifier < args.search_tree.power_up) {
			const int32_t left_subnode_id = branch_identifier << 1;
			assert(left_subnode_id >= 0);  // just in case the <<1 made it negative
			const int32_t right_subnode_id = left_subnode_id + 1;
			if         (assigned_branches.get().assigned_branches.at(left_subnode_id)
				&& !assigned_branches.get().assigned_branches.at(right_subnode_id)) {
				branch_identifier = right_subnode_id;
				goto restart; // oh yeah, you're not really optimizing until you have a goto
			}
			if         (!assigned_branches.get().assigned_branches.at(left_subnode_id)
				&& assigned_branches.get().assigned_branches.at(right_subnode_id)) {
				branch_identifier = left_subnode_id;
				goto restart; // oh yeah, you're not really optimizing until you have a goto
			}
		}
	}

	// time to check potential_overlap
	if(branch_identifier > 1) { // remember, the isf isn't populated at the root node
		const int32_t potential_overlap = args.search_tree.overlap_estimate(args.current_clique, branch_identifier, args.t);
		if(potential_overlap < args.t) {
			assert(potential_overlap == 0);
			return;
		} else
			assert(potential_overlap == args.t);
	}


	// if we made it this far, we must do one of two things
	// - if a leaf node (assert that it is a valid leaf)
	//   - then we check the actual overlap
	// - if not a leaf node
	//   - recursively search both child nodes

	if(branch_identifier >= args.search_tree.power_up) { // is a leaf node
		const int32_t leaf_clique_id = branch_identifier - args.search_tree.power_up;
		assert(leaf_clique_id >= 0);
		assert(size_t(leaf_clique_id) < args.the_cliques.size());
		assert(leaf_clique_id != args.current_clique_id);
		{
			const int32_t component_id_of_leaf = args.current_percolation_level->my_component_id(leaf_clique_id);
			assert(component_id_of_leaf != args.component_already_in);
			if (component_id_of_leaf != args.source_component_id) {
				/* this leaf is in a different source component
				* hence, we can ignore it
				* .. this is unlikely (impossible?) to happen, now there's a different
				* bloom for each source component.
				*/
				return; // not in the current source component
			}
			// time to check if this clique really does have a big enough overlap
			assert(component_id_of_leaf == args.source_component_id);
		}
		const int32_t actual = actual_overlap(args.the_cliques.at(leaf_clique_id), args.current_clique) ;
		assert(actual < (int32_t)args.current_clique.size()); // we never allow it to test for a match with itself, did you forget to include the seed into the community?
		if(actual >= args.t) {
			cliques_found.push_back(leaf_clique_id);
			assigned_branches.mark_as_done(branch_identifier); // this is *critical* for speed (if not accuracy). it stops it checking frontier<>frontier links.
		}
	} else { // not a leaf node. check subbranches
		const int32_t left_subnode_id = branch_identifier << 1;
		assert(left_subnode_id >= 0);  // just in case the <<1 made it negative
		const int32_t right_subnode_id = left_subnode_id + 1;
		assert(right_subnode_id <= 2* args.search_tree.power_up);

		recursive_search(left_subnode_id
				, args
				, cliques_found
				, assigned_branches);
		recursive_search(right_subnode_id
				, args
				, cliques_found
				, assigned_branches);
	}
}

static void neighbours_of_one_clique(const vector<clique> &the_cliques
		, const int32_t current_clique_id
		, const comp & components
		, const int32_t t
		, const int32_t current_component_id
		, const int32_t source_component_id
		, const intersecting_clique_finder & search_tree
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
		const int32_t root_node = 1; // if C==1, then this is also the only leaf node
		if(assigned_branches.get().assigned_branches.at(root_node) == false) { // otherwise, we've assigned everything and the algorithm can complete
			args_to_recursive_search args = {
				search_tree
				, current_clique_id
				, t
				, the_cliques
				, the_cliques.at(current_clique_id)
				, &components
				, current_component_id
				, source_component_id
			};
			recursive_search(
				root_node
				, args
				, cliques_found
				, assigned_branches
				);
		}
}

static void one_k (vector<int32_t> & found_communities
		, vector<int32_t>  & source_components
		, vector<maybe_available>  & members_of_the_source_components
		, comp &current_percolation_level
		, const int32_t t
		, const vector<clique> &the_cliques
		, const int32_t power_up
		, const int32_t C
	     );

static void do_clique_percolation_variant_5b(const int32_t min_k, const int32_t max_k, const int32_t max_k_to_percolate, const vector< clique > &the_cliques, const char * output_dir_name, const graph :: NetworkInterfaceConvertedToString *network) {
	assert(max_k_to_percolate <= max_k);

	assert(network);
	assert(output_dir_name);
	if(the_cliques.size() > static_cast<size_t>(std :: numeric_limits<int32_t> :: max())) {
		throw too_many_cliques_exception();
	}
	const int32_t C = the_cliques.size();

	PP4(C, min_k, max_k, max_k_to_percolate);
	assert(min_k > 0 && min_k <= max_k && C >= 1);

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
	{ // for k==min_k, just put every clique into one source_component
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
	for(int32_t k = min_k; k<=max_k_to_percolate; k++) {
		cout << endl << "Start processing for k = " << k << ". "
			<< HOWLONG
			<< "(" << memory_usage() << ")"
			<< endl;
		vector<int32_t> found_communities; // the component_ids of the communities that will be found
		const int32_t t = k-1;

		assert(members_of_the_source_components.size() > 0);
		assert(t == k-1);
		one_k(
			found_communities
			, source_components
			, members_of_the_source_components
			, *current_percolation_level
			, t
			, the_cliques
			, power_up
			, C
			);
		assert(source_components.size()==0);
		assert(members_of_the_source_components.size()==0); // ensure everything has been consumed
		/* The found communities are now in found_communities. Gotta write them out
		 */
		// PP4(__LINE__, "about to write", k, ELAPSED);
		cout << "Found communities. About to write them: "; PP(ELAPSED);
		write_all_communities_for_this_k(output_dir_name, k, found_communities, *current_percolation_level, the_cliques, network);
		cout << "Written " << found_communities.size() << " communities for k = " << k << endl;


		const int32_t new_k = k + 1;
		if(new_k > max_k_to_percolate) {
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
				, members_of_the_source_components
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
		, vector<int32_t> & source_components
		, vector<maybe_available> & members_of_the_source_components
		, comp &current_percolation_level
		, const int32_t t
		, const vector<clique> &the_cliques
		, const int32_t power_up
		, const int32_t C
	     ) {
	PP2(source_components.size(), members_of_the_source_components.size());
	/* We need a function that,
	 *  - given a list of source components, where the small cliques have been kept out
	 *  - does clique percolation on that, returning the component_ids of the found communities
	 */
	assert(found_communities.empty());

	assigned_branches_t assigned_branches(power_up, C); // the branches where all subleaves have already been assigned.  the recursive search should stop immediately upon reaching one of these
	int32_t C2 = 0; // the number of cliques that are big enough in this level, i.e. >= k nodes
	{ // all the cliques that are too small should be premarked as assigned
		for(int c=0; c<C; c++) {
			const int32_t clique_size = the_cliques.at(c).size();
			if(clique_size <= t) {
				assigned_branches.mark_as_done(power_up + c);
			} else
				C2 ++;
		}
	}
	assigned_branches.num_valid_leaf_assigns = 0;
	assigned_branches.C2 = C2;

	int64_t move_count = 0;
	assert (!source_components.empty());
	int num_cliques_fully_processed = 0;
	const double time_at_start_of_one_k = ELAPSED;
	int integral_time_already_printed = 0;
	while (!source_components.empty()) {
		const int32_t num_assigned_at_the_start_of_this_source = assigned_branches.num_valid_leaf_assigns;
		/* ie.  assigned_branches.num_valid_leaf_assigns - num_assigned_at_the_start_of_this_source
		 * will tell us how many cliques have, so far, been assigned in this source.
		 * By comparing that to num_cliques_in_this_source, we can see how far from the end we are
		 */

		assert(source_components.size() == members_of_the_source_components.size());
		const int32_t source_component = source_components.back();
		source_components.pop_back();
		maybe_available & the_cliques_yet_to_be_assigned_in_this_source_component = members_of_the_source_components.back();
		const int32_t num_cliques_in_this_source = the_cliques_yet_to_be_assigned_in_this_source_component.size();

		/* A distinct intersecting_clique_finder for each source_component,
		 * which can be wiped and rebuilt occasionally
		 */
		intersecting_clique_finder isf(power_up, the_cliques, the_cliques_yet_to_be_assigned_in_this_source_component.get_all_members(), current_percolation_level, source_component);
		isf.dump_state(t+1);
		assert(num_cliques_in_this_source == isf.get_num_cliques_in_here());



		assert( -1 != the_cliques_yet_to_be_assigned_in_this_source_component.get_next(current_percolation_level, source_component) ); // should never be fed an empty component
		while( -1 != the_cliques_yet_to_be_assigned_in_this_source_component.get_next(current_percolation_level, source_component) ) { // keep pulling out communities from current source-component
			// - find a clique that hasn't yet been assigned to a community
			// - create a new community by:
			//   - make it the first 'frontier' clique
			//   - keep adding it, and all its neighbours, to the community until the frontier is empty

			const int32_t seed_clique = the_cliques_yet_to_be_assigned_in_this_source_component.get_next(current_percolation_level, source_component);
			assert(assigned_branches.get().assigned_branches.at(power_up + seed_clique) == false);
			assert(the_cliques.at(seed_clique).size() > size_t(t));

			stack< int32_t, vector<int32_t> > frontier_cliques;
			frontier_cliques.push(seed_clique);
			const int32_t component_to_grow_into = current_percolation_level.create_empty_component();
			int32_t num_cliques_in_this_community = 0;

			current_percolation_level.move_node(seed_clique, component_to_grow_into, source_component);
			assigned_branches.mark_as_done(power_up + seed_clique);

			++ num_cliques_in_this_community;
			++ move_count;

			// PP2(__LINE__, ELAPSED);
			while(!frontier_cliques.empty()) {
				// PP(frontier_cliques.size());
				// PP2(assigned_branches.get().total_marked, 1048576 - assigned_branches.get().total_marked);
				const int32_t popped_clique = frontier_cliques.top();
				frontier_cliques.pop();

				assert(assigned_branches.get().assigned_branches.at(power_up + popped_clique));
				vector<int32_t> fresh_frontier_cliques_found;
				const int32_t current_component_id = current_percolation_level.my_component_id(popped_clique);
				assert(current_component_id == component_to_grow_into);

				// 350 -> 390  = .12
				// 400 -> 420  = .08
				// 440 -> 450  = .70
				if(global_rebuild_occasionally){ // rebuild isf?
					const int32_t num_cliques_remaining_in_this_source
						= num_cliques_in_this_source
						- (assigned_branches.num_valid_leaf_assigns - num_assigned_at_the_start_of_this_source);
					assert(num_cliques_remaining_in_this_source >= 0);
					if(num_cliques_remaining_in_this_source > 100
							&& 2*num_cliques_remaining_in_this_source < isf.get_num_cliques_in_here())
					{
						cout << "We can rebuild you" << endl;
						PP2(num_cliques_remaining_in_this_source, isf.get_num_cliques_in_here());
		isf.rebuild(the_cliques, the_cliques_yet_to_be_assigned_in_this_source_component.get_all_members(), current_percolation_level, source_component);
		isf.dump_state(t+1);
					}
				}


				neighbours_of_one_clique(the_cliques, popped_clique, current_percolation_level, t, component_to_grow_into, source_component, isf, fresh_frontier_cliques_found, assigned_branches);
				// int32_t search_successes = fresh_frontier_cliques_found.size();
				// const int32_t old_size_of_growing_community = current_percolation_level.get_members(component_to_grow_into).size();
				for(int x = 0; x < (int)fresh_frontier_cliques_found.size(); x++) {
					const int32_t frontier_clique_to_be_moved_in = fresh_frontier_cliques_found.at(x);
					frontier_cliques.push(frontier_clique_to_be_moved_in);
					assert(source_component == current_percolation_level.my_component_id(frontier_clique_to_be_moved_in));
					current_percolation_level.move_node(frontier_clique_to_be_moved_in, component_to_grow_into, source_component);
					++num_cliques_in_this_community;
					++ move_count;
					/*
					if(move_count % 100 == 0) {
						PP4(move_count, the_cliques.size(), ELAPSED, found_communities.size());
						PP(assigned_branches.get().total_marked);
					}
					*/
				}
				// const int32_t new_size_of_growing_community = current_percolation_level.get_members(component_to_grow_into).size();
				assert(frontier_cliques.size() < the_cliques.size());
				{
					++num_cliques_fully_processed;
					const double time_now = ELAPSED;
					const int integral_seconds_since_start_of_one_k = time_now - time_at_start_of_one_k;
					if(integral_seconds_since_start_of_one_k > integral_time_already_printed)
					// if(100 * num_cliques_fully_processed / C2 > 100 * (num_cliques_fully_processed-1) / C2)
					{
						integral_time_already_printed = integral_seconds_since_start_of_one_k;
						const double percent = 100.0 * num_cliques_fully_processed / C2;
						cout << "#frontier, #fully processed, ELAPSED, %processed:"
							<< '\t' << thou(frontier_cliques.size())
							<< '\t' << thou(num_cliques_fully_processed)
							<< '\t' << thou(ELAPSED) << 's'
							<< ' ' << percent << " %"
							<< endl;
					}
				}
			}
			// const int32_t final_size_of_growing_community = current_percolation_level.get_members(component_to_grow_into).size();
			// PP2(t+1, final_size_of_growing_community);
			found_communities.push_back(component_to_grow_into);
			// PP3(__LINE__, ELAPSED, num_cliques_in_this_community);
		}
		assert(num_cliques_in_this_source == assigned_branches.num_valid_leaf_assigns - num_assigned_at_the_start_of_this_source);
		assert(the_cliques_yet_to_be_assigned_in_this_source_component.size()==0);
		members_of_the_source_components.pop_back();
	} // looping over the source components
	PP2(t+1, ELAPSED - time_at_start_of_one_k);
	PP2(C2, assigned_branches.num_valid_leaf_assigns);
	assert(C2 == assigned_branches.num_valid_leaf_assigns);
	assert(C2 == num_cliques_fully_processed);
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
		, vector<maybe_available>  & members_of_the_source_components
		,       comp * new_percolation_level
		, const int32_t new_k
		, const vector<int32_t> & found_communities
		, const comp * old_percolation_level
		, const vector<clique> &the_cliques
		) { // identify candidates for the next level
	assert(source_components.empty());
	assert(members_of_the_source_components.empty());
	assert(new_percolation_level && new_percolation_level->component_count() == 1); // everything starts off in component 0 - the 'unassigned' component

	const size_t num_sources = found_communities.size();

	// each found community at k becomes a source component at k+1

	map<int32_t, maybe_available> the_cliques_in_each_community;
	for(size_t f = 0; f < num_sources; f++) {
		the_cliques_in_each_community[found_communities.at(f)]; // create the relevant entry in the map
	}
	assert(the_cliques_in_each_community.size() == num_sources);
	for(int c=0; c<(int)the_cliques.size(); c++) {
		if((int)the_cliques.at(c).size() >= new_k) {
			const int32_t comp_id_in_old =  old_percolation_level->my_component_id(c);
			assert(the_cliques_in_each_community.count(comp_id_in_old));
			the_cliques_in_each_community[comp_id_in_old].insert(c);
		}
	}
	assert(the_cliques_in_each_community.size() == num_sources);

	for (map<int32_t, maybe_available> :: const_iterator i = the_cliques_in_each_community.begin()
		 ; i != the_cliques_in_each_community.end()
		 ; ++i) {
		if(i->second.size()>0) { // this is worth passing up to the next level
			const int32_t new_source = new_percolation_level->create_empty_component();
			source_components.push_back(new_source);
			const vector<int32_t> &being_sent_up = i->second.get_all_members();
			for(size_t j=0; j<being_sent_up.size(); j++) {
				new_percolation_level->move_node(being_sent_up.at(j), new_source, 0);
			}
			members_of_the_source_components.push_back( i->second );
			// swap(i->second, members_of_the_source_components.back()); // will it efficiently swap the underlying vectors?
			// assert(i->second.size() == 0);
		}
	}
	assert(source_components.size() == members_of_the_source_components.size());
}
