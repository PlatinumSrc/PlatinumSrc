#ifndef PSRC_ENGINE_RENDERER_GL_H
#define PSRC_ENGINE_RENDERER_GL_H

#include <stddef.h>
#include <stdbool.h>

extern void (*r_gl_render)(void);
void r_gl_display(void);
void* r_gl_takeScreenshot(unsigned* w, unsigned* h, unsigned* ch);
bool r_gl_beforeCreateWindow(unsigned*);
bool r_gl_afterCreateWindow(void);
bool r_gl_prepRenderer(void);
void r_gl_beforeDestroyWindow(void);
void r_gl_updateFrame(void);
void r_gl_updateVSync(void);

#endif
