#include <ios>
#include "graph_utils.hpp"
#include "assert.h"

using namespace std;

namespace amd {
void create_directory(const std::string& directory) throw() {
	errno=0;
	mkdir(directory.c_str(), S_IRWXU|S_IRWXG);

	if(errno && errno!=EEXIST)
		throw std::ios_base::failure("Attemping to create directory:" + directory);
	
}
	ConnectedComponents::ConnectedComponents() : C(-1) {}
	void ConnectedComponents::setNumCliques(int _C) {
		assert(this->C==-1);
		this->C = _C;
		this->component.resize(C);
		this->next.resize(C);
		this->prev.resize(C);
		this->sizes.resize(C,1);
		assert(this->C>0);
		for(int i=0; i<C; i++) {
			component.at(i) = i;
			next     .at(i) = i;
			prev     .at(i) = i;
		}
	}
	bool ConnectedComponents::joinNodesIntoSameComponent(int cl1, int cl2) {
		assert(this->C>0); // This is important. This represents the uninitialized components. We don't care about k=2 or k=1
		assert(cl1 != cl2);
		const int comp1 = this->component.at(cl1);
		const int comp2 = this->component.at(cl2);
		if(comp1 == comp2)
			return false;
		{ // this'd be faster if comp2 is smaller
			if(this->sizes.at(comp1) < this->sizes.at(comp2)) {
				return this->joinNodesIntoSameComponent(cl2,cl1);
			}
		}
#ifdef checkCompSizes
		int sizeA = 0;
		int sizeB = 0;
		{
			int cl = comp1;
			do {
				assert(this->component.at(cl) == comp1);
				sizeA++;
				// cout << "clique " << cl << " is in component " << this->component.at(cl) << endl;
				assert(cl == this->prev.at(this->next.at(cl)));


				cl = this->next.at(cl);
			} while (cl != comp1);
		}
		{
			int cl = comp2;
			do {
				assert(this->component.at(cl) == comp2);
				sizeB++;
				// cout << "clique " << cl << " is in component " << this->component.at(cl) << endl;
				assert(cl == this->prev.at(this->next.at(cl)));


				cl = this->next.at(cl);
			} while (cl != comp2);
		}
			// PP(sizeA);
			// PP(sizeB);
#endif
		assert(comp1 == this->component.at(comp1));
		assert(comp2 == this->component.at(comp2));
		// abolish comp2, renaming all of its to comp1
		int size2 = 0;
		for(int cl = comp2; this->component.at(cl)==comp2; cl = this->next.at(cl) ) {
			this->component.at(cl) = comp1;
			size2++;
		}
		assert(size2 == this->sizes.at(comp2));
		this->sizes.at(comp1) += size2;
		this->sizes.at(comp2) = 0;
		assert(comp1 == this->component.at(comp2));
		const int comp1SndLast = this->prev.at(comp1);
		const int comp2SndLast = this->prev.at(comp2);
		this->next.at(this->prev.at(comp1)) = comp2;
		this->next.at(this->prev.at(comp2)) = comp1;
		// we modified the nexts in terms of the prevs
		// hence the nexts are correct, but the prevs aren't
		this->prev.at(comp1) = comp2SndLast;
		this->prev.at(comp2) = comp1SndLast;

#ifdef checkCompSizes
		{
			int cl = comp1;
			int size = 0;
			do {
				assert(this->component.at(cl) == comp1);
				size++;
				// cout << "clique " << cl << " is in component " << this->component.at(cl) << endl;
				assert(cl == this->prev.at(this->next.at(cl)));


				cl = this->next.at(cl);
			} while (cl != comp1);
			assert(size == sizeA + sizeB);
		}
#endif
		return true;
	}
} // namespace amd
