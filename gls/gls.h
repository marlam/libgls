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


/**
 * \file gls.h
 * \brief The libgls interface.
 *
 * This document describes the interface of libgls.
 */


/**
 * \mainpage libgls Reference
 *
 * \section intro Introduction
 *
 * The GL stereoscopic library (libgls) allows OpenGL programs to render
 * stereoscopic content in a variety of ways, without depending on OpenGL
 * quad-buffer support. See \link GLSmode \endlink for a list of supported stereoscopic
 * display modes.
 *
 * \section usage Usage
 *
 * All functions need a valid OpenGL context to work properly.
 *
 * Include the header file:
 * \code
 * #include <gls/gls.h>
 * \endcode
 *
 * Create a GLS context:
 * \code
 * GLScontext* ctx = glsCreateContext();
 * \endcode
 *
 * Render your stereoscopic scene:
 * \code
 * glsClear(ctx);
 * // ... clear and render left view ...
 * glsSubmitView(ctx, GLS_VIEW_LEFT);
 * // ... clear and render right view ...
 * glsSubmitView(ctx, GLS_VIEW_RIGHT);
 * glsDrawSubmittedViews(ctx, GLS_MODE_RED_CYAN_DUBOIS, GL_FALSE);
 * // ... swap buffers ...
 * \endcode
 *
 * Or, if you want to manage the views yourself:
 * \code
 * GLuint view_textures[2];
 * // ... render left view into view_texture[0] ...
 * // ... render right view into view_texture[1] ...
 * glsDrawViews(ctx, GLS_MODE_RED_CYAN_DUBOIS, GL_FALSE, view_textures);
 * \endcode
 *
 * Cleanup:
 * \code
 * glsDestroyContext(ctx);
 * \endcode
 *
 * \section scene Scene Management
 *
 * To render left and right view, you need to set up proper projection and
 * modelview matrices to achieve a good stereoscopic effect.
 *
 * The convenience functions glsFrustum() / glsPerspective() and glsLookAt() can
 * do that for you; they are drop-in replacements for glFrustum() /
 * gluPerspective() and gluLookAt().
 *
 * See http://paulbourke.net/miscellaneous/stereographics/stereorender/
 * for more information on this topic.
 */

#ifndef GLS_H
#define GLS_H

#include <GL/gl.h>

/* GLS_EXPORT: Declare functions as part of the library API.
 * (You only need to define GLS_STATIC for a static GLS library
 * if you use Microsoft compilers). */
#if (defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__
#   ifdef GLS_BUILD
#       ifdef DLL_EXPORT
#           define GLS_EXPORT __declspec(dllexport)
#       else
#           define GLS_EXPORT
#       endif
#   else
#       if defined _MSVC && !defined GLS_STATIC
#           define GLS_EXPORT __declspec(dllimport)
#       else
#           define GLS_EXPORT
#       endif
#   endif
#else
#   define GLS_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <gls/gls_version.h>

/**
 * \brief      The GLS context.
 *
 * All functions require a GLS context. See glsCreateContext()
 * and glsDestroyContext().
 */
typedef struct GLS_context GLScontext;

/**
 * \brief       GLS stereoscopic display modes.
 *
 * See http://www.site.uottawa.ca/~edubois/anaglyph/ for more information
 * about the Dubois anaglyph modes.
 */
