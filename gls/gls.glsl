/*
 * This file is part of libgls, a library for stereoscopic OpenGL rendering.
 * It was adapted from video_output_render.fs.glsl from the Bino 3D video
 * player on 2012-10-02. The copyright holders (listed below) explicitly agreed
 * with relicensing under the MIT/X11 license.
 *
 * Copyright (C) 2010, 2011, 2012
 * Martin Lambers <marlam@marlam.de>
 * Frederic Devernay <Frederic.Devernay@inrialpes.fr>
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

#version 110

// mode_onechannel
// mode_red_cyan_monochrome
// mode_red_cyan_half_color
// mode_red_cyan_full_color
// mode_red_cyan_dubois
// mode_green_magenta_monochrome
// mode_green_magenta_half_color
// mode_green_magenta_full_color
// mode_green_magenta_dubois
// mode_amber_blue_monochrome
// mode_amber_blue_half_color
// mode_amber_blue_full_color
// mode_amber_blue_dubois
// mode_red_green_monochrome
// mode_red_blue_monochrome
// mode_even_odd_rows
// mode_even_odd_columns
// mode_checkerboard
#define $mode

// ghostbust_enabled
// ghostbust_disabled
#define $ghostbust

uniform sampler2D rgb_l;
uniform sampler2D rgb_r;
uniform float parallax_adjust;

#if defined(ghostbust_enabled) && (defined(mode_onechannel) || defined(mode_even_odd_rows) || defined(mode_even_odd_columns) || defined(mode_checkerboard))
uniform vec3 crosstalk;
#endif

#if defined(mode_onechannel)
uniform float channel;  // 0.0 for left, 1.0 for right
#endif

#if defined(mode_even_odd_rows) || defined(mode_even_odd_columns) || defined(mode_checkerboard)
uniform sampler2D mask_tex;
uniform float step_x;
uniform float step_y;
#endif


#if defined(mode_red_cyan_monochrome) || defined(mode_red_cyan_half_color) || defined(mode_green_magenta_monochrome) || defined(mode_green_magenta_half_color) || defined(mode_amber_blue_monochrome) || defined(mode_amber_blue_half_color) || defined(mode_red_green_monochrome) || defined(mode_red_blue_monochrome)
float rgb_to_lum(vec3 rgb)
{
    // Values taken from http://www.opengl.org/archives/resources/features/KilgardTechniques/oglpitfall/
    // Section 6, "Reading Back Luminance Pixels"
    return dot(rgb, vec3(0.299, 0.587, 0.114));
}
#endif

#if defined(mode_onechannel) || defined(mode_even_odd_rows) || defined(mode_even_odd_columns) || defined(mode_checkerboard)
#  if defined(ghostbust_enabled)
vec3 ghostbust(vec3 original, vec3 other)
{
    return original + crosstalk - (other + original) * crosstalk;
}
#  else
#    define ghostbust(original, other) original
#  endif
#endif

vec3 tex_l(vec2 texcoord)
{
    return texture2D(rgb_l, texcoord + vec2(parallax_adjust, 0.0)).rgb;
}
vec3 tex_r(vec2 texcoord)
{
    return texture2D(rgb_r, texcoord - vec2(parallax_adjust, 0.0)).rgb;
}

void main()
{
    vec3 l, r;
    vec3 result;

#if defined(mode_onechannel)

    l = tex_l(gl_TexCoord[0].xy);
    r = tex_r(gl_TexCoord[1].xy);
    result = ghostbust(mix(l, r, channel), mix(r, l, channel));

#elif defined(mode_even_odd_rows) || defined(mode_even_odd_columns) || defined(mode_checkerboard)

    /* This implementation of the masked modes works around many different problems and therefore may seem strange.
     * Why not use stencil buffers?
     *  - Because we want to filter, to account for masked out features
     *  - Because stencil did not work with some drivers when switching fullscreen on/off
     * Why not use polygon stipple?
     *  - Because we want to filter, to account for masked out features
     *  - Because polygon stippling may not be hardware accelerated and is currently broken with many free drivers
     * Why use a mask texture? Why not use the mod() function to check for even/odd pixels?
     *  - Because mod() is broken with many drivers, and I found no reliable way to work around it. Some
     *    drivers seem to use extremely low precision arithmetic in the shaders; too low for reliable pixel
     *    position computations.
     */
    float m = texture2D(mask_tex, gl_TexCoord[2].xy).x;
# if defined(mode_even_odd_rows)
    vec3 rgb0_l = tex_l(gl_TexCoord[0].xy - vec2(0.0, step_y));
    vec3 rgb1_l = tex_l(gl_TexCoord[0].xy);
    vec3 rgb2_l = tex_l(gl_TexCoord[0].xy + vec2(0.0, step_y));
    vec3 rgbc_l = (rgb0_l + 2.0 * rgb1_l + rgb2_l) / 4.0;
    vec3 rgb0_r = tex_r(gl_TexCoord[1].xy - vec2(0.0, step_y));
    vec3 rgb1_r = tex_r(gl_TexCoord[1].xy);
    vec3 rgb2_r = tex_r(gl_TexCoord[1].xy + vec2(0.0, step_y));
    vec3 rgbc_r = (rgb0_r + 2.0 * rgb1_r + rgb2_r) / 4.0;
