#include "IgraphTrace.h"
#include "igraph.h"

IgraphTrace::~IgraphTrace(void)
{
}

void IgraphTrace::layoutGraph(const char *infile, const char *outfile)
{
	igraph_t graph;
	igraph_vector_t edges;
	igraph_matrix_t m;

	// Initialize the edges
	igraph_vector_init(&edges, edgeMap.size());

	// Initialize the matrix
	igraph_matrix_init(&m, 0, 0);

	// Add all the edges to the edges vector
	for (edgeMap_t::const_iterator it = edgeMap.begin() ; it != edgeMap.end() ; it++)
	{
		VECTOR(edges)[bblMap[it->second.src].num] = bblMap[it->second.dst].num;
	}

	igraph_create(&graph, &edges, 0, true);

	// Do the processing here
	uint64_t numVerts = bblMap.size();

	igraph_layout_fruchterman_reingold(&graph, &m, 500, numVerts, numVerts * numVerts, 1.5, numVerts * numVerts * numVerts, 1, 0); 

	for (long int i = 0 ; i < igraph_matrix_size(&m) ; i++)
	{
		uint32_t x = igraph_matrix_e(&m, i, 0);
		uint32_t y = igraph_matrix_e(&m, i, 1);

		bblMap[i].x = x;
		bblMap[i].y = y;
	}

	igraph_vector_destroy(&edges);
	igraph_destroy(&graph);

	this->writeGmlFile(outfile);

}