typedef enum {
    GLS_MODE_QUAD_BUFFER_STEREO        = 0,
    /**< OpenGL quad buffered stereo. This mode can only be used if the
     * application created an OpenGL context with quad buffered stereo
     * support. */
    GLS_MODE_ALTERNATING               = 1,
    /**< Left and right view alternating for each display output frame. This
     * allows to use active stereo displays without OpenGL quad buffer support.
     * Note that this requires that you can render your scene at the display
     * framerate, which is at least at 120 Hz for active stereo displays.
     * Note also that this mode may be unreliable and may swap left/right eyes
     * occasionally, depending on your system, graphics hardware, and driver. */
    GLS_MODE_MONO_LEFT                 = 2,
    /**< Left view only. */
    GLS_MODE_MONO_RIGHT                = 3,
    /**< Right view only. */
    GLS_MODE_LEFT_RIGHT                = 4,
    /**< Left view in left half of viewport, right view in right half.
     * Used by some 3D TVs and displays. */
    GLS_MODE_TOP_BOTTOM                = 5,
    /**< Left view in top half of viewport, right view in bottom half.
     * Used by some 3D TVs and displays. */
    GLS_MODE_HDMI_FRAME_PACK           = 6,
    /**< HDMI Frame packing (left view in top half, right view in bottom half,
     * separated by 1/49 of the viewport height).
     *
     * This mode only makes sens if you are forcing your display into the
     * corresponding HDMI 3D mode. A description how to do this on GNU/Linux can
     * be found here:
     * http://lists.nongnu.org/archive/html/bino-list/2011-03/msg00033.html */
    GLS_MODE_EVEN_ODD_ROWS             = 7,
    /**< Left view in even pixel rows, right view in odd pixel rows.
     * Used by some 3D TVs and displays. */
    GLS_MODE_EVEN_ODD_COLUMNS          = 8,
    /**< Left view in even pixel columns, right view in odd pixel columns.
     * Used by some 3D TVs and displays. */
    GLS_MODE_CHECKERBOARD              = 9,
    /**< Left and right view pixels in a checkerboard pattern.
     * Used by some 3D TVs and displays. */
    GLS_MODE_RED_CYAN_MONOCHROME      = 10,
    /**< Red/cyan anaglyph glasses, monochrome method. */
    GLS_MODE_RED_CYAN_HALF_COLOR      = 11,
    /**< Red/cyan anaglyph glasses, half color method. */
    GLS_MODE_RED_CYAN_FULL_COLOR      = 12,
    /**< Red/cyan anaglyph glasses, full color method. */
    GLS_MODE_RED_CYAN_DUBOIS          = 13,
    /**< Red/cyan anaglyph glasses, high quality Dubois method (recommended). */
    GLS_MODE_GREEN_MAGENTA_MONOCHROME = 14,
    /**< Green/magenta anaglyph glasses, monochrome method. */
    GLS_MODE_GREEN_MAGENTA_HALF_COLOR = 15,
    /**< Green/magenta anaglyph glasses, half color method. */
    GLS_MODE_GREEN_MAGENTA_FULL_COLOR = 16,
    /**< Green/magenta anaglyph glasses, full color method. */
    GLS_MODE_GREEN_MAGENTA_DUBOIS     = 17,
    /**< Green/magenta anaglyph glasses, high quality Dubois method (recommended). */
    GLS_MODE_AMBER_BLUE_MONOCHROME    = 18,
    /**< Amber/blue anaglyph glasses, monochrome method. */
    GLS_MODE_AMBER_BLUE_HALF_COLOR    = 19,
    /**< Amber/blue anaglyph glasses, half color method. */
    GLS_MODE_AMBER_BLUE_FULL_COLOR    = 20,
    /**< Amber/blue anaglyph glasses, full color method. */
    GLS_MODE_AMBER_BLUE_DUBOIS        = 21,
    /**< Amber/blue anaglyph glasses, high quality Dubois method (recommended). */
    GLS_MODE_RED_GREEN_MONOCHROME     = 22,
    /**< Red/green anaglyph glasses, monochrome method. */
    GLS_MODE_RED_BLUE_MONOCHROME      = 23,
    /**< Red/blue anaglyph glasses, monochrome method. */
} GLSmode;

/**
 * \brief       GLS stereoscopic view.
 */
typedef enum {
    GLS_VIEW_LEFT  = 0, /**< Left view. */
    GLS_VIEW_RIGHT = 1  /**< Right view. */
} GLSview;

/**
 * \name Version information
 */

/*@{*/

/**
 * \brief       Get the libgls version.
 * \param major Buffer for the major version number, or NULL.
 * \param minor Buffer for the minor version number, or NULL.
 * \param patch Buffer for the patch version number, or NULL.
 * \return      The libgtls version string.
 *
 * This function returns the version string "MAJOR.MINOR.PATCH".
 * If the pointers \a major, \a minor, \a patch are not NULL,
 * the requested version number will be stored there.
 */
extern GLS_EXPORT
const char* glsVersion(GLint *major, GLint *minor, GLint *patch);

/*@}*/

/**
 * \name Manage GLS contexts
 */

/*@{*/

/**
 * \brief               Create a new GLS context.
 * \return              The GLS context.
 *
 * Creates a new GLS context. If you use multiple OpenGL contexts,
 * you need a GLS context for each of them. If something goes wrong,
 * this function returns a NULL pointer.
 */
