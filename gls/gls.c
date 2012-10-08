/*
 * This file is part of libgls, a library for stereoscopic OpenGL rendering.
 *
 * Copyright (C) 2010, 2011, 2012
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
# define M_PI           3.14159265358979323846  /* pi */
#endif

#ifndef GLEW_MX
# define GLEW_MX
#endif
#include <GL/glew.h>
#if GLS_USE_GLX
# include <GL/glxew.h>
#endif

#define GLS_BUILD
#include "gls/gls.h"

#include "gls.glsl.h"


/*
 * Internal Types
 */

struct GLS_context
{
    /* GLEW context: */
    GLEWContext glewctx;
#if GLS_USE_GLX
    GLXEWContext glxewctx;
#endif

    /* The views: */
    GLboolean have_view[2];
    GLuint view_tex[2];
    GLint view_tex_width[2];
    GLint view_tex_height[2];

    /* For masking modes: */
    GLuint even_odd_rows_mask_tex;
    GLuint even_odd_columns_mask_tex;
    GLuint checkerboard_mask_tex;
    GLint viewport_screen_x;
    GLint viewport_screen_y;

    /* For alternating mode: */
    GLulong display_frame_counter;

    /* Parallax adjustment: */
    GLfloat parallax_adjust;

    /* Ghostbusting: */
    GLfloat crosstalk_r;
    GLfloat crosstalk_g;
    GLfloat crosstalk_b;
    GLfloat ghostbust;

    /* The render program: */
    GLuint prg;
    GLSmode prg_mode;
    GLint prg_ghostbust;

    /* For DLP 3D Ready Sync: */
    unsigned int* dlp_3d_ready_sync_buf;
    size_t dlp_3d_ready_sync_buf_size;
};

#define glewGetContext() (&(ctx->glewctx))
#if GLS_USE_GLX
# define glxewGetContext() (&(ctx->glxewctx))
#endif


/*
 * Version information
 */

const char *
glsVersion(GLint* major, GLint* minor, GLint* patch)
{
    if (major)
        *major = GLS_VERSION_MAJOR;
    if (minor)
        *minor = GLS_VERSION_MINOR;
    if (patch)
        *patch = GLS_VERSION_PATCH;
    return GLS_VERSION;
}


/**
 * Helper functions
 */

static void oom_abort()
{
    fprintf(stderr, "libgls: memory allocation failed\n");
    abort();
}

static void str_replace(char** s, const char* a, const char* b)
{
    // Replace all occurrences of a in *s with b.
    // *s will be reallocated as necessary.
    long long s_len = strlen(*s);
    long long a_len = strlen(a);
    long long b_len = strlen(b);
    long long d = b_len - a_len;
    char* p = *s;
    while ((p = strstr(p, a))) {
        if (d != 0) {
            long long pos = p - *s;
            *s = realloc(*s, s_len + d + 1);
            if (!*s)
                oom_abort();
            p = *s + pos;
            memmove(p + b_len, p + a_len, s_len - pos - a_len + 1);
            s_len += d;
        }
        strncpy(p, b, b_len);
        p += b_len;
    }
}

static void kill_crlf(char* str)
{
    size_t l = strlen(str);
    if (l > 0 && str[l - 1] == '\n')
        str[--l] = '\0';
    if (l > 0 && str[l - 1] == '\r')
        str[l - 1] = '\0';
}

static GLuint compile_fragment_shader(GLScontext* ctx, const char* src)
{
    char* log = NULL;
    GLint e, l;
    GLuint shader;

    shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shader, 1, (const GLchar**)(&src), NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &e);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &l);
    if (l > 0) {
        log = malloc(l);
        if (!log)
            oom_abort();
        glGetShaderInfoLog(shader, l, NULL, log);
        kill_crlf(log);
    }
    if (log && log[0] == '\0') {
        free(log);
        log = NULL;
    }

    if (e == GL_TRUE && log) {
#ifndef NDEBUG
        fprintf(stderr, "libgls: OpenGL compiler warning:\n%s\n", log);
#endif
    } else if (e != GL_TRUE) {
        fprintf(stderr, "libgls: OpenGL compiler error:\n%s\n", log);
        abort();
    }

    free(log);
    return shader;
}

