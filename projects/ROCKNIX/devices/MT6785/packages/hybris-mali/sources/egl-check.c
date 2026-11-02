// SPDX-License-Identifier: Apache-2.0
// Boot-time check that the stock Mali blob renders through libhybris
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>

int main(void)
{
	EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
			      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
			      EGL_NONE };
	EGLint pb_attr[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
	EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	GLubyte px[4] = { 0 };
	EGLConfig cfg;
	EGLint n;

	EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (!eglInitialize(dpy, NULL, NULL) || !eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || n < 1) {
		fprintf(stderr, "egl-check: no EGL config (0x%x)\n", eglGetError());
		return 1;
	}
	EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb_attr);
	EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
	/* twice: a lost per-thread EGL state only shows on the second call */
	if (!eglMakeCurrent(dpy, surf, surf, ctx) || !eglMakeCurrent(dpy, surf, surf, ctx)) {
		fprintf(stderr, "egl-check: eglMakeCurrent failed (0x%x)\n", eglGetError());
		return 1;
	}
	glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
	if (px[0] != 64 || px[1] != 128 || px[2] != 191) {
		fprintf(stderr, "egl-check: read back %u,%u,%u\n", px[0], px[1], px[2]);
		return 1;
	}
	printf("egl-check: %s %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglTerminate(dpy);
	return 0;
}
