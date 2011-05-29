#ifndef _CLIQUES_HPP_
#define _CLIQUES_HPP_


#include "graph/network.hpp"

typedef const graph :: VerySimpleGraphInterface * SimpleIntGraph;

namespace cliques {

void cliquesToStdout          (const graph :: NetworkInterfaceConvertedToString * net, unsigned int minimumSize); // You're not allowed to ask for the 2-cliques

} // namespace cliques


#endif
