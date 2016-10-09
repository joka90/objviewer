// How to use:
// + - to change 'eigen value'
// rightclick to pan
// middle to zoom
//
// 
// 


#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <GL/glew.h>

#include <GLFW/glfw3.h>


#include <GL/glut.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <iomanip>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "textfile.h"

using
  std::stringstream;
using
  std::cout;
using
  std::cin;
using
  std::endl;
using
  std::ends;
using
  std::string;
using namespace
  std;


void
mouseCB (int button, int stat, int x, int y);
void
mouseMotionCB (int x, int y);
void
drawString3D (const char *str, float pos[3], float color[4], void *font);
void
showInfo ();
void
drawString (const char *str, int x, int y, float color[4], void *font);

void *
  font = GLUT_BITMAP_8_BY_13;


bool
  mouseLeftDown;
bool
  mouseRightDown;
bool
  mouseMiddleDown;
float
  mouseX,
  mouseY;
float
  cameraAngleX;
float
  cameraAngleY;
float
  cameraDistance = -3;
int
  t = 1;

GLhandleARB
  v,
  f,
  f2,
  p;

typedef struct
{
  GLuint
    vb;				// vertex buffer
  int
    numTriangles;
  size_t
    material_id;
} DrawObject;

std::vector < DrawObject > gDrawObjects;


float bmin[3], bmax[3];
std::vector < tinyobj::material_t > materials;
std::map < std::string, GLuint > textures;

float
  maxExtent;

void
CheckErrors (std::string desc)
{
  GLenum
    e = glGetError ();
  if (e != GL_NO_ERROR)
    {
      fprintf (stderr, "OpenGL error in \"%s\": %d (%d)\n", desc.c_str (), e,
	       e);
      exit (20);
    }
}

void
CalcNormal (float N[3], float v0[3], float v1[3], float v2[3])
{
  float
    v10[3];
  v10[0] = v1[0] - v0[0];
  v10[1] = v1[1] - v0[1];
  v10[2] = v1[2] - v0[2];

  float
    v20[3];
  v20[0] = v2[0] - v0[0];
  v20[1] = v2[1] - v0[1];
  v20[2] = v2[2] - v0[2];

  N[0] = v20[1] * v10[2] - v20[2] * v10[1];
  N[1] = v20[2] * v10[0] - v20[0] * v10[2];
  N[2] = v20[0] * v10[1] - v20[1] * v10[0];

  float
    len2 = N[0] * N[0] + N[1] * N[1] + N[2] * N[2];
  if (len2 > 0.0f)
    {
      float
	len = sqrtf (len2);

      N[0] /= len;
      N[1] /= len;
    }
}

