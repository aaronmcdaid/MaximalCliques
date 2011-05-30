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
PROFILE= -O3 #-ggdb -pg #-DNDEBUG # http://linux.die.net/man/1/gcc
CFLAGS=   \
          -Wmissing-field-initializers   \
          -Wsign-compare   \
          -Wunused-parameter    \
          -Wunused             \
          -Wnon-virtual-dtor \
          -Wall -Wformat -Werror ${PROFILE} -Wextra -Wconversion
          #-Wuninitialized   \



#CXXFLAGS= ${BITS}     -g
LDFLAGS+= ${PROFILE}
#CXXFLAGS= ${BITS} -O3 -p -pg ${CFLAGS} # -DNDEBUG
CXXFLAGS= ${BITS}      ${CFLAGS} # -DNDEBUG
#CXXFLAGS=              -O2                 

justTheCliques: justTheCliques.o cliques.o graph/weights.o graph/loading.o graph/network.o graph/saving.o graph/graph.o graph/bloom.o cmdline.o



cmdline.c:      
	# remake cmdline.c . But it's OK unless you change the .ggo file. You'll need gengetopt(1) to be able to run this.
	gengetopt  --unamed-opts < cmdline.ggo
cmdline.o: cmdline.c
	gcc -Wall cmdline.c -o cmdline.o -c
