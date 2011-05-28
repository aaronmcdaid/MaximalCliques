.PHONY: gitstatus.txt help clean
BITS=
#BITS=-m32
#BITS=-m64

all: boost_1_41_0 justTheCliques

clean:
	-rm tags justTheCliques *.o */*.o

tags:
	ctags *.[ch]pp


					#-Wclobbered   -Wempty-body   \ -Wignored-qualifiers  -Woverride-init   \ -Wtype-limits   -Wunused-but-set-parameter 
# I'm including most of the -Wextra flags, but I want rid of the enum-in-conditional warning from boost
CFLAGS=       \
          -Wmissing-field-initializers   \
          -Wsign-compare   \
          -Wuninitialized   \
          -Wunused-parameter    \
          -Wunused             \
          -Wnon-virtual-dtor \
          -Wall -Wformat -Werror -I./boost_1_41_0

boost_1_41_0:
	@echo "   " This needs Boost. It has been tested with boost 1.41 .
	@echo "   " Extract this to a folder called boost_1_41_0 . 
	@echo "   " http://sourceforge.net/projects/boost/files/boost/1.41.0/
	false


#CXXFLAGS= ${BITS}     -g
LDFLAGS+= -lstdc++ -lrt ${PROFILE}
#CXXFLAGS= ${BITS} -O3 -p -pg ${CFLAGS} # -DNDEBUG
CXXFLAGS= ${BITS} -O3        ${CFLAGS} # -DNDEBUG
#CXXFLAGS=              -O2                 

justTheCliques: shmGraphRaw.o justTheCliques.o Range.o cliques.o graph_utils.o graph/weights.o graph/strings.o graph/loading.o graph/network.o graph/saving.o graph/graph.o
