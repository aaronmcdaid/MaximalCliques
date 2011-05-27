#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <vector>

namespace amd {
void create_directory(const std::string& directory) throw();

struct ConnectedComponents {
	int C; // the number of cliques
	std::vector<int> component;
	std::vector<int> next;
	std::vector<int> prev;
	std::vector<int> sizes;
	ConnectedComponents();
	void setNumCliques(int _C);
	bool joinNodesIntoSameComponent(int cl1, int cl2);
};

} // namespace amd