static void link_program(GLScontext* ctx, GLuint prg)
{
    char* log = NULL;
    GLint e, l;

    glLinkProgram(prg);
    glGetProgramiv(prg, GL_LINK_STATUS, &e);
    glGetProgramiv(prg, GL_INFO_LOG_LENGTH, &l);
    if (l > 0) {
        log = malloc(l);
        if (!log)
            oom_abort();
        glGetProgramInfoLog(prg, l, NULL, log);
        kill_crlf(log);
    }
    if (log && log[0] == '\0') {
        free(log);
        log = NULL;
    }

    if (e == GL_TRUE && log) {
#ifndef NDEBUG
        fprintf(stderr, "libgls: OpenGL linker warning:\n%s\n", log);
#endif
    } else if (e != GL_TRUE) {
        fprintf(stderr, "libgls: OpenGL linker error:\n%s\n", log);
        abort();
    }

    free(log);
}

static void draw_quad(GLScontext* ctx, GLint viewport_width, GLint viewport_height)
{
    const float x = -1.0f;
    const float y = -1.0f;
    const float w = 2.0f;
    const float h = 2.0f;
    const float tex_coords0[4][2] = {
        { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }
    };
    const float tex_coords1[4][2] = {
        { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }
    };
    const float tex_coords2[4][2] = {
        { 0.0f, 0.0f },
        { viewport_width / 2.0f, 0.0f },
        { viewport_width / 2.0f, viewport_height / 2.0f },
        { 0.0f, viewport_height / 2.0f }
    };
    glBegin(GL_QUADS);
    glTexCoord2f(tex_coords0[0][0], tex_coords0[0][1]);
    glMultiTexCoord2f(GL_TEXTURE1, tex_coords1[0][0], tex_coords1[0][1]);
    glMultiTexCoord2f(GL_TEXTURE2, tex_coords2[0][0], tex_coords2[0][1]);
    glVertex2f(x, y);
    glTexCoord2f(tex_coords0[1][0], tex_coords0[1][1]);
    glMultiTexCoord2f(GL_TEXTURE1, tex_coords1[1][0], tex_coords1[1][1]);
    glMultiTexCoord2f(GL_TEXTURE2, tex_coords2[1][0], tex_coords2[1][1]);
    glVertex2f(x + w, y);
    glTexCoord2f(tex_coords0[2][0], tex_coords0[2][1]);
    glMultiTexCoord2f(GL_TEXTURE1, tex_coords1[2][0], tex_coords1[2][1]);
    glMultiTexCoord2f(GL_TEXTURE2, tex_coords2[2][0], tex_coords2[2][1]);
    glVertex2f(x + w, y + h);
    glTexCoord2f(tex_coords0[3][0], tex_coords0[3][1]);
    glMultiTexCoord2f(GL_TEXTURE1, tex_coords1[3][0], tex_coords1[3][1]);
    glMultiTexCoord2f(GL_TEXTURE2, tex_coords2[3][0], tex_coords2[3][1]);
    glVertex2f(x, y + h);
    glEnd();
}

static void crossproduct(
        GLfloat ax, GLfloat ay, GLfloat az,
        GLfloat bx, GLfloat by, GLfloat bz,
        GLfloat *cx, GLfloat *cy, GLfloat *cz)
{
    *cx = ay * bz - az * by;
    *cy = az * bx - ax * bz;
    *cz = ax * by - ay * bx;
}


/*
 * Manage contexts
 */

GLScontext* glsCreateContext()
{
    GLScontext* ctx = malloc(sizeof(GLScontext));
    if (ctx) {
        glewInit();
#if GLS_USE_GLX
        glxewInit();
#endif
        ctx->have_view[0] = GL_FALSE;
        ctx->have_view[1] = GL_FALSE;
        ctx->view_tex[0] = 0;
        ctx->view_tex[1] = 0;
        ctx->even_odd_rows_mask_tex = 0;
        ctx->even_odd_columns_mask_tex = 0;
        ctx->checkerboard_mask_tex = 0;
        ctx->viewport_screen_x = 0;
        ctx->viewport_screen_y = 0;
        ctx->display_frame_counter = 0;
        ctx->parallax_adjust = 0.0f;
        ctx->crosstalk_r = 0.0f;
        ctx->crosstalk_g = 0.0f;
        ctx->crosstalk_b = 0.0f;
        ctx->ghostbust = 0.0f;
        ctx->prg = 0;
        ctx->dlp_3d_ready_sync_buf = NULL;
        ctx->dlp_3d_ready_sync_buf_size = 0;
    }
    return ctx;
}

