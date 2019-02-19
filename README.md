# Libgls: a library for stereoscopic rendering using OpenGL

The libgls library allows stereoscopic rendering with OpenGL, without requiring hardware support for quad-buffered stereo.

Many stereoscopic display modes are supported, from anaglyph glasses to various
types of 3D TVs and stereoscopic displays.  Most of the code comes from the
[Bino 3D video player](https://bino3d.org/) and thus is already known to work
on a wide range of OpenGl implementations. Nevertheless, if you find a problem,
please let us know.

Libgls is available under the MIT/X11 license, which means it can be used both
in free and proprietary software without problems.

Requirements:

- [GLEW MX](http://glew.sourceforge.net/)
- [GLUT](http://freeglut.sourceforge.net/) (optional, only used for the example program)
