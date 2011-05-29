#ifndef _CLIQUES_HPP_
#define _CLIQUES_HPP_

#include <list>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#include <cerrno>
#include <stdexcept>

#include "Range.hpp"
#include "aaron_utils.hpp"
#include "graph/network.hpp"

using namespace std;

typedef const graph :: NetworkInt32 * SimpleIntGraph;

namespace cliques {

void cliquesToStdout          (SimpleIntGraph, unsigned int minimumSize); // You're not allowed to ask for the 2-cliques

} // namespace cliques


#endif