void glsDestroyContext(GLScontext* ctx)
{
    if (ctx) {
        glDeleteTextures(2, ctx->view_tex);
        glDeleteTextures(1, &ctx->even_odd_rows_mask_tex);
        glDeleteTextures(1, &ctx->even_odd_columns_mask_tex);
        glDeleteTextures(1, &ctx->checkerboard_mask_tex);
        if (glIsProgram(ctx->prg)) {
            GLint shader_count;
            glGetProgramiv(ctx->prg, GL_ATTACHED_SHADERS, &shader_count);
            if (shader_count > 0) {
                GLint i;
                GLuint *shaders = malloc(shader_count * sizeof(GLuint));
                if (!shaders)
                    oom_abort();
                glGetAttachedShaders(ctx->prg, shader_count, NULL, shaders);
                for (i = 0; i < shader_count; i++)
                    glDeleteShader(shaders[i]);
                free(shaders);
            }
            glDeleteProgram(ctx->prg);
        }
        free(ctx->dlp_3d_ready_sync_buf);
        free(ctx);
    }
}


/**
 * Set GLS options
 */

void glsSetViewportScreenCoords(GLScontext* ctx, GLint x, GLint y)
{
    ctx->viewport_screen_x = x;
    ctx->viewport_screen_y = y;
}

void glsSetCrosstalkGhostbusting(GLScontext* ctx,
        GLfloat r, GLfloat g, GLfloat b, GLfloat ghostbust)
{
    ctx->crosstalk_r = r;
    ctx->crosstalk_g = g;
    ctx->crosstalk_b = b;
    ctx->ghostbust = ghostbust;
}


/**
 * Stereoscopic Setup
 */

void glsFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
        GLdouble nearVal, GLdouble farVal,
        GLdouble focalLength, GLdouble eyeSeparation, GLSview view)
{
    // Shift left/right according to view and eye separation
    GLdouble d = eyeSeparation / 2.0 * nearVal / focalLength;
    if (view == GLS_VIEW_LEFT) {
        left += d;
        right += d;
    } else {
        left -= d;
        right -= d;
    }
    glFrustum(left, right, bottom, top, nearVal, farVal);

    /* We could shift the eye here:
     *
     * if (view == GLS_VIEW_LEFT)
     *   glTranslatef(eyeSeparationf / 2.0f, 0.0f, 0.0f);
     * else
     *   glTranslatef(-eyeSeparation / 2.0f, 0.0f, 0.0f);
     *
     * However, then a translation is part of the projection matrix, and the
     * OpenGL camera is not in (0,0,0) anymore, which would probably cause a
     * lot of trouble for applications. Therefore it is better to require the
     * user to add this translation to the modelview matrix instead.
     */
}

void glsPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar,
        GLdouble focalLength, GLdouble eyeSeparation, GLSview view)
{
    GLdouble t = tan(fovy / 180.0 * M_PI / 2.0);
    // Compute symmetric frustum
    GLdouble top = zNear * t;
    GLdouble bottom = -top;
    GLdouble right = top * aspect;
    GLdouble left = -right;
    // Use glsFrustum to adjust for stereoscopic view
    glsFrustum(left, right, bottom, top, zNear, zFar,
            focalLength, eyeSeparation, view);
}