extern GLS_EXPORT
GLScontext* glsCreateContext();

/**
 * \brief               Destroy a GLS context.
 * \param ctx           The GLS context.
 *
 * Destroys a GLS context and frees all of its resources.
 */
extern GLS_EXPORT
void glsDestroyContext(GLScontext* ctx);

/*@}*/

/**
 * \name Stereoscopic scene setup
 */

/*@{*/

/**
 * \brief               Stereoscopic variant of glFrustum().
 * \param left          Left clipping plane.
 * \param right         Right clipping plane.
 * \param bottom        Bottom clipping plane.
 * \param top           Top clipping plane.
 * \param zNear         Near plane.
 * \param zFar          Far plane.
 * \param focalLength   Focal length.
 * \param eyeSeparation Eye separation, typically 1/30 of focal length.
 * \param view          The view.
 *
 * This is a stereoscopic drop-in replacement for glFrustum().
 * It sets up an off-axis, asymmetric viewing frustum to render the
 * given view. In addition, you need to shift your scene to the right
 * by half the eye separation; glsLookAt() will do that automatically
 * for you.
 *
 * See http://www.opengl.org/sdk/docs/man/xhtml/glFrustum.xml
 * for a description of the glFrustum() parameters \a left , \a right,
 * \a bottom, \a top, \a zNear, and \a zFar.
 *
 * The \a focalLength parameter is the distance to the zero parallax plane.
 * It is often chosen roughly as the distance between the viewer and the scene
 * center. Objects nearer to the viewer are in front of the zero parallax plane
 * and have a negative parallax. Note that objects should not be nearer than
 * half the focal length, otherwise the stereoscopic effect suffers. Objects farther
 * from the viewer are behind the zero parallax plane and have a positive parallax.
 *
 * The \a eyeSeparation parameter is the distance between the left and the right eye.
 * This is often chosen to be 1/30 of the focal length.
 *
 * The \a view parameter selects the left or right view.
 *
 * See http://paulbourke.net/miscellaneous/stereographics/stereorender/
 * for more information on how to set up stereoscopic views.
 */
extern GLS_EXPORT
void glsFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
        GLdouble zNear, GLdouble zFar,
        GLdouble focalLength, GLdouble eyeSeparation, GLSview view);

/**
 * \brief               Stereoscopic variant of gluPerspective().
 * \param fovy          Field of view angle, in degrees.
 * \param aspect        Aspect ratio of window.
 * \param zNear         Near plane.
 * \param zFar          Far plane.
 * \param focalLength   Focal length.
 * \param eyeSeparation Eye separation, typically 1/30 of focal length.
 * \param view          The view.
 *
 * This is a stereoscopic drop-in replacement for gluPerspective().
 * It sets up an off-axis, asymmetric viewing frustum to render the
 * given view. In addition, you need to shift your scene to the right
 * by half the eye separation; glsLookAt() will do that automatically
 * for you.
 *
 * See http://www.opengl.org/sdk/docs/man/xhtml/gluPerspective.xml
 * for a description of the gluPerspective() parameters \a fovy, \a aspect, \a zNear,
 * and \a zFar.
 *
 * The \a focalLength parameter is the distance to the zero parallax plane.
 * It is often chosen roughly as the distance between the viewer and the scene
 * center. Objects nearer to the viewer are in front of the zero parallax plane
 * and have a negative parallax. Note that objects should not be nearer than
 * half the focal length, otherwise the stereoscopic effect suffers. Objects farther
 * from the viewer are behind the zero parallax plane and have a positive parallax.
 *
 * The \a eyeSeparation parameter is the distance between the left and the right eye.
 * This is often chosen to be 1/30 of the focal length.
 *
 * The \a view parameter selects the left or right view.
 *
 * See http://paulbourke.net/miscellaneous/stereographics/stereorender/
 * for more information on how to set up stereoscopic views.
 */
extern GLS_EXPORT
void glsPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar,
        GLdouble focalLength, GLdouble eyeSeparation, GLSview view);