bool
LoadObjAndConvert (float bmin[3], float bmax[3],
		   std::vector < DrawObject > *drawObjects,
		   std::vector < tinyobj::material_t > &materials,
		   std::map < std::string, GLuint > &textures,
		   const char *filename)
{
  tinyobj::attrib_t attrib;
  std::vector < tinyobj::shape_t > shapes;


  std::string err;
  bool
    ret =
    tinyobj::LoadObj (&attrib, &shapes, &materials, &err, filename, NULL);
  if (!err.empty ())
    {
      std::cerr << err << std::endl;
    }

  if (!ret)
    {
      std::cerr << "Failed to load " << filename << std::endl;
      return false;
    }

  printf ("# of vertices  = %d\n", (int) (attrib.vertices.size ()) / 3);
  printf ("# of normals   = %d\n", (int) (attrib.normals.size ()) / 3);
  printf ("# of texcoords = %d\n", (int) (attrib.texcoords.size ()) / 2);
  printf ("# of materials = %d\n", (int) materials.size ());
  printf ("# of shapes    = %d\n", (int) shapes.size ());

  // Load diffuse textures
  {
    for (size_t m = 0; m < materials.size (); m++)
      {
	tinyobj::material_t * mp = &materials[m];

	if (mp->diffuse_texname.length () > 0)
	  {
	    // Only load the texture if it is not already loaded
	    if (textures.find (mp->diffuse_texname) == textures.end ())
	      {
		GLuint
		  texture_id;
		int
		  w,
		  h;
		int
		  comp;
		unsigned char *
		  image =
		  stbi_load (mp->diffuse_texname.c_str (), &w, &h, &comp,
			     STBI_default);
		if (image == nullptr)
		  {
		    std::cerr << "Unable to load texture: " << mp->
		      diffuse_texname << std::endl;
		    exit (1);
		  }
		glGenTextures (1, &texture_id);
		glBindTexture (GL_TEXTURE_2D, texture_id);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				 GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				 GL_NEAREST);
		if (comp == 3)
		  {
		    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB,
				  GL_UNSIGNED_BYTE, image);
		  }
		else if (comp == 4)
		  {
		    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, image);
		  }
		glBindTexture (GL_TEXTURE_2D, 0);
		stbi_image_free (image);
		textures.
		  insert (std::make_pair (mp->diffuse_texname, texture_id));
	      }
	  }
      }
  }

  bmin[0] = bmin[1] = bmin[2] = std::numeric_limits < float >::max ();
  bmax[0] = bmax[1] = bmax[2] = -std::numeric_limits < float >::max ();

  {
    for (size_t s = 0; s < shapes.size (); s++)
      {
	size_t
	  current_material_id = 0;
	if (shapes[s].mesh.material_ids.size () > 0
	    && shapes[s].mesh.material_ids.size () > s)
	  {
	    // Base case
	    current_material_id = shapes[s].mesh.material_ids[s];
	  }
	DrawObject
	  o;
	std::vector < float >
	  vb;			// pos(3float), normal(3float), color(3float)
	for (size_t f = 0; f < shapes[s].mesh.indices.size () / 3; f++)
	  {
	    tinyobj::index_t idx0 = shapes[s].mesh.indices[3 * f + 0];
	    tinyobj::index_t idx1 = shapes[s].mesh.indices[3 * f + 1];
	    tinyobj::index_t idx2 = shapes[s].mesh.indices[3 * f + 2];

	    current_material_id = shapes[s].mesh.material_ids[f];

	    if (current_material_id >= materials.size ())
	      {
		//std::cerr << "Invalid material index: " << current_material_id << std::endl;
	      }

	    float
	      diffuse[3];
	    for (size_t i = 0; i < 3; i++)
	      {
		diffuse[i] = materials[current_material_id].diffuse[i];
	      }
	    float
	      tc[3][2];
	    if (attrib.texcoords.size () > 0)
	      {
		assert (attrib.texcoords.size () >
			2 * idx0.texcoord_index + 1);
		assert (attrib.texcoords.size () >
			2 * idx1.texcoord_index + 1);
		assert (attrib.texcoords.size () >
			2 * idx2.texcoord_index + 1);
		tc[0][0] = attrib.texcoords[2 * idx0.texcoord_index];
		tc[0][1] =
		  1.0f - attrib.texcoords[2 * idx0.texcoord_index + 1];
		tc[1][0] = attrib.texcoords[2 * idx1.texcoord_index];
		tc[1][1] =
		  1.0f - attrib.texcoords[2 * idx1.texcoord_index + 1];
		tc[2][0] = attrib.texcoords[2 * idx2.texcoord_index];
		tc[2][1] =
		  1.0f - attrib.texcoords[2 * idx2.texcoord_index + 1];
	      }
	    else
	      {
		std::cerr << "Texcoordinates are not defined" << std::endl;
		exit (2);
	      }

	    float
	      v[3][3];
	    for (int k = 0; k < 3; k++)
	      {
		int
		  f0 = idx0.vertex_index;
		int
		  f1 = idx1.vertex_index;
		int
		  f2 = idx2.vertex_index;
		assert (f0 >= 0);
		assert (f1 >= 0);
		assert (f2 >= 0);

		v[0][k] = attrib.vertices[3 * f0 + k];
		v[1][k] = attrib.vertices[3 * f1 + k];
		v[2][k] = attrib.vertices[3 * f2 + k];
		bmin[k] = std::min (v[0][k], bmin[k]);
		bmin[k] = std::min (v[1][k], bmin[k]);
		bmin[k] = std::min (v[2][k], bmin[k]);
		bmax[k] = std::max (v[0][k], bmax[k]);
		bmax[k] = std::max (v[1][k], bmax[k]);
		bmax[k] = std::max (v[2][k], bmax[k]);
	      }

	    float
	      n[3][3];
	    if (attrib.normals.size () > 0)
	      {
		int
		  f0 = idx0.normal_index;
		int
		  f1 = idx1.normal_index;
		int
		  f2 = idx2.normal_index;
		assert (f0 >= 0);
		assert (f1 >= 0);
		assert (f2 >= 0);
		for (int k = 0; k < 3; k++)
		  {
		    n[0][k] = attrib.normals[3 * f0 + k];
		    n[1][k] = attrib.normals[3 * f1 + k];
		    n[2][k] = attrib.normals[3 * f2 + k];
		  }
	      }
	    else
	      {
		// compute geometric normal
		CalcNormal (n[0], v[0], v[1], v[2]);
		n[1][0] = n[0][0];
		n[1][1] = n[0][1];
		n[1][2] = n[0][2];
		n[2][0] = n[0][0];
		n[2][1] = n[0][1];
		n[2][2] = n[0][2];
	      }

	    for (int k = 0; k < 3; k++)
	      {
		vb.push_back (v[k][0]);
		vb.push_back (v[k][1]);
		vb.push_back (v[k][2]);
		vb.push_back (n[k][0]);
		vb.push_back (n[k][1]);
		vb.push_back (n[k][2]);
		// Combine normal and diffuse to get color.
		float
		  normal_factor = 0.2;
		float
		  diffuse_factor = 1 - normal_factor;
		float
		  c[3] = {
		  n[k][0] * normal_factor + diffuse[0] * diffuse_factor,
		  n[k][1] * normal_factor + diffuse[1] * diffuse_factor,
		  n[k][2] * normal_factor + diffuse[2] * diffuse_factor
		};
		float
		  len2 = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
		if (len2 > 0.0f)
		  {
		    float
		      len = sqrtf (len2);

		    c[0] /= len;
		    c[1] /= len;
		    c[2] /= len;
		  }
		vb.push_back (c[0] * 0.5 + 0.5);
		vb.push_back (c[1] * 0.5 + 0.5);
		vb.push_back (c[2] * 0.5 + 0.5);

		vb.push_back (tc[k][0]);
		vb.push_back (tc[k][1]);
	      }
	  }

	o.vb = 0;
	o.numTriangles = 0;
	o.material_id = current_material_id;
	if (vb.size () > 0)
	  {
	    glGenBuffers (1, &o.vb);
	    glBindBuffer (GL_ARRAY_BUFFER, o.vb);
	    glBufferData (GL_ARRAY_BUFFER, vb.size () * sizeof (float),
			  &vb.at (0), GL_STATIC_DRAW);
	    o.numTriangles = vb.size () / (3 + 3 + 3 + 2) * 3;
	    printf ("shape[%d] # of triangles = %d\n", static_cast < int >(s),
		    o.numTriangles);
	  }

	drawObjects->push_back (o);
      }
  }

  printf ("bmin = %f, %f, %f\n", bmin[0], bmin[1], bmin[2]);
  printf ("bmax = %f, %f, %f\n", bmax[0], bmax[1], bmax[2]);

  return true;
}


