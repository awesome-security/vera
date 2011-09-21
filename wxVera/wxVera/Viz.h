#ifndef __VIZ_H__
#define __VIZ_H__

#include "wxvera.h"
#include "wx/glcanvas.h"

// include OpenGL
#ifdef __WXMAC__
#include "OpenGL/glu.h"
#include "OpenGL/gl.h"
#include "GLUT/glut.h"
#else
#include <GL/glu.h>
#include <GL/gl.h>
#include <GL/glut.h>
#endif

#include <math.h>
#include "FreeType.h"

#define PI					3.1415926

#define START_NODE_LABEL	"deadbeef"

#define DEG2RAD(x) ((x)*PI/180)
#define RAD2DEG(x) ((x)*180/PI)

#define MIN_ZOOM 			-100000.00
#define MAX_ZOOM 			-1000.0
#define ZOOM_STEPPING		2000.00
#define LABEL_LEN			256
#define LABEL_CHAR_LEN		8
#define SELECT_BUFF_LEN		10000 // This value is large to accomodate graphs with lots of nodes in them

// Frustum coordinates
#define FOV					45.0f
//#define CLIPNEAR 0.01f
#define CLIPNEAR			100.0f
#define CLIPFAR				100000.0f
#define MIN_ARROW_WIDTH		25.0f
#define ARROW_THETA			45.0f

// Some default colors
#define START_COLOR_GLF		1.0,1.1,1.0
#define TEXT_COLOR_GLF		0.0,0.0,0.0
#define BG_COLOR_GLF		1.0f,1.0f,1.0f

#define ABS(x)	((x) < 0 ? (x) *-1 : (x))
#define ASPECT(cx, cy) ((float(cx))/(float(cy)))

typedef struct Node {
	int					id;
	GLfloat 				x;
	GLfloat 				y;
	GLfloat 				cr;
	GLfloat 				cg;
	GLfloat 				cb;
	char					label[LABEL_LEN];
	GLfloat 				labelcr;
	GLfloat 				labelcg;
	GLfloat 				labelcb;
} node_t;

typedef struct Edge {
	int 					source;
	int 					target;
	GLfloat					lineWidth;
} edge_t;

#ifdef __GNUC__
/** BEGIN FIX **/

namespace __gnu_cxx
{
	template<> struct hash< std::string >                                                       
	{                                                                                           
		size_t operator()( const std::string& x ) const                                           
		{                                                                                         
			return hash< const char* >()( x.c_str() );                                              
		}                                                                                         
	};                                                                                          
}
/** END FIX **/
#endif

// Function prototypes
string normalizeNodeString(string str);

// Class definition
class VizFrame;

class VeraPane : public wxGLCanvas
{
	friend class VizFrame;
public:
	VeraPane				(wxWindow* parent, int* args, VizFrame *parentVizFrame);
	~VeraPane				(void);
    
	int  getWidth();
	int  getHeight();
    
	void prepare3DViewport	(int topleft_x, int topleft_y, int bottomright_x, int bottomright_y);
	void prepare2DViewport	(int topleft_x, int topleft_y, int bottomright_x, int bottomright_y);

	bool openFile			(wxString filename);
    
	// events
	void render				(wxPaintEvent& evt);
	void resized			(wxSizeEvent& evt);
	void mouseMoved			(wxMouseEvent& event);
	void leftMouseDown		(wxMouseEvent& event);
	void mouseWheelMoved	(wxMouseEvent& event);
	void mouseReleased		(wxMouseEvent& event);
	void rightMouseDown		(wxMouseEvent& event);
	void mouseLeftWindow	(wxMouseEvent& event);
	void keyPressed			(wxKeyEvent& event);
	void keyReleased		(wxKeyEvent& event);
	node_t * searchByString (string searchString);

private:
	void DrawScene			(void);
	void DrawAndRender		(void);
	void drawArrow			(float sourceX, float sourceY, float targetX, float targetY, float width);
	int	 GetScreenItems		(int *items, size_t maxItems);
	int  ProcessSelection	(wxPoint point, int *items, size_t maxItems, bool useScreenPickSize);
	void getGLCoords		(wxPoint point, GLdouble *posX, GLdouble *posY, GLdouble *posZ);
	void DeleteNodes		(void);
	void DeleteEdges		(void);
	void zoomControl		(bool doZoomIn);
	
#ifdef _WIN32 // Windows
	stdext::hash_map<int, node_t *>	nodeMap;
	stdext::hash_map<string, node_t *> nodeHashMap;
#elif defined __GNUC__
	hash_map<int, node_t *>	nodeMap;
	hash_map<string, node_t *> nodeHashMap;
#endif

	vector<edge_t *>			        edgeVector;
	GLfloat					maxX, maxY, midX, midY, minX, minY;
	GLfloat 				zoom;
	GLfloat 				tx;
	GLfloat 				ty;
	GLfloat 				theta;
	GLfloat 				distance;
	wxPoint 				lastMousePos;
	GLdouble 				lastMouseX;
	GLdouble 				lastMouseY;
	GLdouble 				lastMouseZ;
	int 					screenWidth;
	int 					screenHeight;
	bool 					nodesLoaded;
	bool 					doFontRendering;
	bool 					gettingSelection;
	bool 					fontsInitialized;
	bool 					sawLeftMouseDown;
	VizFrame *				parentVizFrame;

	freetype::font_data 	freefont_label;
	freetype::font_data 	freefont_startlabel;
	freetype::font_data 	freefont_info;

	//GLYPHMETRICSFLOAT		gmf[256];	// Storage For Information About Our Outline Font Characters
	GLuint					base;		// Base Display List For The Font Set
	GLfloat					cnt1;		// 1st Counter Used To Move Text & For Coloring
	GLfloat					cnt2;		// 2nd Counter Used To Move Text & For Coloring

	DECLARE_EVENT_TABLE()
};

#endif
