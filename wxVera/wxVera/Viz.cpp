#include "wxvera.h"
#include "Viz.h"

VeraPane::~VeraPane(void)
{
	// Cleanup the nodes and edges
	DeleteNodes();
	DeleteEdges();

	// Clean up the freefont information
	freefont_label.clean();
	freefont_startlabel.clean();
	freefont_info.clean();
}

void VeraPane::DeleteNodes(void)
{
	// Hashmaps are painful 
#ifdef _WIN32
	hash_map<int, node_t *>::iterator ii=nodeMap.begin();
#elif __GNUC__
	hash_map<int, node_t *>::iterator ii=nodeMap.begin();
#endif
	for (  ; ii != nodeMap.end() ; ii++ )
	{
		if (ii->second != NULL)
		{	
			delete ii->second;
			ii->second = NULL;
		}
	}

	nodeMap.clear();
}

void VeraPane::DeleteEdges(void)
{
	size_t size = edgeVector.size();
	for (size_t i = 0 ; i < size ; i++)
	{
		delete edgeVector[i];
		edgeVector[i] = NULL;
	}	

	edgeVector.clear();
}

// some useful events to use
void VeraPane::mouseMoved(wxMouseEvent& event) 
{
	// We need to make sure that:
	// 1.  The nodes are loaded
	// 2.  We saw the intial mouse down event (as the delay between the open dialog can
	//     trigger an unwanted movement)
	// 3. It's the left mouse button that's down
	if (nodesLoaded && sawLeftMouseDown && event.LeftIsDown())
	{
		GLdouble posX, posY, posZ;
		posX = posY = posZ = 0.0;

		wxPoint point = event.GetPosition();

		// Get the point in world space so the screen movement matches the mouse
		getGLCoords(point, &posX, &posY, &posZ); 

		// Set the x translation amount
		if (point.x < lastMousePos.x)
		{
			tx -= (lastMouseX - posX);
			
		}
		else 
		{
			tx += (posX - lastMouseX);
		}

		// Set the y translation 
		if (point.y < lastMousePos.y)
		{
			ty -= (lastMouseY - posY);
		}
		else 
		{
			ty += (posY - lastMouseY);
		}

		lastMousePos.x = point.x;
		lastMousePos.y = point.y;

		lastMouseX = posX;
		lastMouseY = posY;
		lastMouseZ = posZ;
		
		DrawAndRender();
	}
}

void VeraPane::resetView(void)
{
	tx = 0.0;
	ty = 0.0;
	zoom = MIN_ZOOM;

	DrawAndRender();
}

void VeraPane::leftMouseDown(wxMouseEvent& event) 
{
	if (nodesLoaded)
	{
		wxPoint point = event.GetPosition();
		getGLCoords(point, &lastMouseX, &lastMouseY, &lastMouseZ);
		lastMousePos.x = point.x;
		lastMousePos.y = point.y;
		sawLeftMouseDown = true;
	}
}

void VeraPane::goToPoint(GLdouble x, GLdouble y, GLfloat zoom)
{

	// Need to set: tx, ty, and zoom
	this->tx = midX - x;
	this->ty = midY - y;
	this->zoom = zoom;

	DrawAndRender();
}

void VeraPane::zoomControl(bool doZoomIn)
{
	if (doZoomIn == true)
	{
		zoom += ZOOM_STEPPING;
		if (zoom > MAX_ZOOM)
			zoom = MAX_ZOOM;
	}
	else
	{
		zoom -= ZOOM_STEPPING;
		if (zoom < MIN_ZOOM)
			zoom = MIN_ZOOM;
	}
}

