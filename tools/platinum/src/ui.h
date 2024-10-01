#ifndef PLATINUM_UI_H
#define PLATINUM_UI_H

#include "input.h"

struct ui_menu;
struct ui_menu* ui_newmenu(char* name, struct key, ...);
void ui_delmenu(struct ui_menu*);

#define UI_WINFLAG_NOCLOSE (1 << 0)
#define UI_WINFLAG_NORESZ  (1 << 1)
#define UI_WINFLAG_NOMIN   (1 << 2)
#define UI_WINFLAG_DIALOG  (1 << 3)
int ui_newwin(int p, char* t, int x, int y, int w, int h, struct ui_menu*, char* s, unsigned f);
void ui_editwin(int, char* t, int x, int y, int w, int h, struct ui_menu*, char* s);
void ui_delwin(int);

enum uielem {
    UIELEM_CONTAINER
};
int ui_newelem(int win, enum uielem, ...);

#endif
