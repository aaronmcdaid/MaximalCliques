.PHONY: gitstatus.txt help clean
BITS=
CC=g++
#BITS=-m32
#BITS=-m64

all: justTheCliques

clean:
	-rm tags justTheCliques *.o */*.o

tags:
	ctags *.[ch]pp


					#-Wclobbered   -Wempty-body   \ -Wignored-qualifiers  -Woverride-init   \ -Wtype-limits   -Wunused-but-set-parameter 
# I'm including most of the -Wextra flags, but I want rid of the enum-in-conditional warning from boost
PROFILE= -O3 #-p -pg #-DNDEBUG
CFLAGS=   \
          -Wmissing-field-initializers   \
          -Wsign-compare   \
          -Wunused-parameter    \
          -Wunused             \
          -Wnon-virtual-dtor \
          -Wall -Wformat -Werror ${PROFILE}
          #-Wuninitialized   \



#CXXFLAGS= ${BITS}     -g
LDFLAGS+= ${PROFILE}
#CXXFLAGS= ${BITS} -O3 -p -pg ${CFLAGS} # -DNDEBUG
CXXFLAGS= ${BITS}      ${CFLAGS} # -DNDEBUG
#CXXFLAGS=              -O2                 

justTheCliques: justTheCliques.o Range.o cliques.o graph/weights.o graph/loading.o graph/network.o graph/saving.o graph/graph.o graph/bloom.o