void VeraPane::mouseWheelMoved(wxMouseEvent& event) 
{
	int direction = event.GetWheelRotation();

	if (direction > 0)
	{
		zoomControl(true);
	}
	else
	{
		zoomControl(false);
	}

	DrawAndRender();
}
void VeraPane::mouseReleased(wxMouseEvent& event) 
{
	if (event.LeftUp())
	{
		lastMousePos.x = lastMousePos.y = 0;
		lastMouseX = lastMouseY = lastMouseZ = 0.0;
		sawLeftMouseDown = false;
	}
}
void VeraPane::rightMouseDown(wxMouseEvent& event) 
{
	if (nodesLoaded)
	{
		int n[10] = {0};

		int numhits = ProcessSelection(event.GetPosition(), n, sizeof(n)/sizeof(int), false);

		if (numhits == 1)
		{
			char msg[MAX_IDA_SERVER_MSG_SIZE] = {0};
			wxLogDebug(wxT("Found node: %s"), nodeMap[n[0]]->label);
			snprintf(msg, sizeof(msg), NAV_CMD " %s\n", nodeMap[n[0]]->label);

			if (parentVizFrame->sendIdaMsg(msg) != true)
				wxLogDebug(wxT("Navigate message failed, server not connected."));
		}
	}
}

// These are here just for fun.
void VeraPane::mouseLeftWindow(wxMouseEvent& event) {}
void VeraPane::keyPressed(wxKeyEvent& event) 
{
	if (!nodesLoaded)
		return;

	int key = event.GetKeyCode();
	if (key == '=' || key == '+')
		// zoom in
	{
		zoomControl(true);
	}
	else if (key == '-')
	{
		zoomControl(false);
	}

	DrawAndRender();

}
void VeraPane::keyReleased(wxKeyEvent& event) {}
 
VeraPane::VeraPane(wxWindow* parent, int* args, VizFrame *parentVizFrame) :
wxGLCanvas(parent, wxID_ANY,  wxDefaultPosition, wxDefaultSize, 0, wxT("GLCanvas"),  args)
{
    int argc = 1;
    char* argv[1] = { wxString((wxTheApp->argv)[0]).char_str() };
 
	this->parentVizFrame = parentVizFrame;

	maxX = maxY = FLT_MIN;
	midX = midY = 0.0;
	minX = minY = FLT_MAX;
	zoom = MIN_ZOOM;
	tx = 0.0;
	ty = 0.0;
	nodesLoaded = false;
	doFontRendering = true;
	lastMouseX = lastMouseY = lastMouseZ = 0.0;
	lastMousePos.x = lastMousePos.y = 0;
	gettingSelection = false;
	fontsInitialized = false;
	sawLeftMouseDown = false;
}
 
void VeraPane::resized(wxSizeEvent& evt)
{
    wxGLCanvas::OnSize(evt);
	
    Refresh();
}
 
