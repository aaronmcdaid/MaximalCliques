#include "shmGraphRaw.hpp"

#include <vector>
#include <memory>

#include <boost/interprocess/containers/string.hpp>
using namespace boost::interprocess;


#include <string.h>

#include "aaron_utils.hpp"



namespace shmGraphRaw {

ReadableShmGraphBase::~ReadableShmGraphBase() {
}

int ReadableShmGraphBase::oppositeEndPoint(int relId, int oneEnd) const {
		const std::pair<int, int> & eps = this->EndPoints(relId);
		if(eps.first == oneEnd) {
			return eps.second;
		} else {
			assert(eps.second == oneEnd);
			return eps.first;
		}
}
std::string ReadableShmGraphBase::WhichNode(int v) const {
		ostringstream s;
		s << "\"";
		s << this->NodeAsString(v); // (*strings_wrapRO)[nodesRO->get<idT>().find(v )->string_h];
		s << "\"";
		return s.str();
}

static const char * STRINGS_MMAP = "Strings.mmap";
static const char * NODES_AND_RELS_MMAP = "NodesAndRels.mmap";
static const char * NEIGHBOURS_MMAP = "Neighbours.mmap";

struct fileDoesNotExistException {};
struct invalideGraphFileFormatException {};

typedef MMapType  ::allocator<char>::type              char_allocator;
typedef bip::basic_string<char, std::char_traits<char>, char_allocator> shm_string; // not to be confused with std::basic_string


/*
 * strings. Every string, including but not limited to node names, to be stored in a mmaped file
 */

StrH::StrH(int _i) : i(_i) {}
struct StrH::hasher {
	size_t operator() (StrH s)         const { return boost::hash_value(s.i); }
};
bool StrH::operator == (StrH r) const { return this->i == r.i; }
int StrH::get_underlying_id() const { return this->i; }

static size_t hashSHMString (const boost::container::basic_string<char, std::char_traits<char>, std::allocator<char> > &s) { std::string s2(s.c_str()); return boost::hash<std::string>() (s2); } // TODO: Make the index now how to hash itself
static bool equalSHMStrings (const boost::container::basic_string<char, std::char_traits<char>, std::allocator<char> > &l , const shm_string &r) { return 0 == strcmp(l.c_str(), r.c_str()); }
class StringWithId_Mic_WrapRO : public StringArray {
public:
	struct StringWithId { // every string, even those that are NOT node names, will be stored just once in a single mmap file.
		int         id;
		shm_string  s;
		StringWithId( int id_
	        	, const char *s_
	        	, const char_allocator &a)
	   	: id(id_), s(s_, a)
		{}
	};
	typedef bmi::multi_index_container<
  	StringWithId,
  	bmi::indexed_by<
	 	bmi::hashed_unique  <bmi::tag<idT>,  BOOST_MULTI_INDEX_MEMBER(StringWithId,int,id)>,
	 	bmi::hashed_unique  <bmi::tag<nameT>,BOOST_MULTI_INDEX_MEMBER(StringWithId,shm_string,s)>
		>,
  	MMapType  ::allocator<StringWithId>::type
	> StringWithId_Mic;
private:
	const StringWithId_Mic * const d;
public:
	explicit StringWithId_Mic_WrapRO(MMapType &segment_strings)
			: d( segment_strings.find<StringWithId_Mic> ("StringWithId_Mic") . first)
		{
		}
	virtual const char * operator[] (StrH sh) const {
		StringWithId_Mic::index_iterator<idT>::type i = d->get<idT>().find(sh.get_underlying_id());
		assert(i != d->get<idT>().end() );
		return i->s.c_str();
	}
	virtual StrH StringToStringId(const char *s) const {
		StringWithId_Mic::index_iterator<nameT>::type i = d->get<nameT>().find(s, hashSHMString, equalSHMStrings);
		assert(i != d->get<nameT>().end() );
		return StrH(i->id); // TODO: Remove Duplication of the two implementations of StringArrays members (and look for other dupes?)
	}
};
class ModifiableStringArray : public StringArray {
public:
	virtual size_t size() const = 0;
	virtual StrH insert(const char * s) = 0;
        virtual StrH insert(std::string &s) {
			return insert(s.c_str());
	}
	virtual const char * operator[] (StrH sh) const = 0;
	virtual StrH StringToStringId(const char *s) const = 0;
};
class ModifiableStringArrayInPlainMemory : public ModifiableStringArray {
private:
	std::vector<std::string> the_strings;
	boost::unordered_map<std::string, int> string2id; // TODO: map by const char *
public:
	explicit ModifiableStringArrayInPlainMemory() {
			this->insert("");
	}
	virtual size_t size() const {
		return the_strings.size();
	}
	virtual StrH insert(const char * s) {
		std::string str(s);
		if(string2id.count(str)==0) {
			int nextID = the_strings.size();
			string2id[str] = nextID;
			assert((int)string2id.size() == nextID+1);
			the_strings.push_back(str);
			return StrH(nextID);
		} else {
			return StrH(string2id.at(str));
		}
	}
	virtual const char * operator[] (StrH sh) const {
		return the_strings.at(sh.get_underlying_id()).c_str();
	}
	virtual StrH StringToStringId(const char *s) const {
		std::string str(s);
		return StrH(string2id.at(str));
	}
};
class StringWithId_Mic_Wrap : public ModifiableStringArray {