void
Draw (const std::vector < DrawObject > &drawObjects,
      std::vector < tinyobj::material_t > &materials, std::map < std::string,
      GLuint > &textures)
{
  glPolygonMode (GL_FRONT, GL_FILL);
  glPolygonMode (GL_BACK, GL_FILL);

  glEnable (GL_POLYGON_OFFSET_FILL);
  glPolygonOffset (1.0, 1.0);
  GLsizei
    stride = (3 + 3 + 3 + 2) * sizeof (float);
  for (size_t i = 0; i < drawObjects.size (); i++)
    {
      DrawObject
	o = drawObjects[i];
      if (o.vb < 1)
	{
	  continue;
	}

      glBindBuffer (GL_ARRAY_BUFFER, o.vb);
      glEnableClientState (GL_VERTEX_ARRAY);
      glEnableClientState (GL_NORMAL_ARRAY);
      glEnableClientState (GL_COLOR_ARRAY);
      glEnableClientState (GL_TEXTURE_COORD_ARRAY);

      std::string diffuse_texname = materials[o.material_id].diffuse_texname;
      if (diffuse_texname.length () > 0)
	{
	  glBindTexture (GL_TEXTURE_2D, textures[diffuse_texname]);
	}
      glVertexPointer (3, GL_FLOAT, stride, (const void *) 0);
      glNormalPointer (GL_FLOAT, stride, (const void *) (sizeof (float) * 3));
      glColorPointer (3, GL_FLOAT, stride,
		      (const void *) (sizeof (float) * 6));
      glTexCoordPointer (2, GL_FLOAT, stride,
			 (const void *) (sizeof (float) * 9));

      glDrawArrays (GL_TRIANGLES, 0, 3 * o.numTriangles);
      CheckErrors ("drawarrays");
      glBindTexture (GL_TEXTURE_2D, 0);
    }

  // draw wireframe
  /*glDisable(GL_POLYGON_OFFSET_FILL);
     glPolygonMode(GL_FRONT, GL_LINE);
     glPolygonMode(GL_BACK, GL_LINE);

     glColor3f(0.0f, 0.0f, 0.4f);
     for (size_t i = 0; i < drawObjects.size(); i++) {
     DrawObject o = drawObjects[i];
     if (o.vb < 1) {
     continue;
     }

     glBindBuffer(GL_ARRAY_BUFFER, o.vb);
     glEnableClientState(GL_VERTEX_ARRAY);
     glEnableClientState(GL_NORMAL_ARRAY);
     glDisableClientState(GL_COLOR_ARRAY);
     glDisableClientState(GL_TEXTURE_COORD_ARRAY);
     glVertexPointer(3, GL_FLOAT, stride, (const void*)0);
     glNormalPointer(GL_FLOAT, stride, (const void*)(sizeof(float) * 3));
     glColorPointer(3, GL_FLOAT, stride, (const void*)(sizeof(float) * 6));
     glTexCoordPointer(2, GL_FLOAT, stride, (const void*)(sizeof(float) * 9));

     glDrawArrays(GL_TRIANGLES, 0, 3 * o.numTriangles);
     CheckErrors("drawarrays");
     } */
}

