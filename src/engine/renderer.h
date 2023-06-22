#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include <stdbool.h>

bool initRenderer(void);
bool startRenderer(void);
bool restartRenderer(void);
bool stopRenderer(void);
bool deinitRenderer(void);
void render(void);

#endif