	char_allocator ca;
	typedef StringWithId_Mic_WrapRO::StringWithId StringWithId;
	typedef StringWithId_Mic_WrapRO::StringWithId_Mic StringWithId_Mic;
	StringWithId_Mic * const d;
public:
	// ~StringWithId_Mic_Wrap() { Pn("130"); delete d; Pn("160"); } // seems like you can't/shouldn't delete objects like this
	explicit StringWithId_Mic_Wrap(MMapType &segment_strings)
			: ca(char_allocator (segment_strings.get_allocator<char>()))
			, d( segment_strings.find_or_construct<StringWithId_Mic> ("StringWithId_Mic") ( StringWithId_Mic::ctor_args_list()                         , segment_strings.get_allocator<StringWithId>()))
		{
			this->insert("");
		}
	size_t size() const { return d->size(); }
	StrH insert(const char * s) { // insert, or find, this string. Return its id
		StringWithId_Mic::index_iterator<nameT>::type i = d->get<nameT>().find(s, hashSHMString, equalSHMStrings);
		if(i == d->get<nameT>().end()) {
			int proposedNewId = d->size();
			std::pair<StringWithId_Mic::iterator, bool> insertionResult = d->insert(StringWithId(proposedNewId, s, ca));
			assert(proposedNewId == insertionResult.first->id);
			assert(insertionResult.second);
			return StrH( insertionResult.first->id );
		} else {
			return StrH( i->id );
		}
	}
	StrH insert(std::string &s) { return this->insert(s.c_str()); }