# elif defined(mode_even_odd_columns)
    vec3 rgb0_l = tex_l(gl_TexCoord[0].xy - vec2(step_x, 0.0));
    vec3 rgb1_l = tex_l(gl_TexCoord[0].xy);
    vec3 rgb2_l = tex_l(gl_TexCoord[0].xy + vec2(step_x, 0.0));
    vec3 rgbc_l = (rgb0_l + 2.0 * rgb1_l + rgb2_l) / 4.0;
    vec3 rgb0_r = tex_r(gl_TexCoord[1].xy - vec2(step_x, 0.0));
    vec3 rgb1_r = tex_r(gl_TexCoord[1].xy);
    vec3 rgb2_r = tex_r(gl_TexCoord[1].xy + vec2(step_x, 0.0));
    vec3 rgbc_r = (rgb0_r + 2.0 * rgb1_r + rgb2_r) / 4.0;
# elif defined(mode_checkerboard)
    vec3 rgb0_l = tex_l(gl_TexCoord[0].xy - vec2(0.0, step_y));
    vec3 rgb1_l = tex_l(gl_TexCoord[0].xy - vec2(step_x, 0.0));
    vec3 rgb2_l = tex_l(gl_TexCoord[0].xy);
    vec3 rgb3_l = tex_l(gl_TexCoord[0].xy + vec2(step_x, 0.0));
    vec3 rgb4_l = tex_l(gl_TexCoord[0].xy + vec2(0.0, step_y));
    vec3 rgbc_l = (rgb0_l + rgb1_l + 4.0 * rgb2_l + rgb3_l + rgb4_l) / 8.0;
    vec3 rgb0_r = tex_r(gl_TexCoord[1].xy - vec2(0.0, step_y));
    vec3 rgb1_r = tex_r(gl_TexCoord[1].xy - vec2(step_x, 0.0));
    vec3 rgb2_r = tex_r(gl_TexCoord[1].xy);
    vec3 rgb3_r = tex_r(gl_TexCoord[1].xy + vec2(step_x, 0.0));
    vec3 rgb4_r = tex_r(gl_TexCoord[1].xy + vec2(0.0, step_y));
    vec3 rgbc_r = (rgb0_r + rgb1_r + 4.0 * rgb2_r + rgb3_r + rgb4_r) / 8.0;
# endif
    result = ghostbust(mix(rgbc_r, rgbc_l, m), mix(rgbc_l, rgbc_r, m));

#elif defined(mode_red_cyan_dubois) || defined(mode_green_magenta_dubois) || defined(mode_amber_blue_dubois)

    // The Dubois anaglyph method is generally the highest quality anaglyph method.
    // Authors page: http://www.site.uottawa.ca/~edubois/anaglyph/
    // This method depends on the characteristics of the display device and the anaglyph glasses.
    // According to the author, the matrices below are intended to be applied to linear RGB values,
    // and are designed for CRT displays.
    l = tex_l(gl_TexCoord[0].xy);
    r = tex_r(gl_TexCoord[1].xy);
# if defined(mode_red_cyan_dubois)
    // Source of this matrix: http://www.site.uottawa.ca/~edubois/anaglyph/LeastSquaresHowToPhotoshop.pdf
    mat3 m0 = mat3(
             0.437, -0.062, -0.048,
             0.449, -0.062, -0.050,
             0.164, -0.024, -0.017);
    mat3 m1 = mat3(
            -0.011,  0.377, -0.026,
            -0.032,  0.761, -0.093,
            -0.007,  0.009,  1.234);
# elif defined(mode_green_magenta_dubois)
    // Source of this matrix: http://www.flickr.com/photos/e_dubois/5132528166/
    mat3 m0 = mat3(
            -0.062,  0.284, -0.015,
            -0.158,  0.668, -0.027,
            -0.039,  0.143,  0.021);
    mat3 m1 = mat3(
             0.529, -0.016,  0.009,
             0.705, -0.015,  0.075,
             0.024, -0.065,  0.937);
# elif defined(mode_amber_blue_dubois)
    // Source of this matrix: http://www.flickr.com/photos/e_dubois/5230654930/
    mat3 m0 = mat3(
             1.062, -0.026, -0.038,
            -0.205,  0.908, -0.173,
             0.299,  0.068,  0.022);
    mat3 m1 = mat3(
            -0.016,  0.006,  0.094,
            -0.123,  0.062,  0.185,
            -0.017, -0.017,  0.911);
# endif
    result = m0 * l + m1 * r;

#else // lower quality anaglyph methods

    l = tex_l(gl_TexCoord[0].xy);
    r = tex_r(gl_TexCoord[1].xy);
# if defined(mode_red_cyan_monochrome)
    result = vec3(rgb_to_lum(l), rgb_to_lum(r), rgb_to_lum(r));
# elif defined(mode_red_cyan_half_color)
    result = vec3(rgb_to_lum(l), r.g, r.b);
# elif defined(mode_red_cyan_full_color)
    result = vec3(l.r, r.g, r.b);
# elif defined(mode_green_magenta_monochrome)
    result = vec3(rgb_to_lum(r), rgb_to_lum(l), rgb_to_lum(r));
# elif defined(mode_green_magenta_half_color)
    result = vec3(r.r, rgb_to_lum(l), r.b);
# elif defined(mode_green_magenta_full_color)
    result = vec3(r.r, l.g, r.b);
# elif defined(mode_amber_blue_monochrome)
    result = vec3(rgb_to_lum(l), rgb_to_lum(l), rgb_to_lum(r));
# elif defined(mode_amber_blue_half_color)
    result = vec3(rgb_to_lum(l), rgb_to_lum(l), r.b);
# elif defined(mode_amber_blue_full_color)
    result = vec3(l.r, l.g, r.b);
# elif defined(mode_red_green_monochrome)
    result = vec3(rgb_to_lum(l), rgb_to_lum(r), 0.0);
# elif defined(mode_red_blue_monochrome)
    result = vec3(rgb_to_lum(l), 0.0, rgb_to_lum(r));
# endif

#endif

    gl_FragColor = vec4(result, 1.0);
}