void
setShaders ()
{

  char *
  vs, *
    fs;

  v = glCreateShader (GL_VERTEX_SHADER);
  f = glCreateShader (GL_FRAGMENT_SHADER);

  vs = textFileRead ("shader/toon.vert");
  fs = textFileRead ("shader/toon.frag");

  const char *
    vv = vs;
  const char *
    ff = fs;

  glShaderSource (v, 1, &vv, NULL);
  glShaderSource (f, 1, &ff, NULL);

  free (vs);
  free (fs);

  glCompileShader (v);
  glCompileShader (f);

  p = glCreateProgram ();

  glAttachShader (p, v);
  glAttachShader (p, f);

  glLinkProgram (p);
  glUseProgram (p);
}

void
idle ()
{
  glutPostRedisplay ();
}


void
display ()
{
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable (GL_DEPTH_TEST);
  glEnable (GL_TEXTURE_2D);

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  //glTranslatef(0,0,-3);

  // tramsform camera
  glTranslatef (0, 0, cameraDistance);
  glRotatef (cameraAngleX, 1, 0, 0);	// pitch
  glRotatef (cameraAngleY, 0, 1, 0);	// heading


  // Fit to -1, 1
  glScalef (1.0f / maxExtent, 1.0f / maxExtent, 1.0f / maxExtent);

  // Centerize object.
  glTranslatef (-0.5 * (bmax[0] + bmin[0]), -0.5 * (bmax[1] + bmin[1]),
		-0.5 * (bmax[2] + bmin[2]));

  Draw (gDrawObjects, materials, textures);

  showInfo ();
  glutSwapBuffers ();
}

void
reshape (int width, int height)
{
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glViewport (0, 0, width, height);

  float
    fovy = 60;
  float
    aspect_ratio = float (width) /
    height;
  float
    near_clip = 0.01;
  float
    far_clip = 100;

  gluPerspective (fovy, aspect_ratio, near_clip, far_clip);

  glMatrixMode (GL_MODELVIEW);
}

void
key (unsigned char key, int x, int y)
{
  if (key == 27)
    {				//esc
      exit (0);
    }
  if (key == '+')
    {
//      egen += 0.1;
    }
  if (key == '-')
    {
//      egen -= 0.1;
    }
}

void
init (int argc, char **argv)
{
  glutInitWindowSize (800, 600);
  glutInitDisplayMode (GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);


  glutCreateWindow ("Simple obj viewer");
  glewInit ();

  if (GLEW_ARB_vertex_shader && GLEW_ARB_fragment_shader)
    {
      printf ("Ready for GLSL\n");
      setShaders ();
    }
  else
    {
      printf ("No GLSL support\n");
      //exit(1);
    }

  if (false ==
      LoadObjAndConvert (bmin, bmax, &gDrawObjects, materials, textures,
			 argv[1]))
    {
      printf ("failed to load model\n");
    }

  maxExtent = 0.5f * (bmax[0] - bmin[0]);
  if (maxExtent < 0.5f * (bmax[1] - bmin[1]))
    {
      maxExtent = 0.5f * (bmax[1] - bmin[1]);
    }
  if (maxExtent < 0.5f * (bmax[2] - bmin[2]))
    {
      maxExtent = 0.5f * (bmax[2] - bmin[2]);
    }

  glutIdleFunc (idle);
  glutDisplayFunc (display);
  glutReshapeFunc (reshape);
  glutKeyboardFunc (key);
  glutMouseFunc (mouseCB);
  glutMotionFunc (mouseMotionCB);

  glEnable (GL_DEPTH_TEST);
}