	virtual const char * operator[] (StrH sh) const {
		StringWithId_Mic::index_iterator<idT>::type i = d->get<idT>().find(sh.get_underlying_id());
		assert(i != d->get<idT>().end() );
		return i->s.c_str();
	}
	virtual StrH StringToStringId(const char *s) const {
		StringWithId_Mic::index_iterator<nameT>::type i = d->get<nameT>().find(s, hashSHMString, equalSHMStrings);
		assert(i != d->get<nameT>().end() );
		return StrH(i->id);
	}
};

/*
 * nodes and relationships
 * neighbours. For each node, a set of the ids of its relationships.
 */
struct nodeWithName {
	int         id;
	StrH  string_h;
	nodeWithName( int _id
	        , StrH _string_h
	        )
	   : id(_id), string_h(_string_h)
	{}
};
template<class T> struct nodeWithName_set;
template<>        struct nodeWithName_set<MapMem> {
	typedef bmi::multi_index_container<
		nodeWithName,
		bmi::indexed_by<
			bmi::hashed_unique  <bmi::tag<idT>,  BOOST_MULTI_INDEX_MEMBER(nodeWithName,int,id)>,
			bmi::hashed_unique  <bmi::tag<nameT>,BOOST_MULTI_INDEX_MEMBER(nodeWithName,StrH,string_h) ,StrH::hasher>
		>
		, MMapType  ::allocator<nodeWithName>::type
	> t;
};
template<>        struct nodeWithName_set<PlainMem> {
	typedef bmi::multi_index_container<
		nodeWithName,
		bmi::indexed_by<
			bmi::hashed_unique  <bmi::tag<idT>,  BOOST_MULTI_INDEX_MEMBER(nodeWithName,int,id)>,
			bmi::hashed_unique  <bmi::tag<nameT>,BOOST_MULTI_INDEX_MEMBER(nodeWithName,StrH,string_h) ,StrH::hasher>
		>
	> t;
};


relationship::relationship( int id_ , const std::pair<int,int> &nodes_) : relId(id_), nodeIds(nodes_) {
		assert(nodes_ . first <= nodes_ . second); // make sure each relationship, including self-loops, appears only once
}



template <class T>
class DumbGraphReadableTemplate : public ReadableShmGraphTemplate<T> {
protected:
	const typename nodeWithName_set<T>::t *nodesRO;
	const typename T::relationship_set *relationshipsRO;
	const typename T::neighbours_to_relationships_map *neighbouring_relationshipsRO;
	std::auto_ptr<const StringArray> strings_wrapRO;
	const typename T::neighbouring_relationship_set *empty_set_for_neighboursRO;
public:
	virtual int numNodes() const;
	virtual int numRels()  const;
	virtual int numNodesWithAtLeastOneRel()  const;
	virtual const typename T::mmap_uset_of_ints & myRels(int n) const;
	virtual pair<const char*, const char*> EndPointsAsStrings(int relId) const;
	virtual const char * NodeAsString(int v) const;
	virtual int StringToNodeId(const char *s) const;
	virtual const std::pair<int, int> & EndPoints(int relId) const;
	virtual bool are_connected(int v1, int v2) const;
};
template<class T>
/* virtual */ int DumbGraphReadableTemplate<T>::numNodes() const { return nodesRO->size(); }
template<class T>
/* virtual */ int DumbGraphReadableTemplate<T>::numRels()  const { return relationshipsRO->size(); }
template<class T>
/* virtual */ int DumbGraphReadableTemplate<T>::numNodesWithAtLeastOneRel()  const { return neighbouring_relationshipsRO->size(); }
template<class T>
/* virtual */ const typename T::mmap_uset_of_ints & DumbGraphReadableTemplate<T>::myRels(int n) const {
		typename T::neighbours_to_relationships_map::const_iterator i = neighbouring_relationshipsRO->find(n);
		if(i == neighbouring_relationshipsRO->end()) { // it's possible to add a node without any corresponding edges. Must check for this.
			return *empty_set_for_neighboursRO;
		}
		else
			return i->second;
	}
template<class T>
/* virtual */ pair<const char*, const char*> DumbGraphReadableTemplate<T>::EndPointsAsStrings(int relId) const {
		assert(relId >=0 && relId < this->numRels() );
		const pair<int,int> &endpoints = relationshipsRO->template get<idT>().find(relId)->nodeIds;
		const char *l = this->NodeAsString(endpoints.first); // (*strings_wrap)[nodes->get<idT>().find(endpoints.first )->string_h];
		const char *r = this->NodeAsString(endpoints.second);// (*strings_wrap)[nodes->get<idT>().find(endpoints.second)->string_h];
		return make_pair(l,r);
	}
template<class T>
/* virtual */ const char * DumbGraphReadableTemplate<T>::NodeAsString(int v) const {
		return (*strings_wrapRO)[nodesRO->template get<idT>().find(v )->string_h];
	}
template<class T>
/* virtual */ int DumbGraphReadableTemplate<T>::StringToNodeId(const char *s) const {
		StrH string_handle = strings_wrapRO->StringToStringId(s);
		// assert (0==strcmp(s , (*strings_wrapRO)[string_handle]));
		typename nodeWithName_set<T>::t:: template index_iterator<nameT>::type i = nodesRO->template get<nameT>().find(string_handle );
		assert(i != nodesRO->template get<nameT>().end());
		// assert (0==strcmp(s , (*strings_wrapRO)[i->string_h]));
		return i->id;
	}
template<class T>
/* virtual */ const std::pair<int, int> & DumbGraphReadableTemplate<T>::EndPoints(int relId) const {
		assert(relId >=0 && relId < this->numRels() );
		return relationshipsRO->template get<idT>().find(relId)->nodeIds;
	}
template<class T>
/* virtual */ bool DumbGraphReadableTemplate<T>::are_connected(int v1, int v2) const {
		if(v1 > v2)
			swap(v1,v2);
		assert (v1 <= v2);
		return (relationshipsRO->template get<nodeIdsT>().end() != relationshipsRO->template get<nodeIdsT>().find(make_pair(v1,v2)) );
	}

DumbGraphReadableTemplate<PlainMem> testosdlfjslkdfjlsdjfkldsbject;

template <class T>
class DumbGraphReadONLYTemplate : public DumbGraphReadableTemplate<T> {
	typedef T BackingT;
	MMapType   segment_strings; // managed_mapped_file   segment               (open_read_only, (dir + "/" + NODES_AND_RELS_MMAP).c_str() );
	MMapType   segment_nodesAndRels; // managed_mapped_file   segment_neigh         (open_read_only, (dir + "/" + NEIGHBOURS_MMAP    ).c_str() );
	MMapType   segment_neigh;
	const typename T::neighbouring_relationship_set empty_set_for_neighbours;

public:
	virtual ~DumbGraphReadONLYTemplate() {
		// delete nodes; delete relationships; delete neighbouring_relationships; // seems like you can't/shouldn't delete objects like this
	}
	explicit DumbGraphReadONLYTemplate(const std::string &dir);
	static DumbGraphReadONLYTemplate<T> * New(const std::string &dir);
};
template <>
	DumbGraphReadONLYTemplate<MapMem>::DumbGraphReadONLYTemplate(const std::string &dir)
		: segment_strings     (open_read_only, (dir + "/" + STRINGS_MMAP       ).c_str() /*, 1000000*/)
		, segment_nodesAndRels(open_read_only, (dir + "/" + NODES_AND_RELS_MMAP).c_str() /*, 1000000*/)
		, segment_neigh       (open_read_only, (dir + "/" + NEIGHBOURS_MMAP    ).c_str() /*, 1000000*/)
		, empty_set_for_neighbours(segment_neigh.get_allocator<int>())
	{
		this->nodesRO         = segment_nodesAndRels.find<nodeWithName_set<MapMem>::t > ("nodeWithName_set").first; // ( nodeWithName_set::ctor_args_list()                         , segment_nodesAndRels.get_allocator<nodeWithName>());
		this->relationshipsRO = segment_nodesAndRels.find<MapMem::relationship_set> ("relationship_set").first; // ( relationship_set::ctor_args_list()                         , segment_nodesAndRels.get_allocator<relationship>());
	 	this->neighbouring_relationshipsRO
		                = segment_neigh.find<BackingT::neighbours_to_relationships_map>("Neighbours").first; // ( 3, boost::hash<int>(), std::equal_to<int>()  , segment_neigh.get_allocator<valtype>());    
		this->strings_wrapRO .reset( new StringWithId_Mic_WrapRO(segment_strings));
		this->empty_set_for_neighboursRO = &empty_set_for_neighbours;
	}
template <>
	DumbGraphReadONLYTemplate<MapMem> * DumbGraphReadONLYTemplate<MapMem>::New(const std::string &dir) {
		return new DumbGraphReadONLYTemplate<MapMem>(dir);
	}
template <>
	DumbGraphReadONLYTemplate<PlainMem> * DumbGraphReadONLYTemplate<PlainMem>::New(const std::string &dir) {
		assert(1==2);
		return NULL;
	}
#if 0
template <>
	DumbGraphReadONLYTemplate<PlainMem>::DumbGraphReadONLYTemplate(const std::string &dir)
		: segment_strings     (open_read_only, (dir + "/" + STRINGS_MMAP       ).c_str() /*, 1000000*/)
		, segment_nodesAndRels(open_read_only, (dir + "/" + NODES_AND_RELS_MMAP).c_str() /*, 1000000*/)
		, segment_neigh       (open_read_only, (dir + "/" + NEIGHBOURS_MMAP    ).c_str() /*, 1000000*/)
		, empty_set_for_neighbours()
	{
		this->nodesRO         = segment_nodesAndRels.find<nodeWithName_set> ("nodeWithName_set").first; // ( nodeWithName_set::ctor_args_list()                         , segment_nodesAndRels.get_allocator<nodeWithName>());
		this->relationshipsRO = new PlainMem::relationship_set();
	 	this->neighbouring_relationshipsRO
		                = segment_neigh.find<BackingT::neighbours_to_relationships_map>("Neighbours").first; // ( 3, boost::hash<int>(), std::equal_to<int>()  , segment_neigh.get_allocator<valtype>());    
		this->strings_wrapRO .reset( new StringWithId_Mic_WrapRO(segment_strings));
		this->empty_set_for_neighboursRO = &empty_set_for_neighbours;
	}
#endif

template <class T>
class DumbGraphRaw : public DumbGraphReadableTemplate<T> {
	typename T::segment_type   segment_strings; // managed_mapped_file   segment               (open_read_only, (dir + "/" + NODES_AND_RELS_MMAP).c_str() );
	typename T::segment_type   segment_nodesAndRels; // managed_mapped_file   segment_neigh         (open_read_only, (dir + "/" + NEIGHBOURS_MMAP    ).c_str() );
	typename T::segment_type   segment_neigh;

