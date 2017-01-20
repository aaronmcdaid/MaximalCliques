SHELL=bash
.PHONY: gitstatus.txt help clean
BITS=
CC=g++
#BITS=-m32
#BITS=-m64

all: justTheCliques cp5

clean:
	-rm tags justTheCliques cp5 *.o */*.o

tags:
	ctags *.[ch]pp


					#-Wclobbered   -Wempty-body   \ -Wignored-qualifiers  -Woverride-init   \ -Wtype-limits   -Wunused-but-set-parameter 
# I'm including most of the -Wextra flags, but I want rid of the enum-in-conditional warning from boost
PROFILE= -O3 -std=gnu++98 #-ggdb -pg #-DNDEBUG # http://linux.die.net/man/1/gcc
CFLAGS=   \
          -Wmissing-field-initializers   \
          -Wsign-compare   \
          -Wunused-parameter    \
          -Wunused             \
          -Wnon-virtual-dtor \
          -Wall -Wformat -Werror ${PROFILE} -Wextra # -Wconversion
          #-Wuninitialized   \



#CXXFLAGS= ${BITS}     -g
LDFLAGS+= ${PROFILE}
#CXXFLAGS= ${BITS} -O3 -p -pg ${CFLAGS} # -DNDEBUG
CXXFLAGS= ${BITS}      ${CFLAGS} # -DNDEBUG
#CXXFLAGS=              -O2                 

justTheCliques: justTheCliques.o cliques.o graph/weights.o graph/loading.o graph/network.o graph/saving.o graph/graph.o graph/bloom.o graph/stats.o cmdline.o
cp5:            cp5.o comments.o cliques.o graph/weights.o graph/loading.o graph/network.o graph/saving.o graph/graph.o graph/bloom.o graph/stats.o cmdline-cp5.o clustering/components.o
mscp:            mscp.o comments.o      graph/weights.o graph/loading.o graph/network.o                graph/graph.o graph/bloom.o graph/stats.o cmdline-mscp.o



cmdline.c.FORCE:      
	# remake cmdline.c . But it's OK unless you change the .ggo file. You'll need gengetopt(1) to be able to run this.
	gengetopt  --unamed-opts < cmdline.ggo
cmdline.o: cmdline.c
	gcc -Wall cmdline.c -o cmdline.o -c

cmdline-%.c.FORCE:
	# remake cmdline-%.c . But it's OK unless you change the .ggo file. You'll need gengetopt(1) to be able to run this.
	gengetopt  --unamed-opts -F cmdline-$* < cmdline-$*.ggo
cmdline-%.o: cmdline-%.c
	gcc -Wall cmdline-$*.c -o cmdline-$*.o -c

comments.o: comments.cc.FORCE
	g++ -c comments.cc -o comments.o

comments.cc.FORCE:
	@echo '#include "comments.hh"' > comments.cc
	@echo "const char commentSlashes[] =" >> comments.cc
	@echo -n '"/''//git: ' >> comments.cc
	@{ git log || echo commit "??????"; } | egrep commit | head -n1 | cut -b8-13 | tr -d '\n'  >> comments.cc
	@echo '" "\n"' >> comments.cc
	@egrep '/'// *.?pp Makefile -h | sed -re 's/.*(\/\/\/.*)$$/"\1" "\\n"/' >> comments.cc
	@echo ";" >> comments.cc

