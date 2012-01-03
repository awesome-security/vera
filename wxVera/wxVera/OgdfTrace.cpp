#include "OgdfTrace.h"
#include "ogdf/energybased/FMMMLayout.h"

OgdfTrace::~OgdfTrace()
{
	// Stub function
}

void OgdfTrace::layoutGraph(const char *infile, const char *outfile)
{
	if (!infile)
		throw "Specify input file";


	if (!outfile)
		throw "Specify output file";

	using namespace ogdf;
	
	Graph G;
	GraphAttributes GA(G,
			   GraphAttributes::nodeGraphics |
			   GraphAttributes::edgeGraphics |
			   GraphAttributes::nodeLabel |
			   GraphAttributes::edgeStyle |
			   GraphAttributes::nodeColor
		);

	if( !GA.readGML(G, infile) )
	{
		throw "Could not read GML file\n";
	}
	
	FMMMLayout *fmmm = new FMMMLayout();

	if (fmmm == NULL)
		throw "Could not allocate memory";
	
	fmmm->useHighLevelOptions(true);
	fmmm->unitEdgeLength(300.0);
	fmmm->newInitialPlacement(true);
	fmmm->qualityVersusSpeed(FMMMLayout::qvsGorgeousAndEfficient);
	
	fmmm->call(GA);
	GA.writeGML(outfile);

	delete fmmm;
}