	const typename T::neighbouring_relationship_set empty_set_for_neighbours;

	typename nodeWithName_set<T>::t *nodes;
	typename T::relationship_set *relationships;
	typename T::neighbours_to_relationships_map *neighbouring_relationships;
public:
	//StringWithId_Mic_Wrap *strings_wrap;
	ModifiableStringArray *strings_wrap;
public:
	virtual ~DumbGraphRaw() {
		assert(this->strings_wrap == this->strings_wrapRO.get()); // delete strings_wrap; // DO NOT delete here, the auto_ptr already has it
		// delete nodes; delete relationships; delete neighbouring_relationships; // seems like you can't/shouldn't delete objects like this
	}
	explicit DumbGraphRaw(const std::string &dir);
	int insertNode(StrH node_name) {
		typename nodeWithName_set<T>::t:: template index_iterator<nameT>::type i = nodes->template get<nameT>().find(node_name);
		if(i == nodes->template get<nameT>().end()) {
			int proposedNewId = nodes->size();
			std::pair<typename nodeWithName_set<T>::t::iterator, bool> insertionResult = nodes->insert(nodeWithName(proposedNewId, node_name));
			assert(proposedNewId == insertionResult.first->id        );
			assert(node_name     == insertionResult.first->string_h  );
			assert(insertionResult.second);
			return insertionResult.first->id ;
		} else {
			return i->id ;
		}
	}
	int insertRel(pair<int,int> p) {
		if(p.first > p.second)
			swap(p.first, p.second);
		assert(p.first <= p.second);
		typename T::relationship_set:: template index_iterator<nodeIdsT>::type i = relationships->template get<nodeIdsT>().find(p);
		if(i == relationships->template get<nodeIdsT>().end()) {
			int relId = relationships->size();
			std::pair<typename T::relationship_set::iterator, bool> insertionResult = relationships->insert(relationship(relId, p));
			assert(relId == insertionResult.first->relId        );
			assert(p.first       == insertionResult.first->nodeIds.first  );
			assert(p.second      == insertionResult.first->nodeIds.second  );
			assert(insertionResult.second);
			{ // add this to the neighbouring relationships object too
				typename T::neighbours_to_relationships_map::iterator i;
				{
					i = neighbouring_relationships->find(p.first);
					if(i == neighbouring_relationships->end())
						i = neighbouring_relationships->insert( make_pair(p.first, empty_set_for_neighbours) ) .first ;
					i->second.insert(relId);
				}
				{
					i = neighbouring_relationships->find(p.second);
					if(i == neighbouring_relationships->end())
						i = neighbouring_relationships->insert( make_pair(p.second, empty_set_for_neighbours) ) .first ;
					i->second.insert(relId);
				}
			}
			return relId;
		} else {
			// no need to update the neighbouring_relationships object here. The relationship was already entered
			return i->relId ;
		}
	}
};
template <>
DumbGraphRaw<MapMem>::DumbGraphRaw(const std::string &dir)
		: segment_strings     (open_or_create, (dir + "/" + STRINGS_MMAP       ).c_str() , 100000000)
		, segment_nodesAndRels(open_or_create, (dir + "/" + NODES_AND_RELS_MMAP).c_str() , 100000000)
		, segment_neigh       (open_or_create, (dir + "/" + NEIGHBOURS_MMAP    ).c_str() , 100000000)
		, empty_set_for_neighbours(segment_neigh.get_allocator<int>())
{
		nodes         = segment_nodesAndRels.find_or_construct<nodeWithName_set<MapMem>::t > ("nodeWithName_set") ( nodeWithName_set<MapMem>::t::ctor_args_list()                         , segment_nodesAndRels.get_allocator<nodeWithName>());
		relationships = segment_nodesAndRels.find_or_construct<MapMem::relationship_set> ("relationship_set") ( MapMem::relationship_set::ctor_args_list()                         , segment_nodesAndRels.get_allocator<relationship>());
	 	neighbouring_relationships
		              = segment_neigh.find_or_construct<MapMem::neighbours_to_relationships_map>("Neighbours") ( 3, boost::hash<int>(), std::equal_to<int>()  , segment_neigh.get_allocator<MapMem::valtype>());    
		strings_wrap = new StringWithId_Mic_Wrap(segment_strings);
		this->nodesRO = nodes;
		this->relationshipsRO = relationships;
		this->neighbouring_relationshipsRO = neighbouring_relationships;
		this->strings_wrapRO.reset(strings_wrap);
		this->empty_set_for_neighboursRO = &empty_set_for_neighbours;
}
template <>
DumbGraphRaw<PlainMem>::DumbGraphRaw(const std::string &dir)
{
		nodes         = new nodeWithName_set<PlainMem>::t();
		relationships = new PlainMem::relationship_set();
	 	neighbouring_relationships
		              = new PlainMem::neighbours_to_relationships_map;
		strings_wrap = new ModifiableStringArrayInPlainMemory();
		this->nodesRO = nodes;
		this->relationshipsRO = relationships;
		this->neighbouring_relationshipsRO = neighbouring_relationships;
		this->strings_wrapRO.reset(strings_wrap);
		this->empty_set_for_neighboursRO = &empty_set_for_neighbours;
}


/*
 * The funcs to read the text and load the object ...
 */

template <class T>
ReadableShmGraphTemplate<T> * loadEdgeList(const char *graphTextFileName, const char *directory = 0) {
	DumbGraphRaw<T> *nodes_and_rels_wrap = NULL;
	if(T::isMapMem) {
		assert(directory && strlen(directory)>0);
		std::string dir(directory);
		{
			if(dir.length()==0)
				dir="."; // current directory
			else // remove any / on the end of it
				if(*dir.rbegin() == '/')
					dir.erase(dir.length()-1,1);
		}

		if(!graphTextFileName) {
			return DumbGraphReadONLYTemplate<T>::New(dir);
		}
		nodes_and_rels_wrap = new DumbGraphRaw<T>(dir);
	} else {
		assert(!directory);
		assert(graphTextFileName);

		nodes_and_rels_wrap = new DumbGraphRaw<T>("");
	}
	assert(nodes_and_rels_wrap);

		// PP(nodes_and_rels_wrap->strings_wrap->size());

		// PP(nodes_and_rels_wrap->numNodes());
		// PP(nodes_and_rels_wrap->numNodesWithAtLeastOneRel());
		// PP(nodes_and_rels_wrap->numRels());

		ifstream graphTextStream(graphTextFileName); 
		if(!graphTextStream.is_open())
			throw fileDoesNotExistException();
		forEach(const std::string &s, amd::rangeOverStream(graphTextStream)) {
			istringstream oneLine(s);
			amd::RangeOverStream fields(oneLine, " \t");
			!fields.empty() || ({ throw invalideGraphFileFormatException(); 0; });
			std::string l = fields.front();
			fields.popFront();
			!fields.empty() || ({ throw invalideGraphFileFormatException(); 0; });
			std::string r = fields.front();
			// cout << l << '\t' << r << endl;
			pair<int,int> edgeAsIds = make_pair(
					nodes_and_rels_wrap->insertNode(nodes_and_rels_wrap->strings_wrap->insert(l))
					,nodes_and_rels_wrap->insertNode(nodes_and_rels_wrap->strings_wrap->insert(r))
				);
			nodes_and_rels_wrap->insertRel(edgeAsIds);
		}

	return nodes_and_rels_wrap;
}

template 
ReadableShmGraphTemplate<MapMem> * loadEdgeList(const char *graphTextFileName, const char * directory = 0);
template 
ReadableShmGraphTemplate<PlainMem> * loadEdgeList(const char *graphTextFileName, const char * directory = 0);

} // namespace shmGraphRaw {