void glsLookAt(GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ,
        GLdouble centerX, GLdouble centerY, GLdouble centerZ,
        GLdouble upX, GLdouble upY, GLdouble upZ,
        GLdouble eyeSeparation, GLSview view)
{
    GLfloat viewX, viewY, viewZ, viewLen;
    GLfloat sX, sY, sZ, sLen;
    GLfloat uX, uY, uZ;
    GLfloat M[16];
    GLfloat ef;

    // Compute the view direction and normalize it
    viewX = centerX - eyeX;
    viewY = centerY - eyeY;
    viewZ = centerZ - eyeZ;
    viewLen = sqrtf(viewX * viewX + viewY * viewY + viewZ * viewZ);
    viewX /= viewLen;
    viewY /= viewLen;
    viewZ /= viewLen;
    // Compute right side direction
    crossproduct(viewX, viewY, viewZ, upX, upY, upZ, &sX, &sY, &sZ);
    sLen = sqrtf(sX * sX + sY * sY + sZ * sZ);
    sX /= sLen;
    sY /= sLen;
    sZ /= sLen;
    // Perform the equivalent of gluLookAt()
    crossproduct(sX, sY, sZ, viewX, viewY, viewZ, &uX, &uY, &uZ);
    M[0 * 4 + 0] = sX;
    M[0 * 4 + 1] = uX;
    M[0 * 4 + 2] = -viewX;
    M[0 * 4 + 3] = 0.0f;
    M[1 * 4 + 0] = sY;
    M[1 * 4 + 1] = uY;
    M[1 * 4 + 2] = -viewY;
    M[1 * 4 + 3] = 0.0f;
    M[2 * 4 + 0] = sZ;
    M[2 * 4 + 1] = uZ;
    M[2 * 4 + 2] = -viewZ;
    M[2 * 4 + 3] = 0.0f;
    M[3 * 4 + 0] = 0.0f;
    M[3 * 4 + 1] = 0.0f;
    M[3 * 4 + 2] = 0.0f;
    M[3 * 4 + 3] = 1.0f;
    glMultMatrixf(M);
    // Shift eye position according to eye separation and view.
    // Note that this is the only difference to gluLookAt()!
    ef = eyeSeparation / 2.0;
    if (view == GLS_VIEW_LEFT)
        glTranslated(-(eyeX - sX * ef), -(eyeY - sY * ef), -(eyeZ - sZ * ef));
    else
        glTranslated(-(eyeX + sX * ef), -(eyeY + sY * ef), -(eyeZ + sZ * ef));
}


/**
 * Stereoscopic Display
 */

void glsClear(GLScontext* ctx)
{
    ctx->have_view[0] = 0;
    ctx->have_view[1] = 0;

    /* Get display frame counter */
#if GLS_USE_GLX
    GLuint display_frame_counter;
    if (GLXEW_SGI_video_sync && glXGetVideoSyncSGI(&display_frame_counter) == 0)
        ctx->display_frame_counter = display_frame_counter;
    else
        ctx->display_frame_counter++;
#else
    ctx->display_frame_counter++;
#endif
}

GLboolean glsIsViewRequired(GLScontext* ctx, GLSmode mode, GLboolean swap_views, GLSview view)
{
    if (mode == GLS_MODE_MONO_LEFT
            || (mode == GLS_MODE_ALTERNATING && ctx->display_frame_counter % 2 == 0))
        return (!swap_views && view == GLS_VIEW_LEFT) || (swap_views && view == GLS_VIEW_RIGHT);
    else if (mode == GLS_MODE_MONO_RIGHT
            || (mode == GLS_MODE_ALTERNATING && ctx->display_frame_counter % 2 == 1))
        return (!swap_views && view == GLS_VIEW_RIGHT) || (swap_views && view == GLS_VIEW_LEFT);
    else
        return GL_TRUE;
}

void glsSubmitView(GLScontext* ctx, GLSview view)
{
    GLint texture_binding_2d_bak;
    GLint viewport[4];

    /* Backup GL state */
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_binding_2d_bak);

    /* Get current viewport */
    glGetIntegerv(GL_VIEWPORT, viewport);

    /* Make sure our view texture is valid and has the right size */
    if (ctx->view_tex[view] == 0) {
        glGenTextures(1, &(ctx->view_tex[view]));
        ctx->view_tex_width[view] = -1;
        ctx->view_tex_height[view] = -1;
    }
    glBindTexture(GL_TEXTURE_2D, ctx->view_tex[view]);
    if (ctx->view_tex_width[view] != viewport[2]
            || ctx->view_tex_height[view] != viewport[3]) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewport[2], viewport[3], 0,
                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        ctx->view_tex_width[view] = viewport[2];
        ctx->view_tex_height[view] = viewport[3];
    }

    /* Copy the GL_READ_BUFFER content to our view texture */
    //glPixelTransferf(GL_RED_SCALE, 1.0f);
    //glPixelTransferf(GL_GREEN_SCALE, 1.0f);
    //glPixelTransferf(GL_BLUE_SCALE, 1.0f);
    //glPixelTransferf(GL_RED_BIAS, 0.0f);
    //glPixelTransferf(GL_GREEN_BIAS, 0.0f);
    //glPixelTransferf(GL_BLUE_BIAS, 0.0f);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
            viewport[0], viewport[1], viewport[2], viewport[3]);

    /* Restore GL state */
    glBindTexture(GL_TEXTURE_2D, texture_binding_2d_bak);

    ctx->have_view[view] = 1;
}

