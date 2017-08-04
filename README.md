# MaximalCliques

**justTheCliques:**   Find maximal cliques, via the Bron Kerbosch algorithm, [Bron Kerbosch in Wikipedia](http://en.wikipedia.org/wiki/Bron%E2%80%93Kerbosch_algorithm)

**cp5:**     Fast clique percolation algorithm, described in [Cornell University Library](http://arxiv.org/abs/1205.0038)

## Copyright

Copyright 2009-2011 - Aaron McDaid aaronmcdaid@gmail.com.
Licensed under GPL v3. See gpl.txt included with this package.

## Compiling

First, to get your hands on the latest code from GitHub.
In a directory of your choice:

	git clone --recursive https://github.com/aaronmcdaid/MaximalCliques.git
	make clean justTheCliques cp5

## Usage


	./justTheCliques your_edge_list.txt                         [--stringIDs]    > cliques.txt
	./cp5            your_edge_list.txt output_directory        [--stringIDs]

- justTheCliques will output the cliques to standard output, hence you should
  redirect it with "> cliques.txt" as in this example.
- cp5 will create its output in a directory of your choice.

or, if you just want cliques with at least 10 nodes in them,

	./justTheCliques your_edge_list.txt -k 10  [--stringIDs]    > cliques.txt

this should be a little faster. --stringIDs is to allow strings, not just integers
in the input - see below.

The cliques themselves are printed to stdout (hence the redirection above). Various
summary stats are printed on stderr. So, if you didn't want the cliques but
did want to see the sizes of the cliques found, to

	./justTheCliques edge_list.txt       > /dev/null

## Input file

Each line of the **your_edge_list.txt** represents an edge. The first two fields
(delimited by commas, pipes(|), spaces or tabs) are the names of the two
nodes that are connected. Directionality is ignored, and self-loops will be rejected.
Any other fields on the line are also ignored.

By default, the node names are integers (64-bit integers). But you can change
this with the --stringIDs option; note this will increase memory usage.

