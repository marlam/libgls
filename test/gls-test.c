/*
 * This file is part of libgls, a library for stereoscopic OpenGL rendering.
 *
 * Copyright (C) 2012
 * Martin Lambers <marlam@marlam.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
# define M_PI		3.14159265358979323846	/* pi */
#endif

#include <GL/glut.h>
#include <gls/gls.h>


/* GLUT state */
GLboolean glut_stereo = 0;
int glut_window_id;
GLboolean fullscreen = GL_FALSE;
int fullscreen_window_x_bak;
int fullscreen_window_y_bak;
int fullscreen_window_w_bak;
int fullscreen_window_h_bak;

/* Scene state */
GLboolean pause = GL_FALSE;
GLfloat rotation_angle = 0.0f;

/* GLS context */
GLScontext* gls_ctx;

/* GLS parameters */
GLSmode gls_mode = GLS_MODE_RED_CYAN_DUBOIS;
GLboolean gls_swap_eyes = GL_FALSE;

/* GLUT display function */
void display()
{
    /* Setup scene: an 1m large teapot centered at (2,3,1), with the viewer
     * moving around it at roughly 2m distance, always looking at the teapot's
     * center. */
    GLfloat objX = 2.0f, objY = 3.0f, objZ = 1.0f;
    GLfloat upX = 0.0f, upY = 1.0f, upZ = 0.0f;
    GLfloat eyeX = 2.0f * cosf(rotation_angle / 180.0f * (float)M_PI) + objX;
    GLfloat eyeY = cosf(3.0f * rotation_angle / 180.0f * (float)M_PI) + objY;
    GLfloat eyeZ = 2.0f * sinf(rotation_angle / 180.0f * (float)M_PI) + objZ;
    /* Setup view: a typical frustum */
    GLfloat fovy = 50.0f;
    GLfloat aspect = (GLfloat)glutGet(GLUT_WINDOW_WIDTH) / glutGet(GLUT_WINDOW_HEIGHT);
    GLfloat zNear = 0.1f;
    GLfloat zFar = 10.0f;
    /* Setup stereoscopic parameters: the zero-parallax plane is at the center
     * of the object, and the eye separation follows the common rule-of-thumb. */
    GLfloat focalLength = 2.0f;
    GLfloat eyeSeparation = focalLength / 30.0f;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    /* Render stereoscopic scene */
    glsClear(gls_ctx);
    glsSetViewportScreenCoords(gls_ctx, glutGet(GLUT_WINDOW_X), glutGet(GLUT_WINDOW_Y));
    // left view
    if (glsIsViewRequired(gls_ctx, gls_mode, gls_swap_eyes, GLS_VIEW_LEFT)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glsPerspective(fovy, aspect, zNear, zFar, focalLength, eyeSeparation, GLS_VIEW_LEFT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glsLookAt(eyeX, eyeY, eyeZ, objX, objY, objZ, upX, upY, upZ, eyeSeparation, GLS_VIEW_LEFT);
        glTranslatef(objX, objY, objZ);
        glutSolidTeapot(0.5);
        glsSubmitView(gls_ctx, GLS_VIEW_LEFT);
    }
    // render right view
    if (glsIsViewRequired(gls_ctx, gls_mode, gls_swap_eyes, GLS_VIEW_RIGHT)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glsPerspective(fovy, aspect, zNear, zFar, focalLength, eyeSeparation, GLS_VIEW_RIGHT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glsLookAt(eyeX, eyeY, eyeZ, objX, objY, objZ, upX, upY, upZ, eyeSeparation, GLS_VIEW_RIGHT);
        glTranslatef(objX, objY, objZ);
        glutSolidTeapot(0.5);
        glsSubmitView(gls_ctx, GLS_VIEW_RIGHT);
    }
    // render result
    glsDrawSubmittedViews(gls_ctx, gls_mode, gls_swap_eyes);
    if (fullscreen)
        glsDrawDLP3dReadySyncMarker(gls_ctx, gls_mode);

    glutSwapBuffers();
}

/* GLUT idle func */
void idle()
{
    if (!pause)
        rotation_angle = glutGet(GLUT_ELAPSED_TIME) / 1000.0f * 15.0f;
    display();
}

/* GLUT keyboard function */
void keyboard(unsigned char key, int x, int y)
{
    switch (key) {
    case 'f':
        fullscreen = !fullscreen;
        if (fullscreen) {
            fullscreen_window_x_bak = glutGet(GLUT_WINDOW_X);
            fullscreen_window_y_bak = glutGet(GLUT_WINDOW_X);
            fullscreen_window_w_bak = glutGet(GLUT_WINDOW_WIDTH);
            fullscreen_window_h_bak = glutGet(GLUT_WINDOW_HEIGHT);
            glutFullScreen();
        } else {
            glutReshapeWindow(fullscreen_window_w_bak, fullscreen_window_h_bak);
            glutPositionWindow(fullscreen_window_x_bak, fullscreen_window_y_bak);
        }
        break;
    case 'p':
    case ' ':
        pause = !pause;
        break;
    case 's':
        gls_swap_eyes = !gls_swap_eyes;
        break;
    case 'm':
        {
            int m = gls_mode;
            m++;
            if (m > GLS_MODE_RED_BLUE_MONOCHROME)
                m = GLS_MODE_QUAD_BUFFER_STEREO;
            if (m == GLS_MODE_QUAD_BUFFER_STEREO && !glut_stereo)
                m++;
            gls_mode = m;
        }
        break;
    case 27:
    case 'q':
        glutDestroyWindow(glut_window_id);
        break;
    }
}

int main(int argc, char *argv[])
{
    int i;
    for (i = 0; i < argc; i++)
        if (strcmp(argv[i], "-s") == 0)
            glut_stereo = GL_TRUE;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH
            | (glut_stereo ? GLUT_STEREO : 0));
    glutInitWindowSize(800, 800);
    glutInitWindowPosition(300, 200);
    glut_window_id = glutCreateWindow("Stereoscopic Teapot");
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutKeyboardFunc(keyboard);
    gls_ctx = glsCreateContext();
    glutMainLoop();
    glsDestroyContext(gls_ctx);
    return 0;
}
