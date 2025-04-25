#ifndef PSRC_ENGINE_RENDERER_SW_H
#define PSRC_ENGINE_RENDERER_SW_H

#include <stddef.h>
#include <stdbool.h>

void r_sw_render(void);
void r_sw_display(void);
void* r_sw_takeScreenshot(unsigned* w, unsigned* h, size_t* sz);
bool r_sw_beforeCreateWindow(unsigned*);
bool r_sw_afterCreateWindow(void);
bool r_sw_prepRenderer(void);
void r_sw_beforeDestroyWindow(void);
void r_sw_updateFrame(void);
void r_sw_updateVSync(void);

#endif
