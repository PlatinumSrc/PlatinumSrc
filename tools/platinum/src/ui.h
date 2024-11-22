#ifndef PLATINUM_UI_H
#define PLATINUM_UI_H

#include <stdbool.h>
#include <common/string.h>

enum ui_layerevent {
    UI_LAYEREVENT_DELETE,
    UI_LAYEREVENT_RESIZE
};
union ui_layereventdata {
    struct {
        int w, h;
    } resize;
};
typedef bool (*ui_layercallback)(int layer, enum ui_layerevent, union ui_layereventdata*, void*);
enum ui_layer {
    UI_LAYER_CANVAS,
    UI_LAYER_DIALOG,
    UI_LAYER_MENU
};
enum ui_layerprop {
    UI_LAYERPROP__END,
    UI_LAYERPROP_RESET, // enum ui_layerprop
    UI_LAYERPROP_CB, // ui_layercallback
    UI_LAYERPROP_CBCTX, // void*
    UI_LAYERPROP_TITLE, // char*
    UI_LAYERPROP_X, // int
    UI_LAYERPROP_Y, // int
    UI_LAYERPROP_W, // int
    UI_LAYERPROP_H, // int
    UI_LAYERPROP_MENU_ITEMS // int ct, char** items, int* impinds
};
int ui_layer_push(enum ui_layer, ...);
#define ui_layer_push(...) ui_layer_push(__VA_ARGS__, UI_LAYERPROP__END)
void ui_layer_edit(int, ...);
#define ui_layer_edit(...) ui_layer_edit(__VA_ARGS__, UI_LAYERPROP__END)
void ui_layer_pop(void);

enum ui_elemevent {
    UI_ELEMEVENT_DELETE,
    UI_ELEMEVENT_TEXTBOX_ADDCHAR,
    UI_ELEMEVENT_TEXTBOX_DELCHAR,
    UI_ELEMEVENT_TEXTBOX_PASTE,
    UI_ELEMEVENT_TEXTBOX_UPDATE,
    UI_ELEMEVENT_TEXTBOX_ENTER,
    UI_ELEMEVENT_TEXTAREA_UPDATE,
    UI_ELEMEVENT_BUTTON_PRESS,
    UI_ELEMEVENT_CHECKBOX_TOGGLE,
    UI_ELEMEVENT_MENUBAR_SELECT
};
union ui_elemeventdata {
    struct {
        struct charbuf* buffer;
        char c;
        int at;
        bool block;
    } textbox_addchar;
    struct {
        struct charbuf* buffer;
        int at;
        bool block;
    } textbox_delchar;
    struct {
        struct charbuf* buffer;
        char* text;
        int at;
        bool block;
    } textbox_paste;
    struct {
        struct charbuf* buffer;
    } textbox_update;
    struct {
        struct charbuf* buffer;
    } textbox_enter;
    struct {
        struct charbuf* buffer;
    } textarea_update;
    struct {
        bool checked;
    } checkbox_toggle;
    struct {
        int item;
        int menuy;
        int menuxl;
        int menuxr;
    } menubar_select;
};
typedef bool (*ui_elemcallback)(int layer, int elem, enum ui_elemevent, union ui_elemeventdata*, void*);
enum ui_elem {
    UI_ELEM_BACKGROUND,
    UI_ELEM_LABEL,
    UI_ELEM_TEXTBOX,
    UI_ELEM_TEXTAREA,
    UI_ELEM_BUTTON,
    UI_ELEM_CHECKBOX,
    UI_ELEM_RADIO,
    UI_ELEM_LIST,
    UI_ELEM_DROPDOWN,
    UI_ELEM_MENUBAR,
    UI_ELEM_STATUSBAR,
    UI_ELEM_TITLE
};
enum ui_elemprop {
    UI_ELEMPROP__END,
    UI_ELEMPROP_RESET, // enum ui_elemprop
    UI_ELEMPROP_CB, // ui_elemcallback
    UI_ELEMPROP_CBCTX, // void*
    UI_ELEMPROP_X, // int
    UI_ELEMPROP_Y, // int
    UI_ELEMPROP_W, // int
    UI_ELEMPROP_H, // int
    UI_ELEMPROP_TEXT, // char*
    UI_ELEMPROP_CHECKBOX_CHECKED, // unsigned
    UI_ELEMPROP_RADIO_SELECTED, // unsigned
    UI_ELEMPROP_LIST_ITEMS, // int ct, char** items
    UI_ELEMPROP_DROPDOWN_ITEMS, // int ct, char** items, int* impinds
    UI_ELEMPROP_MENUBAR_ITEMS // int ct, char** items, int* impinds
};
int ui_elem_new(int layer, ...);
#define ui_elem_new(...) ui_elem_new(__VA_ARGS__, UI_ELEMPROP_END)
void ui_elem_edit(int layer, int elem, ...);
#define ui_elem_edit(...) ui_elem_edit(__VA_ARGS__, UI_ELEMPROP_END)
void ui_elem_del(int layer, int elem);

enum ui_color {
    UI_COLOR_TEXT,
    UI_COLOR__CONTINUE
};
void ui_setcolors(uint16_t*);

#endif
