#include "GLContext.hpp"

#include "Error.hpp"

struct GLVersion {
	int major;
	int minor;
};

GLVersion version[] = {
	{4, 5},
	{4, 4},
	{4, 3},
	{4, 2},
	{4, 1},
	{4, 0},
	{3, 3},
	{3, 2},
	{3, 1},
	{0, 0},
};

int versions = sizeof(version) / sizeof(GLVersion);

#if defined(_WIN32) || defined(_WIN64)

#include <Windows.h>

#define WGL_ACCELERATION                0x2003
#define WGL_FULL_ACCELERATION           0x2027
#define WGL_CONTEXT_MAJOR_VERSION       0x2091
#define WGL_CONTEXT_MINOR_VERSION       0x2092
#define WGL_CONTEXT_PROFILE_MASK        0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT    0x0001

typedef int (WINAPI * mglChoosePixelFormatProc)(HDC hdc, const int * piAttribIList, const float * pfAttribFList, unsigned nMaxFormats, int * piFormats, unsigned * nNumFormats);
typedef HGLRC (WINAPI * mglCreateContextAttribsProc)(HDC hdc, HGLRC hglrc, const int * attribList);

mglChoosePixelFormatProc mglChoosePixelFormat;
mglCreateContextAttribsProc mglCreateContextAttribs;

PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),  // nSize
	1,                              // nVersion
	PFD_DRAW_TO_WINDOW |
	PFD_SUPPORT_OPENGL |
	PFD_GENERIC_ACCELERATED |
	PFD_DOUBLEBUFFER,               // dwFlags
	0,                              // iPixelType
	32,                             // cColorBits
	0,                              // cRedBits
	0,                              // cRedShift
	0,                              // cGreenBits
	0,                              // cGreenShift
	0,                              // cBlueBits
	0,                              // cBlueShift
	0,                              // cAlphaBits
	0,                              // cAlphaShift
	0,                              // cAccumBits
	0,                              // cAccumRedBits
	0,                              // cAccumGreenBits
	0,                              // cAccumBlueBits
	0,                              // cAccumAlphaBits
	24,                             // cDepthBits
	0,                              // cStencilBits
	0,                              // cAuxBuffers
	0,                              // iLayerType
	0,                              // bReserved
	0,                              // dwLayerMask
	0,                              // dwVisibleMask
	0,                              // dwDamageMask
};

