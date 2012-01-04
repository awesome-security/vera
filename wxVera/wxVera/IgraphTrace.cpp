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

	int numEdges = edgeMap.size();
	uint64_t numVerts = bblMap.size();

	// create an array with the vertices indexed by the num (id)
	uint32_t *num2addr = (uint32_t *) malloc(sizeof(uint32_t) * numVerts);
	
	if (num2addr == NULL)
		throw "Couldn't allocate memory in igraphtrace::layoutGraph";

	memset(num2addr, 0, sizeof(uint32_t) * numVerts);

	// Copy the num and address to the new array
	for (bblMap_t::const_iterator it = bblMap.begin(); it != bblMap.end() ; it++)
	{
		trace_address_t addr = it->second;
		num2addr[addr.num] = addr.addr;
	}

	// Initialize the edges
	igraph_vector_init(&edges, numEdges);

	// Initialize the matrix
	igraph_matrix_init(&m, 0, 0);

	// Add all the edges to the edges vector
	for (edgeMap_t::const_iterator it = edgeMap.begin() ; it != edgeMap.end() ; it++)
	{
		trace_edge_t 		edge = it->second;
		uint32_t			src_addr = it->second.src;
		uint32_t			dst_addr = it->second.dst;
		trace_address_t 	src  = bblMap[src_addr];
		trace_address_t     dst  = bblMap[dst_addr];

		num2addr[src.num] = src.addr;
		num2addr[dst.num] = dst.addr;
		VECTOR(edges)[src.num] = dst.num;
	}


	igraph_create(&graph, &edges, 0, true);

	// Do the processing here

	igraph_layout_lgl(&graph, &m, 10, numVerts, numVerts * numVerts, 1.5, numVerts * numVerts, 1, 0); 

	long matrix_colsize = igraph_matrix_ncol(&m);
	long matrix_size = igraph_matrix_size(&m) / 2;
	for (long int i = 0 ; i < matrix_size; i++)
	{
		uint32_t x = igraph_matrix_e(&m, i, 0);
		uint32_t y = igraph_matrix_e(&m, i, 1);

		bblMap[num2addr[i]].x = x;
		bblMap[num2addr[i]].y = y;
	}

	igraph_vector_destroy(&edges);
	igraph_destroy(&graph);

	free(num2addr);

	this->writeGmlFile(outfile);

}