void VeraPane::prepare3DViewport(int topleft_x, int topleft_y, int bottomright_x, int bottomright_y)
{
    /*
     *  Inits the OpenGL viewport for drawing in 3D.
     */
	
	glShadeModel(GL_FLAT);							// Enable Smooth Shading
    glClearColor(BG_COLOR_GLF, 0.50f);				// White Background
    
    glEnable(GL_DEPTH_TEST); // Enables Depth Testing
    glDepthFunc(GL_LEQUAL); // The Type Of Depth Testing To Do
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	
    glEnable(GL_COLOR_MATERIAL);
	
    glViewport(topleft_x, topleft_y, bottomright_x-topleft_x, bottomright_y-topleft_y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	
	screenWidth = bottomright_x - topleft_x;
	screenHeight = bottomright_y - topleft_y;

	// Set the ratio so that the CLIPNEAR and CLIPFAR numbers define the correct ratio
    float ratio_w_h = (float)(bottomright_x-topleft_x)/(float)(bottomright_y-topleft_y);
    gluPerspective(FOV, ratio_w_h, CLIPNEAR, CLIPFAR);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

	// Fonts for drawing the labels on the vertices
	if (!fontsInitialized)
	{
		#ifdef __APPLE__

		// Mac, Windows, etc.
		freefont_label.init("fonts/cour.ttf", 16);
		freefont_startlabel.init("fonts/arial.ttf", 16);
		freefont_info.init("fonts/tahoma.ttf", 12);

		#elif defined __linux__
		// Something here for the Linux port
		#elif defined _WIN32
		
		freefont_label.init("\\windows\\fonts\\courbd.ttf", 16);
		freefont_startlabel.init("\\windows\\fonts\\arial.ttf", 16);
		freefont_info.init("\\windows\\fonts\\tahoma.ttf", 12);

		#endif
		fontsInitialized = true;
	}
}
 
int VeraPane::getWidth()
{
    return GetSize().x;
}
 
int VeraPane::getHeight()
{
    return GetSize().y;
}
 
void VeraPane::render( wxPaintEvent& evt )
{
    if(!IsShown()) return;
    
    wxGLCanvas::SetCurrent();
    wxPaintDC(this); // only to be used in paint events. use wxClientDC to paint outside the paint event
	
	prepare3DViewport(0,0,getWidth(), getHeight());

	// ------------- draw some 2D ----------------
    //prepare2DViewport(0,0,getWidth()/2, getHeight());

	DrawAndRender();
	
}

void VeraPane::DrawAndRender(void)
{
	DrawScene();
	glFlush();
    SwapBuffers();

}

void VeraPane::DrawScene(void)
{
	// Draw using the GL primitives
	
    glLoadIdentity();

	// Clear color and depth buffer bits
	glClear(GL_COLOR_BUFFER_BIT |  GL_DEPTH_BUFFER_BIT);

	glInitNames();
	glPushName(-1); // Generic holder for the entire graph

	// Put a rectangle  at (0,0,zoom) so the mouse tracking works correctly.
	// This is so later when we're trying to align the mouse movement we can always have 
	// a fixed point of reference.

	glPushMatrix();
	glTranslatef(0, 0, zoom);
	  glColor3f(1.0, 1.0, 1.0);
	  glRectf(-1000.0, -1000.0, 1000.0, 1000.0);
	glPopMatrix();


	if (nodesLoaded)
	{
		// Render the edges
		if(!edgeVector.empty())
		{
			int numedges = edgeVector.size();
			int numnodes = nodeMap.size();

			for(int i = 0 ; i < numedges ; i++)
			{
				// Set the position using the midpoint and the translation amount
				GLfloat sourceX = nodeMap[edgeVector[i]->source]->x - midX + tx;
				GLfloat sourceY = nodeMap[edgeVector[i]->source]->y - midY + ty;
				GLfloat targetX = nodeMap[edgeVector[i]->target]->x - midX + tx;
				GLfloat targetY = nodeMap[edgeVector[i]->target]->y - midY + ty;

				glPushMatrix();
				  glColor3f(0.0, 0.0, 0.0);
				  glLineWidth(edgeVector[i]->lineWidth);
				  glBegin(GL_LINES);
				  glVertex3f(sourceX, sourceY, zoom);
				  glVertex3f(targetX, targetY, zoom);
				  glEnd();

				  // Draw the arrow head
				  drawArrow(sourceX, sourceY, targetX, targetY, edgeVector[i]->lineWidth);

				glPopMatrix();
			}
		}

		// render the nodes
		if(!nodeMap.empty()) 
		{
#ifdef _WIN32
			hash_map<int, node_t *>::iterator ii=nodeMap.begin();
#elif defined __GNUC__
			hash_map<int, node_t *>::iterator ii=nodeMap.begin();
#endif
			int numnodes = nodeMap.size();

			for (  ; ii != nodeMap.end() ; ii++ )
			{
				node_t *node = ii->second;

				if (node == NULL)
					continue;

				// Set the position using the midpoint and the translation amount
				GLfloat xPos = node->x - midX + tx;
				GLfloat yPos = node->y - midY + ty;
				GLfloat nodeDimensions[4] = {0};

				// Draw the node boxes
				glLoadName(node->id); // Setup for the selection rectangles
				glPushMatrix();
				glColor3f(node->cr, node->cg, node->cb);
				glTranslatef(xPos, yPos, zoom);
				
				if (zoom <= MAX_ZOOM - 2*ZOOM_STEPPING)
				{
					// Calculate the stepping so the blocks are visible from a zoomed out view

					// Call attention to the start label
					if (strcmp(node->label, START_NODE_LABEL) == 0)
					{
						nodeDimensions[0] = 0.0;
						nodeDimensions[1] = 0.0;
						nodeDimensions[2] = MAX((zoom/-100000)*520.0 * 10.0, 130.0);
						nodeDimensions[3] = MAX((zoom/-100000)*200.0 * 10.0, 50.0);
					}
					else
					{
						nodeDimensions[0] = 0.0;
						nodeDimensions[1] = 0.0;
						nodeDimensions[2] = MAX((zoom/-100000)*520.0,130.0);
						nodeDimensions[3] = MAX((zoom/-100000)*200.0,50.0);

					}
				}
				else
				{
					nodeDimensions[0] = 0.0;
					nodeDimensions[1] = 0.0;
					nodeDimensions[2] = strlen(node->label) > 8 ? (14.0 * strlen(node->label)) : 130.0; // I just messed around until these numbers looked good
					nodeDimensions[3] = 50.0;
				}

				glRectf(nodeDimensions[0], nodeDimensions[1], nodeDimensions[2], nodeDimensions[3]);

				glTranslatef(0.0, 0.0, 1.0);
				glPopMatrix();

			}

			// Draw the visible text nodes
			if(!gettingSelection && doFontRendering && zoom > MAX_ZOOM - 2 * ZOOM_STEPPING)
			{
				int *items = (int *) malloc(sizeof(int) * (numnodes+1));
				
				if (items == NULL)
				{
					wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
					return;
				}
				
				memset(items, -1, sizeof(int) * (numnodes+1));

				// Only draw the number of nodes that are actually visible on the screen
				int numberVisibleNodes = GetScreenItems(items, numnodes+1);

				for (int j = 0 ; j < numberVisibleNodes ; j++)
				{
					int i = items[j];
					GLfloat xPos = nodeMap[i]->x - midX + tx;
					GLfloat yPos = nodeMap[i]->y - midY + ty;
					
					glPushMatrix();
					
					glTranslatef(xPos, yPos, zoom-.1);
					
					// Handle the starting indicator
					if(strcmp(nodeMap[i]->label, START_NODE_LABEL) == 0) 
					{
						glColor3f(START_COLOR_GLF);
						freetype::print(freefont_startlabel, 0, 20, false, "START", cnt1);
					}
					else
					{
						// Normal addresses
						glColor3f(TEXT_COLOR_GLF);
						freetype::print(freefont_label, 0, 20, false, nodeMap[i]->label, cnt1);
					}
					
					glPopMatrix();
				}

				free(items);
			} // End node drawing
		}
	}
	//else // No nodes are loaded
	//{
	//	glPushMatrix();
	//	glTranslatef(0, 0, -1000);
	//	glColor3f(TEXT_COLOR_GLF);
	//	freetype::print(freefont_startlabel, -100, 0, false, "Welcome to VERA v" __VERA_VERSION__, cnt1);
	//	freetype::print(freefont_startlabel, -110, -30, false, "Open a file to get started", cnt1);
	//	glPopMatrix();
	//}

}

int VeraPane::GetScreenItems(int *items, size_t maxItems)
{
	wxPoint point;
	point.x = 0; 
	point.y = 0;
	return ProcessSelection(point, items, maxItems, true);
}

int VeraPane::ProcessSelection(wxPoint point, int *items, size_t maxItems, bool useScreenPickSize)
{
	int xPos = point.x, yPos = point.y;
	static GLuint selectBuff[SELECT_BUFF_LEN] = {0};
	GLint hits = 0, viewport[4] = {0};
	size_t numItems = 0;

	// Get the viewport
	glGetIntegerv(GL_VIEWPORT, viewport);

	// Setup the selection buffer
	glSelectBuffer(SELECT_BUFF_LEN, selectBuff);
	
	// Change the render mode
	glRenderMode(GL_SELECT);

	// Switch to projection and save the matrix
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	
	//gluPickMatrix(xPos, viewport[3] - yPos + viewport[1], 1.0f, 1.0f, viewport);
	if (useScreenPickSize)
	{
		gluPickMatrix(viewport[2] / 2, viewport[3] / 2, viewport[2], viewport[3], viewport);
		gluPerspective(FOV, ASPECT(viewport[2], viewport[3]), CLIPNEAR, CLIPFAR);
	}
	else
	{
		// Establish a new clipping volume +/- 2 pixels around the coordinate
		gluPickMatrix(xPos, viewport[3] - yPos, 2, 2, viewport);
		gluPerspective(FOV, ASPECT(viewport[2]-viewport[0], viewport[3]-viewport[1]), CLIPNEAR, CLIPFAR);		
	}
	
	// Set the perspective to be the range of the viewport

	glMatrixMode(GL_MODELVIEW);
	
	gettingSelection = true;
	// Draw the scene
	DrawScene();
	gettingSelection = false;

	// Collect the hits
	hits = glRenderMode(GL_SELECT);

	//if (hits == -1)
	//{
	//	TRACE("Something screwed up with selection buffer (hits = -1)\n");
	//}

	// If a single hit occurred, do something with it
	for (GLint i = 0 ; i < hits ; i++)
	{
		// selectBuff looks like this:
		// selectBuff[0] Number of selections in this hit
		// ...       [1] Min depth of the selections
		// ...       [2] Max depth of the selections
		// ...       [3] Name of the selection (this is our index into the list
		// I assume that there's only going to be one name per hit mostly out of optimism.

		// A [3] value of 0xFFFFFFFF means it's outside the range of our graph
		if (selectBuff[(i*4)+3] == 0xFFFFFFFF)
			continue;

		if (items != NULL && numItems < maxItems && (int) numItems < nodeMap.size())
		{
			items[numItems++] = selectBuff[(i*4)+3];
		}
		//TRACE("%5i: Got a hit on %s\n", i, nodes[selectBuff[(i*4)+3]].label);
	}

	glRenderMode(GL_RENDER);

	//Restore the projection matrix
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	// Go back to the modelview for normal rendering
	glMatrixMode(GL_MODELVIEW);

	return numItems;

}

// This should only be used to open a GML file
bool VeraPane::openFile(wxString filename)
{
	char line[256] = {0};

	bool inNode = false;
	bool inEdge = false;
	bool inGraphics = false;

	FILE *fin = NULL;
	
	fin = fopen(filename.ToAscii(), "r");

	if (NULL == fin)
		return false;
	
	// Node stuff
	GLfloat x = 0.0;
	GLfloat y = 0.0;
	int id = -1;
	char label[LABEL_LEN] = {0};
	float cr = -1.0, cg = -1.0, cb = -1.0;
	float labelcr = -1.0, labelcg = -1.0, labelcb = -1.0;

	// Graph stuff
	int source = -1;
	int target = -1;
	GLfloat lineWidth = 0.0;

	// Check to see if a graph has been loaded already
	if (!edgeVector.empty() || !nodeMap.empty())
	{
		DeleteNodes();
		DeleteEdges();
		midX = midY = maxX = maxY = 0.0;
	}

	while(fgets(line, sizeof(line)-1, fin) != NULL) // main line reading loop
	{
		// Check to see if we're at the edges section of the file
		if(strstr(line, "node ["))
		{
			inNode = true;
			inEdge = false;
		}
		else if (strstr(line, "edge ["))
		{
			inNode = false;
			inEdge = true;
		}

		if(inNode) {
			if(strstr(line, "x ")) // Read the x coordinate
				sscanf(line, "x %f", &x);
			else if (strstr(line, "y ")) // Read the y coordinate
				sscanf(line, "y %f", &y);
			else if (strstr(line, "fill ")) // Get the node fill color
			{
				size_t i = 0;
				size_t len = strlen(line);
				unsigned int x = 0;
				bool seenHash = false;
				char str[10] ={0};

				for(i = 0 ; i < len; i++)
				{
					if (line[i] == '#')
					{
						i++;
						break;
					}
				}
				
				// Fill the box so we can convert the hex text to floats
				// Red
				str[0] = line[i];
				str[1] = line[i+1];
				str[2] = '\0';
				// Green
				str[3] = line[i+2];
				str[4] = line[i+3];
				str[5] = '\0';
				// Blue
				str[6] = line[i+4];
				str[7] = line[i+5];
				str[8] = '\0';
				
				xtoi(&str[0], &x);
				cr = float(x) / 255;
				xtoi(&str[3], &x);
				cg = float(x) / 255;
				xtoi(&str[6], &x);
				cb = float(x) / 255;

			}
			else if (strstr(line, "label ")) // read the label
			{
				bool inTextColor = false;
				char str[4] = {0};
				unsigned int x = 0;
				char *colorPos = strstr(line, "textColor=\\\"");
				char *labelPos = strstr(line, ">");

				// Read the textColor 
				// Red
				if (colorPos != NULL)
				{
					colorPos += 12; // Skip the textColor=\"
				
					str[0] = colorPos[0]; str[1] = colorPos[1]; str[2] = '\0';
					xtoi(str, &x);
					labelcr = float(x) / 255;

					// Green
					str[0] = colorPos[2]; str[1] = colorPos[3]; str[2] = '\0';
					xtoi(str, &x);
					labelcg = float(x) / 255;

					// Blue
					str[0] = colorPos[4]; str[1] = colorPos[5]; str[2] = '\0';
					xtoi(str, &x);
					labelcb = float(x) / 255;
				}

				if (labelPos != NULL)
				{
					labelPos++; // Skip the >

					// Read the actual label value until a '<' is seen.
					for(int i = 0; i < LABEL_LEN - 1 ; i++)
					{
						if(labelPos[i] != '<')
							label[i] = labelPos[i];
						else
						{
							label[i] = '\0';
							break;
						}
					}
				}
			}
			else if (strstr(line, "id "))
			{
				sscanf(line, "id %u", &id);
			}
			
			// We have all the node information, so store it in an array entry
			if (x != 0.0 && 
				y != 0.0 && 
				cr != -1.0 && 
				cg != -1.0 && 
				cb != -1.0 && 
				strlen(label) > 0 && 
				id != -1)
			{
				node_t *tmp = new node_t;

				if (tmp == NULL)
				{
					return false;
				}
				
				// Get the dimensions of the graph
				maxX = (x > maxX) ? x : maxX;
				maxY = (y > maxY) ? y : maxY;

				minX = (x < minX) ? x : minX;
				minY = (y < minY) ? y : minY;
				
				tmp->x = x;
				tmp->y = y;
				tmp->cr = cr;
				tmp->cg = cg;
				tmp->cb = cb;
				tmp->id = id;

				strncpy(tmp->label,
					label, 
					sizeof(tmp->label) - 1);

				size_t len = strlen(tmp->label) < sizeof(tmp->label) ? strlen(label) : sizeof(tmp->label);
				TOLOWER(tmp->label, len);

				nodeMap[id] = tmp;

				// Store the id in a hash map
				string key = tmp->label;
				nodeHashMap[key] = tmp;
				
				// Clear out the variables
				x = 0.0;
				y = 0.0;
				cr = cg = cb = -1.0;
				id = -1;
				memset(label, 0, sizeof(label));
				inNode = false;
				tmp	= NULL;
			}
		} // end if(inNode) 
		else if (inEdge)
		{
			if(strstr(line, "source "))
			{
				sscanf(line, "source %d", &source);
			}
			else if (strstr(line, "target "))
			{
				sscanf(line, "target %d", &target);
			}
			else if (strstr(line, "lineWidth "))
			{
				sscanf(line, "lineWidth %f", &lineWidth);
			}

			// Found everything, now create it
			if (source >= 0 && target >= 0 && lineWidth != 0.0)
			{
				edge_t *tmp = new edge_t;

				if (tmp == NULL)
				{
					return false;
				}

				tmp->source = source;
				tmp->target = target;
				tmp->lineWidth = lineWidth;

				edgeVector.push_back(tmp);

				source = target = 0;
				lineWidth = 0.0;
				inEdge = false;
				tmp = NULL;
				
			}
		} // end if(inEdge)
		
	}
	
	midX = (maxX - minX) / 2;
	midY = (maxY - minY) / 2;
	
	fclose(fin);

	// Validate the file contents, present error if there's a problem
	if (edgeVector.empty() || nodeMap.empty() )
		return false;

	nodesLoaded = true;
	resetView();

	return true;
}

/**
 * This code adapted from http://www.codeproject.com/KB/GDI/arrows.aspx
 **/
void VeraPane::drawArrow(float sourceX, float sourceY, float targetX, float targetY, float width)
{
	float vecLine[2];
	float vecLeft[2];
	float baseX, baseY;
	float fLength;
	float th;
	float ta;
	float arrowWidth = 0.0;

	if (MIN_ARROW_WIDTH > width)
		arrowWidth = MIN_ARROW_WIDTH;
	else
		arrowWidth = width * 2;

	vecLine[0] = targetX - sourceX;
	vecLine[1] = targetY - sourceY;

	vecLeft[0] = -vecLine[1];
	vecLeft[1] = vecLine[0];

	fLength = (float) sqrt(vecLine[0] * vecLine[0] + vecLine[1] * vecLine[1]);

	th = arrowWidth / (2.0f * fLength);
	ta = arrowWidth / (2.0f * (tanf(DEG2RAD(ARROW_THETA)) / 2.0f) * fLength);

	baseX = (targetX + -ta * vecLine[0]);
	baseY = (targetY + -ta * vecLine[1]);

	glBegin(GL_POLYGON);
	glVertex3f(targetX, targetY, zoom);
	glVertex3f(
		baseX + th * vecLeft[0],
		baseY + th * vecLeft[1],
		zoom);
	glVertex3f(
		baseX + -th * vecLeft[0],
		baseY + -th * vecLeft[1],
		zoom);
	glEnd();
}

void VeraPane::getGLCoords(wxPoint point, GLdouble *posX, GLdouble *posY, GLdouble *posZ)
{
	GLint viewport[4];
	GLdouble modelview[16];
	GLdouble projection[16];
	GLfloat winX, winY, winZ;
	//GLdouble posX, posY, posZ;

	glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
	glGetDoublev(GL_PROJECTION_MATRIX, projection);
	glGetIntegerv(GL_VIEWPORT, viewport);

	winX = (float)point.x;
	winY = (float)viewport[3] - point.y;

	// Read a location in the center of the screen. I draw a 100x100 rectangle
	// that is always at the correct zoom factor. This makes the screen movement
	// match the mouse movement
	
	//glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winZ );
	glReadPixels(screenWidth/2, screenHeight/2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winZ);
	
	gluUnProject(winX, winY, winZ, modelview, projection, viewport, posX, posY, posZ);
}

// Convert the string into the format the hashmap is expecting
// The normalized form is a lower-case, 4 byte hex representation of
// the data (e.g. 0040bb08 or 00104000
string normalizeNodeString(string str)
{
	char		label[LABEL_LEN]	= {0};
	uint32_t	label_val			= -1;

	strncpy(label, str.c_str(), MIN(sizeof(label) - 1, str.length()));

	// There's probably a C++ way to do this
	// This also truncates the labels too
	// This will cause problems in a 64-bit version

	sscanf(label, "%x", &label_val);
	sprintf(label, "%8.8x", label_val);

	return string(label);
}

// Search the list of nodes for a specific string we're looking for
node_t * VeraPane::searchByString (string searchString)
{
	node_t *ret = NULL;
	char searchStr[LABEL_LEN + 1] = {0};
	string normalizedString;

	// Too short of a string
	if (searchString.length() == 0)
		return NULL;

	// Too long of a string
	if (searchString.length() > LABEL_LEN)
		return NULL;

	// Make sure the string is actually a hex string
	if ( ! isHexString(searchString.c_str(), searchString.length()) )
	{
		return NULL;
	}

	// Normalize the string so it looks like what's in the nodeHashMap
	normalizedString = normalizeNodeString(searchString);

	// Now try to find in the nodeMap

	ret = this->nodeHashMap[normalizedString];

	return ret;
}






