GLContext LoadCurrentGLContext() {
	GLContext context = {};

	HGLRC hrc = wglGetCurrentContext();

	if (!hrc) {
		MGLError * error = MGLError_FromFormat(TRACE, "cannot detect context");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	HDC hdc = wglGetCurrentDC();

	if (!hdc) {
		MGLError * error = MGLError_FromFormat(TRACE, "cannot detect device content");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	HWND hwnd = WindowFromDC(hdc);

	if (!hwnd) {
		MGLError * error = MGLError_FromFormat(TRACE, "cannot detect window");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	context.hwnd = (void *)hwnd;
	context.hdc = (void *)hdc;
	context.hrc = (void *)hrc;

	context.standalone = false;

	return context;
}

void InitModernContext() {
	static bool initialized = false;

	if (initialized) {
		return;
	}

	initialized = true;

	HMODULE hinst = GetModuleHandle(0);

	if (!hinst) {
		return;
	}

	WNDCLASS extClass = {
		CS_OWNDC,                       // style
		DefWindowProc,                  // lpfnWndProc
		0,                              // cbClsExtra
		0,                              // cbWndExtra
		hinst,                          // hInstance
		0,                              // hIcon
		0,                              // hCursor
		0,                              // hbrBackground
		0,                              // lpszMenuName
		"ContextLoader",                // lpszClassName
	};

	if (!RegisterClass(&extClass)) {
		return;
	}

	HWND loader_hwnd = CreateWindow(
		"ContextLoader",                // lpClassName
		0,                              // lpWindowName
		0,                              // dwStyle
		0,                              // x
		0,                              // y
		0,                              // nWidth
		0,                              // nHeight
		0,                              // hWndParent
		0,                              // hMenu
		hinst,                          // hInstance
		0                               // lpParam
	);

	if (!loader_hwnd) {
		return;
	}

	HDC loader_hdc = GetDC(loader_hwnd);

	if (!loader_hdc) {
		return;
	}

	int loader_pixelformat = ChoosePixelFormat(loader_hdc, &pfd);

	if (!loader_pixelformat) {
		return;
	}

	if (!SetPixelFormat(loader_hdc, loader_pixelformat, &pfd)) {
		return;
	}

	HGLRC loader_hglrc = wglCreateContext(loader_hdc);

	if (!loader_hglrc) {
		return;
	}

	if (!wglMakeCurrent(loader_hdc, loader_hglrc)) {
		return;
	}

	mglCreateContextAttribs = (mglCreateContextAttribsProc)wglGetProcAddress("wglCreateContextAttribsARB");

	if (!mglCreateContextAttribs) {
		return;
	}

	mglChoosePixelFormat = (mglChoosePixelFormatProc)wglGetProcAddress("wglChoosePixelFormatARB");

	if (!mglChoosePixelFormat) {
		return;
	}

	if (!wglMakeCurrent(0, 0)) {
		return;
	}

	if (!wglDeleteContext(loader_hglrc)) {
		return;
	}

	if (!ReleaseDC(loader_hwnd, loader_hdc)) {
		return;
	}

	if (!DestroyWindow(loader_hwnd)) {
		return;
	}

	if (!UnregisterClass("ContextLoader", hinst)) {
		return;
	}
}

GLContext CreateGLContext(int width, int height) {
	GLContext context = {};

	InitModernContext();

	HINSTANCE inst = GetModuleHandle(0);

	if (!inst) {
		MGLError * error = MGLError_FromFormat(TRACE, "module handle is null");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	static bool registered = false;

	if (!registered) {
		WNDCLASS wndClass = {
			CS_OWNDC,                            // style
			DefWindowProc,                       // lpfnWndProc
			0,                                   // cbClsExtra
			0,                                   // cbWndExtra
			inst,                                // hInstance
			0,                                   // hIcon
			0,                                   // hCursor
			0,                                   // hbrBackground
			0,                                   // lpszMenuName
			"StandaloneContext",                 // lpszClassName
		};

		if (!RegisterClass(&wndClass)) {
			MGLError * error = MGLError_FromFormat(TRACE, "cannot register window class");
			PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
			return context;
		}

		registered = true;
	}

	HWND hwnd = CreateWindowEx(
		0,                                   // exStyle
		"StandaloneContext",                 // lpClassName
		0,                                   // lpWindowName
		0,                                   // dwStyle
		0,                                   // x
		0,                                   // y
		0,                                   // nWidth
		0,                                   // nHeight
		0,                                   // hWndParent
		0,                                   // hMenu
		inst,                                // hInstance
		0                                    // lpParam
	);

	if (!hwnd) {
		MGLError * error = MGLError_FromFormat(TRACE, "cannot create window");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	HDC hdc = GetDC(hwnd);

	if (!hdc) {
		MGLError * error = MGLError_FromFormat(TRACE, "cannot create device content");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	HGLRC hrc = 0;

	if (mglCreateContextAttribs && mglChoosePixelFormat) {

		int pixelformat = 0;
		unsigned num_formats = 0;

		// WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB

		if (!mglChoosePixelFormat(hdc, 0, 0, 1, &pixelformat, &num_formats)) {
			MGLError * error = MGLError_FromFormat(TRACE, "cannot choose pixel format");
			PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
			return context;
		}

		if (!num_formats) {
			MGLError * error = MGLError_FromFormat(TRACE, "no pixel formats available");
			PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
			return context;
		}

		if (!SetPixelFormat(hdc, pixelformat, &pfd)) {
			MGLError * error = MGLError_FromFormat(TRACE, "cannot set pixel format");
			PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
			return context;
		}

		for (int i = 0; i < versions; ++i) {
			int attribs[] = {
				WGL_CONTEXT_PROFILE_MASK, WGL_CONTEXT_CORE_PROFILE_BIT,
				WGL_CONTEXT_MAJOR_VERSION, version[i].major,
				WGL_CONTEXT_MINOR_VERSION, version[i].minor,
				0, 0,
			};

			hrc = mglCreateContextAttribs(hdc, 0, attribs);

			if (hrc) {
				break;
			}
		}

	} else {

		int pf = ChoosePixelFormat(hdc, &pfd);

		if (!pf) {
			MGLError * error = MGLError_FromFormat(TRACE, "cannot choose pixel format");
			PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
			return context;
		}

		int set_pixel_format = SetPixelFormat(hdc, pf, &pfd);

		if (!set_pixel_format) {
			MGLError * error = MGLError_FromFormat(TRACE, "cannot set pixel format");
			PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
			return context;
		}

		hrc = wglCreateContext(hdc);

	}

	if (!hrc) {
		MGLError * error = MGLError_FromFormat(TRACE, "cannot create OpenGL context");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	int make_current = wglMakeCurrent(hdc, hrc);

	if (!make_current) {
		MGLError * error = MGLError_FromFormat(TRACE, "cannot select OpenGL context");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	context.hwnd = (void *)hwnd;
	context.hrc = (void *)hrc;
	context.hdc = (void *)hdc;

	context.standalone = true;

	return context;
}

void DestroyGLContext(const GLContext & context) {
	if (!context.standalone) {
		return;
	}

	if (wglGetCurrentContext() == context.hrc) {
		wglMakeCurrent(0, 0);
	}

	wglDeleteContext((HGLRC)context.hrc);
	ReleaseDC((HWND)context.hwnd, (HDC)context.hdc);
	DestroyWindow((HWND)context.hwnd);
}

#elif defined(__APPLE__)

#include <OpenGL/gl.h>

GLContext LoadCurrentGLContext() {
	GLContext context = {};
	return context;
}

GLContext CreateGLContext(int width, int height) {
	GLContext context = {};
	return context;
}

void DestroyGLContext(const GLContext & context) {
	// TODO:
}

#else

#include <GL/glx.h>
#include <GL/gl.h>

#define GLX_CONTEXT_MAJOR_VERSION 0x2091
#define GLX_CONTEXT_MINOR_VERSION 0x2092
#define GLX_CONTEXT_PROFILE_MASK 0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT 0x0001

typedef GLXContext (* GLXCREATECONTEXTATTRIBSARBPROC)(Display * display, GLXFBConfig config, GLXContext context, Bool direct, const int * attribs);

GLContext LoadCurrentGLContext() {
	GLContext context = {};

	Display * dpy = glXGetCurrentDisplay();

	if (!dpy) {
		MGLError * error = MGLError_FromFormat(TRACE, "cannot detect display");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	Window win = glXGetCurrentDrawable();

	if (!win) {
		MGLError * error = MGLError_FromFormat(TRACE, "cannot detect window");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	GLXContext ctx = glXGetCurrentContext();

	if (!ctx) {
		MGLError * error = MGLError_FromFormat(TRACE, "cannot detect context");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	context.display = (void *)dpy;
	context.window = (void *)win;
	context.context = (void *)ctx;

	context.standalone = false;

	return context;
}

int SilentXErrorHandler(Display * d, XErrorEvent * e) {
    return 0;
}

GLContext CreateGLContext(int width, int height) {
	GLContext context = {};

	Display * dpy = XOpenDisplay(0);

	if (!dpy) {
		MGLError * error = MGLError_FromFormat(TRACE, "cannot detect the display");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	int nelements = 0;

	GLXFBConfig * fbc = glXChooseFBConfig(dpy, DefaultScreen(dpy), 0, &nelements);

	if (!fbc) {
		MGLError * error = MGLError_FromFormat(TRACE, "cannot read the display configuration");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		XCloseDisplay(dpy);
		return context;
	}

	static int attributeList[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1, None};

	XVisualInfo * vi = glXChooseVisual(dpy, DefaultScreen(dpy), attributeList);

	if (!vi) {
		XCloseDisplay(dpy);

		MGLError * error = MGLError_FromFormat(TRACE, "cannot choose a visual info");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	XSetWindowAttributes swa;
	swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
	swa.border_pixel = 0;
	swa.event_mask = StructureNotifyMask;

	Window win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, width, height, 0, vi->depth, InputOutput, vi->visual, CWBorderPixel | CWColormap | CWEventMask, &swa);

	if (!win) {
		XCloseDisplay(dpy);

		MGLError * error = MGLError_FromFormat(TRACE, "cannot create window");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	// XMapWindow(dpy, win);

	GLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB = (GLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress((const GLubyte *)"glXCreateContextAttribsARB");

	GLXContext ctx = 0;

	XSetErrorHandler(SilentXErrorHandler);

	if (glXCreateContextAttribsARB) {
		for (int i = 0; i < versions; ++i) {
			int attribs[] = {
				GLX_CONTEXT_PROFILE_MASK, GLX_CONTEXT_CORE_PROFILE_BIT,
				GLX_CONTEXT_MAJOR_VERSION, version[i].major,
				GLX_CONTEXT_MINOR_VERSION, version[i].minor,
				0, 0,
			};

			ctx = glXCreateContextAttribsARB(dpy, *fbc, 0, true, attribs);

			if (ctx) {
				break;
			}
		}
	}

	if (!ctx) {
		ctx = glXCreateContext(dpy, vi, 0, GL_TRUE);
	}

	if (!ctx) {
		XDestroyWindow(dpy, win);
		XCloseDisplay(dpy);

		MGLError * error = MGLError_FromFormat(TRACE, "cannot create OpenGL context");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	XSetErrorHandler(0);

	int make_current = glXMakeCurrent(dpy, win, ctx);

	if (!make_current) {
		glXDestroyContext(dpy, ctx);
		XDestroyWindow(dpy, win);
		XCloseDisplay(dpy);

		MGLError * error = MGLError_FromFormat(TRACE, "cannot select OpenGL context");
		PyErr_SetObject((PyObject *)&MGLError_Type, (PyObject *)error);
		return context;
	}

	context.display = (void *)dpy;
	context.window = (void *)win;
	context.context = (void *)ctx;

	context.standalone = true;

	return context;
}

void DestroyGLContext(const GLContext & context) {
	if (!context.standalone) {
		return;
	}

	if (context.display) {
		glXMakeCurrent((Display *)context.display, 0, 0);

		if (context.context) {
			glXDestroyContext((Display *)context.display, (GLXContext)context.context);
			// context.context = 0;
		}

		if (context.window) {
			XDestroyWindow((Display *)context.display, (Window)context.window);
			// context.window = 0;
		}

		XCloseDisplay((Display *)context.display);
		// context.display = 0;
	}
}

#endif