/**
 * \brief               Stereoscopic variant of gluLookAt().
 * \param eyeX          X coordinate of the eye.
 * \param eyeY          Y coordinate of the eye.
 * \param eyeZ          Z coordinate of the eye.
 * \param centerX       X coordinate of the reference point.
 * \param centerY       Y coordinate of the reference point.
 * \param centerZ       Z coordinate of the reference point.
 * \param upX           X component of the up vector.
 * \param upY           Y component of the up vector.
 * \param upZ           Z component of the up vector.
 * \param eyeSeparation Eye separation, typically 1/30 of focal length.
 * \param view          The view.
 *
 * This is a stereoscopic drop-in replacement for gluLookAt().
 * It shifts your scene sidewise by half the eye separation, depending
 * on the view. In addition, you need to set up an off-axis, asymmetric
 * viewing frustum; glsPerspective() will do that automatically for you.
 *
 * See http://www.opengl.org/sdk/docs/man/xhtml/gluLookAt.xml
 * for a description of the gluLookAt() parameters \a eyeX, \a eyeY, \a eyeZ,
 * \a centerX, \a centerY, \a centerZ, \a upX, \a upY, and \a upZ.
 *
 * The \a eyeSeparation parameter is the distance between the left and the right eye.
 * This is often chosen to be 1/30 of the focal length; see glsPerspective().
 *
 * The \a view parameter selects the left or right view.
 *
 * See http://paulbourke.net/miscellaneous/stereographics/stereorender/
 * for more information on how to set up stereoscopic views.
 */
void glsLookAt(GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ,
        GLdouble centerX, GLdouble centerY, GLdouble centerZ,
        GLdouble upX, GLdouble upY, GLdouble upZ,
        GLdouble eyeSeparation, GLSview view);

/*@}*/

/**
 * \name Set GLS options
 */

/*@{*/

/**
 * \brief               Set the screen coordinates of the viewport.
 * \param ctx           The GLS context.
 * \param x             The x coordinate of the bottom left corner.
 * \param y             The y coordinate of the bottom left corner.
 *
 * Sets the current screen coordinates of the OpenGL viewport.
 *
 * This information is used by the modes \a GLS_MODE_EVEN_ODD_ROWS,
 * \a GLS_MODE_EVEN_ODD_COLUMNS, and \a GLS_MODE_CHECKERBOARD, because
 * these modes operate on absolute pixel coordinates and not on
 * viewport-relative pixel coordinates.
 * All other modes ignore this.
 *
 * Note that lines are counted from bottom to top, and that the bottom left
 * corner coordinates are required.
 */
extern GLS_EXPORT
void glsSetViewportScreenCoords(GLScontext* ctx, GLint x, GLint y);

/**
 * \brief               Set crosstalk levels for the display device.
 * \param ctx           The GLS context.
 * \param r             Red color channel crosstalk, from 0 to 1.
 * \param g             Green color channel crosstalk, from 0 to 1.
 * \param b             Blue color channel crosstalk, from 0 to 1.
 * \param ghostbust     Ghostbusting level, from 0 to 1.
 *
 * Set the crosstalk levels for this display device, and the level of ghostbusting
 * that will be applied.
 * By default, ghostbusting is disabled (\a ghostbust = 0).
 *
 * Many stereoscopic display devices suffer from crosstalk between the left and
 * right view. This results in ghosting artifacts that can degrade the viewing
 * quality, depending on the stereoscopic content.
 *
 * Libgls can optionally reduce the ghosting artifacts. For this, it needs two know
 * - the amount of crosstalk of the display device and
 * - the amount of ghostbusting that is adequate for the stereoscopic content.
 *
 * To measure the display crosstalk, do the following:
 * - Display the <a href="gamma-pattern-tb.png">gamma-pattern-tb.png</a>
 *   stereoscopic image (e.g. with the <a href="http://bino3d.org">Bino 3D video
 *   player</a>) and correct the display gamma settings according to the included
 *   instructions. You need to have correct gamma settings before measuring
 *   crosstalk.
 * - Display the <a href="crosstalk-pattern-tb.png">crosstalk-pattern-tb.png</a>
 *   stereoscopic image (e.g. with the <a href="http://bino3d.org">Bino 3D video player</a>)
 *   and determine the crosstalk levels using the included instructions.
 *
 * You now have three crosstalk values for the red, green, and blue channels. Use
 * these values as the crosstalk parameters \a r, \a g, and \a b of this function.
 *
 * Furthermore, you can adjust the amount of ghostbusting that libgls should apply
 * using the \a ghostbust parameter.
 * This will vary depending on the stereoscopic contenty. Very dark scenes should
 * be viewed with at least 50% ghostbusting (\a ghostbust is 0.5), whereas overall
 * bright scenes, where crosstalk is less disturbing, could be viewed with a lower
 * level (e.g. \a ghostbust 0.1).
 *
 * Please note that ghostbusting does not work with anaglyph glasses.
 */