void glsDrawSubmittedViews(GLScontext* ctx, GLSmode mode, GLboolean swap_views)
{
    GLuint view_textures[2] = { 0, 0 };
    if (ctx->have_view[0])
        view_textures[0] = ctx->view_tex[0];
    if (ctx->have_view[1])
        view_textures[1] = ctx->view_tex[1];
    glsDrawViews(ctx, mode, swap_views, view_textures);
}

void glsDrawViews(GLScontext* ctx, GLSmode mode, GLboolean swap_views,
        const GLuint* view_textures)
{
    GLint viewport[4];
    GLint current_program_bak;
    GLint active_texture_bak;
    GLint left, right;

    if (view_textures[0] == 0 && view_textures[1] == 0) {
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    /* Backup GL state */
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &current_program_bak);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture_bak);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();

    /* Determine left and right view indices */
    left = (view_textures[0] == 0 ? 1 : 0);
    right = (left == 0 ? 1 : 0);
    if (view_textures[right] == 0)
        right = left;
    if (swap_views) {
        GLint tmp = left;
        left = right;
        right = tmp;
    }
    if ((mode == GLS_MODE_EVEN_ODD_ROWS || mode == GLS_MODE_CHECKERBOARD)
            && (ctx->viewport_screen_y + viewport[1]) % 2 == 0) {
        GLint tmp = left;
        left = right;
        right = tmp;
    }
    if ((mode == GLS_MODE_EVEN_ODD_COLUMNS || mode == GLS_MODE_CHECKERBOARD)
            && (ctx->viewport_screen_x + viewport[0]) % 2 == 1) {
        GLint tmp = left;
        left = right;
        right = tmp;
    }

    /* Initialize GL things */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glDisable(GL_FOG);
    glDisable(GL_NORMALIZE);
    glDisableClientState(GL_COLOR_ARRAY | GL_EDGE_FLAG_ARRAY | GL_FOG_COORD_ARRAY
            | GL_INDEX_ARRAY | GL_NORMAL_ARRAY | GL_SECONDARY_COLOR_ARRAY
            | GL_TEXTURE_COORD_ARRAY | GL_VERTEX_ARRAY);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_TEXTURE_2D);
    if (mode == GLS_MODE_EVEN_ODD_ROWS && ctx->even_odd_rows_mask_tex == 0) {
        GLubyte even_odd_rows_mask[4] = { 0xff, 0xff, 0x00, 0x00 };
        glGenTextures(1, &ctx->even_odd_rows_mask_tex);
        glBindTexture(GL_TEXTURE_2D, ctx->even_odd_rows_mask_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, 2, 2, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE, even_odd_rows_mask);
    }
    if (mode == GLS_MODE_EVEN_ODD_COLUMNS && ctx->even_odd_columns_mask_tex == 0) {
        GLubyte even_odd_columns_mask[4] = { 0xff, 0x00, 0xff, 0x00 };
        glGenTextures(1, &ctx->even_odd_columns_mask_tex);
        glBindTexture(GL_TEXTURE_2D, ctx->even_odd_columns_mask_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, 2, 2, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE, even_odd_columns_mask);
    }
    if (mode == GLS_MODE_CHECKERBOARD && ctx->checkerboard_mask_tex == 0) {
        GLubyte checkerboard_mask[4] = { 0xff, 0x00, 0x00, 0xff };
        glGenTextures(1, &ctx->checkerboard_mask_tex);
        glBindTexture(GL_TEXTURE_2D, ctx->checkerboard_mask_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, 2, 2, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE, checkerboard_mask);
    }
    if (ctx->prg == 0 || ctx->prg_mode != mode
            || ctx->prg_ghostbust != (ctx->ghostbust > 0.0f)) {
        const char* ghostbust_val = ctx->ghostbust > 0.0f
            ? "ghostbust_enabled" : "ghostbust_disabled";
        const char* mode_val =
              mode == GLS_MODE_EVEN_ODD_ROWS ? "mode_even_odd_rows"
            : mode == GLS_MODE_EVEN_ODD_COLUMNS ? "mode_even_odd_columns"
            : mode == GLS_MODE_CHECKERBOARD ? "mode_checkerboard"
            : mode == GLS_MODE_RED_CYAN_MONOCHROME ? "mode_red_cyan_monochrome"
            : mode == GLS_MODE_RED_CYAN_HALF_COLOR ? "mode_red_cyan_half_color"
            : mode == GLS_MODE_RED_CYAN_FULL_COLOR ? "mode_red_cyan_full_color"
            : mode == GLS_MODE_RED_CYAN_DUBOIS ? "mode_red_cyan_dubois"
            : mode == GLS_MODE_GREEN_MAGENTA_MONOCHROME ? "mode_green_magenta_monochrome"
            : mode == GLS_MODE_GREEN_MAGENTA_HALF_COLOR ? "mode_green_magenta_half_color"
            : mode == GLS_MODE_GREEN_MAGENTA_FULL_COLOR ? "mode_green_magenta_full_color"
            : mode == GLS_MODE_GREEN_MAGENTA_DUBOIS ? "mode_green_magenta_dubois"
            : mode == GLS_MODE_AMBER_BLUE_MONOCHROME ? "mode_amber_blue_monochrome"
            : mode == GLS_MODE_AMBER_BLUE_HALF_COLOR ? "mode_amber_blue_half_color"
            : mode == GLS_MODE_AMBER_BLUE_FULL_COLOR ? "mode_amber_blue_full_color"
            : mode == GLS_MODE_AMBER_BLUE_DUBOIS ? "mode_amber_blue_dubois"
            : mode == GLS_MODE_RED_GREEN_MONOCHROME ? "mode_red_green_monochrome"
            : mode == GLS_MODE_RED_BLUE_MONOCHROME ? "mode_red_blue_monochrome"
            : "mode_onechannel";
        char* shader_src = strdup(GLS_GLSL_STR);
        GLuint shader;
        if (shader_src)
            str_replace(&shader_src, "$ghostbust", ghostbust_val);
        if (shader_src)
            str_replace(&shader_src, "$mode", mode_val);
        if (!shader_src)
            oom_abort();
        shader = compile_fragment_shader(ctx, shader_src);
        free(shader_src);
        ctx->prg = glCreateProgram();
        glAttachShader(ctx->prg, shader);
        link_program(ctx, ctx->prg);
        ctx->prg_mode = mode;
        ctx->prg_ghostbust = (ctx->ghostbust > 0.0f);
    }
    glUseProgram(ctx->prg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, view_textures[left]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, view_textures[right]);
    glUniform1i(glGetUniformLocation(ctx->prg, "rgb_l"), 0);
    glUniform1i(glGetUniformLocation(ctx->prg, "rgb_r"), 1);
    glUniform1f(glGetUniformLocation(ctx->prg, "parallax_adjust"),
            ctx->parallax_adjust);
    if (mode != GLS_MODE_RED_CYAN_MONOCHROME
            && mode != GLS_MODE_RED_CYAN_HALF_COLOR
            && mode != GLS_MODE_RED_CYAN_FULL_COLOR
            && mode != GLS_MODE_RED_CYAN_DUBOIS
            && mode != GLS_MODE_GREEN_MAGENTA_MONOCHROME
            && mode != GLS_MODE_GREEN_MAGENTA_HALF_COLOR
            && mode != GLS_MODE_GREEN_MAGENTA_FULL_COLOR
            && mode != GLS_MODE_GREEN_MAGENTA_DUBOIS
            && mode != GLS_MODE_AMBER_BLUE_MONOCHROME
            && mode != GLS_MODE_AMBER_BLUE_HALF_COLOR
            && mode != GLS_MODE_AMBER_BLUE_FULL_COLOR
            && mode != GLS_MODE_AMBER_BLUE_DUBOIS
            && mode != GLS_MODE_RED_BLUE_MONOCHROME
            && mode != GLS_MODE_RED_GREEN_MONOCHROME
            && ctx->ghostbust > 0.0f) {
        glUniform3f(glGetUniformLocation(ctx->prg, "crosstalk"),
                ctx->crosstalk_r * ctx->ghostbust,
                ctx->crosstalk_g * ctx->ghostbust,
                ctx->crosstalk_b * ctx->ghostbust);
    }
    if (mode == GLS_MODE_EVEN_ODD_ROWS || mode == GLS_MODE_EVEN_ODD_COLUMNS
            || mode == GLS_MODE_CHECKERBOARD) {
        glUniform1i(glGetUniformLocation(ctx->prg, "mask_tex"), 2);
        glUniform1f(glGetUniformLocation(ctx->prg, "step_x"), 1.0f / viewport[2]);
        glUniform1f(glGetUniformLocation(ctx->prg, "step_y"), 1.0f / viewport[3]);
    }

    /* Render */
    if (mode == GLS_MODE_QUAD_BUFFER_STEREO) {
        glUniform1f(glGetUniformLocation(ctx->prg, "channel"), 0.0f);
        glDrawBuffer(GL_BACK_LEFT);
        draw_quad(ctx, viewport[2], viewport[3]);
        glUniform1f(glGetUniformLocation(ctx->prg, "channel"), 1.0f);
        glDrawBuffer(GL_BACK_RIGHT);
        draw_quad(ctx, viewport[2], viewport[3]);
    } else if (mode == GLS_MODE_EVEN_ODD_ROWS) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, ctx->even_odd_rows_mask_tex);
        draw_quad(ctx, viewport[2], viewport[3]);
    } else if (mode == GLS_MODE_EVEN_ODD_COLUMNS) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, ctx->even_odd_columns_mask_tex);
        draw_quad(ctx, viewport[2], viewport[3]);
    } else if (mode == GLS_MODE_CHECKERBOARD) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, ctx->checkerboard_mask_tex);
        draw_quad(ctx, viewport[2], viewport[3]);
    } else if (mode == GLS_MODE_RED_CYAN_MONOCHROME
            || mode == GLS_MODE_RED_CYAN_HALF_COLOR
            || mode == GLS_MODE_RED_CYAN_FULL_COLOR
            || mode == GLS_MODE_RED_CYAN_DUBOIS
            || mode == GLS_MODE_GREEN_MAGENTA_MONOCHROME
            || mode == GLS_MODE_GREEN_MAGENTA_HALF_COLOR
            || mode == GLS_MODE_GREEN_MAGENTA_FULL_COLOR
            || mode == GLS_MODE_GREEN_MAGENTA_DUBOIS
            || mode == GLS_MODE_AMBER_BLUE_MONOCHROME
            || mode == GLS_MODE_AMBER_BLUE_HALF_COLOR
            || mode == GLS_MODE_AMBER_BLUE_FULL_COLOR
            || mode == GLS_MODE_AMBER_BLUE_DUBOIS
            || mode == GLS_MODE_RED_GREEN_MONOCHROME
            || mode == GLS_MODE_RED_BLUE_MONOCHROME) {
        draw_quad(ctx, viewport[2], viewport[3]);
    } else if (mode == GLS_MODE_MONO_LEFT
            || (mode == GLS_MODE_ALTERNATING && ctx->display_frame_counter % 2 == 0)) {
        glUniform1f(glGetUniformLocation(ctx->prg, "channel"), 0.0f);
        draw_quad(ctx, viewport[2], viewport[3]);
    } else if (mode == GLS_MODE_MONO_RIGHT
            || (mode == GLS_MODE_ALTERNATING && ctx->display_frame_counter % 2 == 1)) {
        glUniform1f(glGetUniformLocation(ctx->prg, "channel"), 1.0f);
        draw_quad(ctx, viewport[2], viewport[3]);
    } else if (mode == GLS_MODE_LEFT_RIGHT) {
        int hw = viewport[2] / 2;
        glViewport(viewport[0], viewport[1], hw, viewport[3]);
        glUniform1f(glGetUniformLocation(ctx->prg, "channel"), 0.0f);
        draw_quad(ctx, viewport[2], viewport[3]);
        glViewport(viewport[0] + hw, viewport[1], viewport[2] - hw, viewport[3]);
        glUniform1f(glGetUniformLocation(ctx->prg, "channel"), 1.0f);
        draw_quad(ctx, viewport[2], viewport[3]);
    } else if (mode == GLS_MODE_TOP_BOTTOM) {
        int hh = viewport[3] / 2;
        glViewport(viewport[0], viewport[1] + hh, viewport[2], viewport[3] - hh);
        glUniform1f(glGetUniformLocation(ctx->prg, "channel"), 0.0f);
        draw_quad(ctx, viewport[2], viewport[3]);
        glViewport(viewport[0], viewport[1], viewport[2], hh);
        glUniform1f(glGetUniformLocation(ctx->prg, "channel"), 1.0f);
        draw_quad(ctx, viewport[2], viewport[3]);
    } else if (mode == GLS_MODE_HDMI_FRAME_PACK) {
        // HDMI frame packing mode has left view top, right view bottom, plus a
        // blank area separating the two. 720p uses 30 blank lines (total: 720
        // + 30 + 720 = 1470), 1080p uses 45 (total: 10280 + 45 + 1080 = 2205).
        // In both cases, the blank area is 30/1470 = 45/2205 = 1/49 of the
        // total height. See the document "High-Definition Multimedia Interface
        // Specification Version 1.4a Extraction of 3D Signaling Portion" from
        // hdmi.org.
        int blank_lines = viewport[3] / 49;
        int hh = (viewport[3] - blank_lines) / 2;
        glViewport(viewport[0], viewport[1] + hh, viewport[2], blank_lines);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(viewport[0], viewport[1] + hh + blank_lines, viewport[2], viewport[3] - hh - blank_lines);
        glUniform1f(glGetUniformLocation(ctx->prg, "channel"), 0.0f);
        draw_quad(ctx, viewport[2], viewport[3]);
        glViewport(viewport[0], viewport[1], viewport[2], hh);
        glUniform1f(glGetUniformLocation(ctx->prg, "channel"), 1.0f);
        draw_quad(ctx, viewport[2], viewport[3]);
    }

    /* Restore GL state */
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopClientAttrib();
    glPopAttrib();
    glActiveTexture(active_texture_bak);
    glUseProgram(current_program_bak);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void glsDrawDLP3dReadySyncMarker(GLScontext* ctx, GLSmode mode)
{
    const unsigned int R = 0xffu << 16u;
    const unsigned int G = 0xffu << 8u;
    const unsigned int B = 0xffu;
    GLint viewport[4];
    GLfloat raster_pos_bak[4];
    size_t required_size;
    GLint i;

    /* DLP 3-D Ready Sync: draw colored lines to allow the projector
     * to identify the stereo mode and the left / right views automatically. */
    if (mode != GLS_MODE_LEFT_RIGHT
            && mode != GLS_MODE_TOP_BOTTOM
            && mode != GLS_MODE_ALTERNATING) {
        // The sync marker is only defined for the above modes.
        return;
    }

    /* Get current viewport */
    glGetIntegerv(GL_VIEWPORT, viewport);

    if (viewport[0] != 0 || viewport[1] != 0) {
        // The sync marker only makes sense in full screen mode.
        return;
    }

    /* Backup GL state */
    glGetFloatv(GL_CURRENT_RASTER_POSITION, raster_pos_bak);

    /* Manage the buffer for pixel data */
    required_size = viewport[2] * sizeof(unsigned int);
    if (ctx->dlp_3d_ready_sync_buf_size < required_size) {
        ctx->dlp_3d_ready_sync_buf = realloc(ctx->dlp_3d_ready_sync_buf, required_size);
        ctx->dlp_3d_ready_sync_buf_size = required_size;
    }
    if (!ctx->dlp_3d_ready_sync_buf)
        oom_abort();

    /* Draw the marker */
    if (mode == GLS_MODE_LEFT_RIGHT) {
        unsigned int color = (ctx->display_frame_counter % 2 == 0 ? R : G | B);
        for (i = 0; i < viewport[2]; i++)
            ctx->dlp_3d_ready_sync_buf[i] = color;
        glWindowPos2i(0, 0);
        glDrawPixels(viewport[2], 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, ctx->dlp_3d_ready_sync_buf);
    } else if (mode == GLS_MODE_TOP_BOTTOM) {
        unsigned int color = (ctx->display_frame_counter % 2 == 0 ? B : R | G);
        for (i = 0; i < viewport[2]; i++)
            ctx->dlp_3d_ready_sync_buf[i] = color;
        glWindowPos2i(0, 0);
        glDrawPixels(viewport[2], 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, ctx->dlp_3d_ready_sync_buf);
        glWindowPos2i(0, viewport[3] / 2);
        glDrawPixels(viewport[2], 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, ctx->dlp_3d_ready_sync_buf);
    } else if (mode == GLS_MODE_ALTERNATING) {
        unsigned int color = (ctx->display_frame_counter % 4 < 2 ? G : R | B);
        for (i = 0; i < viewport[2]; i++)
            ctx->dlp_3d_ready_sync_buf[i] = color;
        glWindowPos2i(0, 0);
        glDrawPixels(viewport[2], 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, ctx->dlp_3d_ready_sync_buf);
    }

    /* Restore GL state */
    glWindowPos3f(raster_pos_bak[0], raster_pos_bak[1], raster_pos_bak[2]);
}