int
main (int argc, char **argv)
{

  if (argc < 2)
    {
      std::cout << "Needs input.obj\n" << std::endl;
      return 0;
    }
  glutInit (&argc, argv);
  init (argc, argv);

  glutMainLoop ();

  return 0;


}

void
mouseMotionCB (int x, int y)
{
  if (mouseLeftDown)
    {
      cameraAngleY += (x - mouseX);
      cameraAngleX += (y - mouseY);
      mouseX = x;
      mouseY = y;
    }
  if (mouseMiddleDown)
    {
      cameraDistance += (y - mouseY) * 0.2f;
      mouseY = y;
    }

  glutPostRedisplay ();
}

void
mouseCB (int button, int state, int x, int y)
{
  mouseX = x;
  mouseY = y;

  if (button == GLUT_LEFT_BUTTON)
    {
      if (state == GLUT_DOWN)
	{
	  mouseLeftDown = true;
	}
      else if (state == GLUT_UP)
	mouseLeftDown = false;
    }

  else if (button == GLUT_RIGHT_BUTTON)
    {
      if (state == GLUT_DOWN)
	{
	  mouseRightDown = true;
	}
      else if (state == GLUT_UP)
	mouseRightDown = false;
    }

  else if (button == GLUT_MIDDLE_BUTTON)
    {
      if (state == GLUT_DOWN)
	{
	  mouseMiddleDown = true;
	}
      else if (state == GLUT_UP)
	mouseMiddleDown = false;
    }
}

///////////////////////////////////////////////////////////////////////////////
// display info messages
///////////////////////////////////////////////////////////////////////////////
void
showInfo ()
{
  // backup current model-view matrix
  glPushMatrix ();		// save current modelview matrix
  glLoadIdentity ();		// reset modelview matrix

  // set to 2D orthogonal projection
  glMatrixMode (GL_PROJECTION);	// switch to projection matrix
  glPushMatrix ();		// save current projection matrix
  glLoadIdentity ();		// reset projection matrix
  gluOrtho2D (0, 400, 0, 300);	// set to orthogonal projection

  float
  color[4] = { 1, 1, 1, 1 };

  stringstream
    ss;
  ss << std::fixed << std::setprecision (3);


  // unset floating format
  ss << std::resetiosflags (std::ios_base::fixed | std::ios_base::floatfield);

  // restore projection matrix
  glPopMatrix ();		// restore to previous projection matrix

  // restore modelview matrix
  glMatrixMode (GL_MODELVIEW);	// switch to modelview matrix
  glPopMatrix ();		// restore to previous modelview matrix
}

///////////////////////////////////////////////////////////////////////////////
// set camera position and lookat direction
///////////////////////////////////////////////////////////////////////////////
void
setCamera (float posX, float posY, float posZ, float targetX, float targetY,
	   float targetZ)
{
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  gluLookAt (posX, posY, posZ, targetX, targetY, targetZ, 0, 1, 0);	// eye(x,y,z), focal(x,y,z), up(x,y,z)
}

///////////////////////////////////////////////////////////////////////////////
// draw a string in 3D space
///////////////////////////////////////////////////////////////////////////////
void
drawString3D (const char *str, float pos[3], float color[4], void *font)
{
  glPushAttrib (GL_LIGHTING_BIT | GL_CURRENT_BIT);	// lighting and color mask
  glDisable (GL_LIGHTING);	// need to disable lighting for proper text color

  glColor4fv (color);		// set text color
  glRasterPos3fv (pos);		// place text position

  // loop all characters in the string
  while (*str)
    {
      glutBitmapCharacter (font, *str);
      ++str;
    }

  glEnable (GL_LIGHTING);
  glPopAttrib ();
}

///////////////////////////////////////////////////////////////////////////////
// write 2d text using GLUT
// The projection matrix must be set to orthogonal before call this function.
///////////////////////////////////////////////////////////////////////////////
void
drawString (const char *str, int x, int y, float color[4], void *font)
{
  glPushAttrib (GL_LIGHTING_BIT | GL_CURRENT_BIT);	// lighting and color mask
  glDisable (GL_LIGHTING);	// need to disable lighting for proper text color

  glColor4fv (color);		// set text color
  glRasterPos2i (x, y);		// place text position

  // loop all characters in the string
  while (*str)
    {
      glutBitmapCharacter (font, *str);
      ++str;
    }

  glEnable (GL_LIGHTING);
  glPopAttrib ();
}