extern GLS_EXPORT
void glsSetCrosstalkGhostbusting(GLScontext* ctx, GLfloat r, GLfloat g, GLfloat b, GLfloat ghostbust);

/*@}*/

/**
 * \name Stereoscopic Display
 */

/*@{*/

/**
 * \brief               Start a new frame.
 * \param ctx           The GLS context.
 *
 * This function must always be called when starting a new frame,
 * before calling any other GLS function.
 */
extern GLS_EXPORT
void glsClear(GLScontext* ctx);

/**
 * \brief               Check if a view is required for the given mode.
 * \param ctx           The GLS context.
 * \param mode          The stereoscopic display mode.
 * \param swapViews     Whether to swap left and right view.
 * \param view          The view.
 * \return              Whether the given view is required for the given mode.
 *
 * This function can be used to avoid rendering the left or right view if that view
 * is not required by the given mode. This is mainly useful for the
 * \a GLS_MODE_ALTERNATING mode, which needs only either the left or the right view
 * and requires rendering with at least 120 fps, but it can also be useful for
 * the \a GLS_MODE_MONO_LEFT and \a GLS_MODE_MONO_RIGHT modes.
 */
extern GLS_EXPORT
GLboolean glsIsViewRequired(GLScontext* ctx, GLSmode mode, GLboolean swapViews, GLSview view);

/**
 * \brief               Submit a view to the current frame.
 * \param ctx           The GLS context.
 * \param view          The view.
 *
 * Submit a view to the current frame. Before displaying
 * the current frame in a stereoscopic mode, you need to
 * submit left view and/or right view.
 *
 * The view is taken from the current viewport inside the
 * current GL_READ_BUFFER.
 */
extern GLS_EXPORT
void glsSubmitView(GLScontext* ctx, GLSview view);

/**
 * \brief               Displays the submitted views in stereoscopic mode.
 * \param ctx           The GLS context.
 * \param mode          The stereoscopic display mode.
 * \param swapViews     Whether to swap left and right view.
 *
 * Takes the views that were submitted for this frame and renders them in the given mode.
 *
 * The frame is rendered into the current GL_DRAW_BUFFER.
 */
extern GLS_EXPORT
void glsDrawSubmittedViews(GLScontext* ctx, GLSmode mode, GLboolean swapViews);

/**
 * \brief               Displays the given views in stereoscopic mode.
 * \param ctx           The GLS context.
 * \param mode          The stereoscopic display mode.
 * \param swapViews     Whether to swap left and right view.
 * \param leftViewTexture  The texture containing the left view.
 * \param rightViewTexture The texture containing the right view.
 *
 * Renders the given views in the given mode. If a view is not available, its texture
 * can be zero. In this case, libgls will simply use the other texture for both views
 * if required by the display mode.
 *
 * The result is rendered into the current GL_DRAW_BUFFER.
 */
extern GLS_EXPORT
void glsDrawViews(GLScontext* ctx, GLSmode mode, GLboolean swapViews,
        GLuint leftViewTexture, GLuint rightViewTexture);

/**
 * \brief               Draw optional DLP 3D Ready Sync markers.
 * \param ctx           The GLS context.
 * \param mode          The stereoscopic display mode.
 *
 * Draws DLP 3D Ready Sync markers. Display devices can use these markers
 * to identify certain stereoscopic modes automatically.
 *
 * This only makes sense if
 * - \a mode is one of \a GLS_MODE_LEFT_RIGHT, \a GLS_MODE_TOP_BOTTOM,
 *   or \a GLS_MODE_ALTERNATING,
 * - and the application renders in fullscreen mode,
 * - and the connected display device supports this feature.
 *
 * Use this function after glsDrawSubmittedViews() / glsDrawViews().
 */
extern GLS_EXPORT
void glsDrawDLP3dReadySyncMarker(GLScontext* ctx, GLSmode mode);

/*@}*/

#endif
