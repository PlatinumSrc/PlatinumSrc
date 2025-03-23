#ifndef PSRC_ENGINE_UI_H
#define PSRC_ENGINE_UI_H

#include "../resource.h"
#include "../attribs.h"
#include "../vlb.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

PACKEDENUM uielemtype {
    UIELEMTYPE_BOX,
    UIELEMTYPE_LABEL,
    UIELEMTYPE_TEXTBOX,
    UIELEMTYPE_BUTTON,
    UIELEMTYPE_CHECKBOX,
    UIELEMTYPE_RADIO,
    UIELEMTYPE_SLIDER,
    UIELEMTYPE_PROGRESSBAR,
    UIELEMTYPE_SCROLLBAR,
    UIELEMTYPE_DROPDOWN,
    UIELEMTYPE_LIST,
    UIELEMTYPE_TABS,
    UIELEMTYPE_MENU,
    UIELEMTYPE_MEDIA,
    UIELEMTYPE_SEPARATOR,
    UIELEMTYPE__COUNT
};

enum uielemprop {
    UIELEMPROP_COMMON_CALLBACK, // uielemcallback
    UIELEMPROP_COMMON_HIDDEN, // unsigned
    UIELEMPROP_COMMON_DISABLED, // unsigned
    UIELEMPROP_COMMON_X, // double
    UIELEMPROP_COMMON_Y, // double
    UIELEMPROP_COMMON_W, // double
    UIELEMPROP_COMMON_H, // double
    UIELEMPROP_COMMON_TOOLTIP, // char*
    UIELEMPROP_COMMON_TOOLTIP_FONT, // size_t ct, char**
    UIELEMPROP_COMMON_TOOLTIP_FRMT, // struct ui_textfrmt*
    UIELEMPROP_COMMON_TOOLTIP_INTERPESC, // unsigned
    UIELEMPROP_COMMON_TAGS, // char* ... NULL
    UIELEMPROP_COMMON_RMTAGS, // char* ... NULL

    UIELEMPROP_BOX_COLOR, // uint32_t
    UIELEMPROP_BOX_BACKGROUND, // char*
    UIELEMPROP_BOX_BORDER, // char*
    UIELEMPROP_BOX_IMAGE, // struct ui_image*
    UIELEMPROP_BOX_ICON, // struct ui_icon*

    UIELEMPROP_LABEL_TEXT, // char*
    UIELEMPROP_LABEL_TEXT_FONT, // size_t ct, char**
    UIELEMPROP_LABEL_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_LABEL_TEXT_INTERPESC, // unsigned
    UIELEMPROP_LABEL_TEXT_INTERPESC_LINK, // unsigned
    UIELEMPROP_LABEL_ALIGN_X, // enum ui_align
    UIELEMPROP_LABEL_ALIGN_Y, // enum ui_align

    UIELEMPROP_TEXTBOX_TEXT, // char*
    UIELEMPROP_TEXTBOX_TEXT_FONT, // size_t ct, char**
    UIELEMPROP_TEXTBOX_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_TEXTBOX_TEXT_INTERPESC, // unsigned
    UIELEMPROP_TEXTBOX_SHADOWTEXT, // char*
    UIELEMPROP_TEXTBOX_SHADOWTEXT_FONT, // size_t ct, char**
    UIELEMPROP_TEXTBOX_SHADOWTEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_TEXTBOX_SHADOWTEXT_INTERPESC, // unsigned
    UIELEMPROP_TEXTBOX_PASSWORD, // unsigned
    UIELEMPROP_TEXTBOX_MULTILINE, // unsigned
    UIELEMPROP_TEXTBOX_EDITABLE, // unsigned
    UIELEMPROP_TEXTBOX_SELECTABLE, // unsigned

    UIELEMPROP_BUTTON_PRESSED, // unsigned
    UIELEMPROP_BUTTON_ICON, // struct ui_icon*
    UIELEMPROP_BUTTON_TEXT, // char*
    UIELEMPROP_BUTTON_TEXT_FONT, // size_t ct, char**
    UIELEMPROP_BUTTON_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_BUTTON_TEXT_INTERPESC, // unsigned
    UIELEMPROP_BUTTON_ALIGN_X, // enum ui_align
    UIELEMPROP_BUTTON_ALIGN_Y, // enum ui_align

    UIELEMPROP_CHECKBOX_CHECKED, // unsigned

    UIELEMPROP_RADIO_SELECTED, // unsigned

    UIELEMPROP_SLIDER_MAX, // size_t
    UIELEMPROP_SLIDER_VALUE, // size_t
    UIELEMPROP_SLIDER_NOTCHES, // unsigned
    UIELEMPROP_SLIDER_SNAPTONOTCH, // unsigned

    UIELEMPROP_PROGRESSBAR_MAX, // size_t
    UIELEMPROP_PROGRESSBAR_PROGRESS, // size_t
    UIELEMPROP_PROGRESSBAR_ICON, // struct ui_icon*
    UIELEMPROP_PROGRESSBAR_TEXT, // char*
    UIELEMPROP_PROGRESSBAR_TEXT_FONT, // size_t ct, char**
    UIELEMPROP_PROGRESSBAR_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_PROGRESSBAR_TEXT_INTERPESC, // unsigned
    UIELEMPROP_PROGRESSBAR_ALIGN_X, // enum ui_align
    UIELEMPROP_PROGRESSBAR_ALIGN_Y, // enum ui_align

    UIELEMPROP_SCROLLBAR_AMOUNT, // size_t
    UIELEMPROP_SCROLLBAR_SCROLL, // size_t

    UIELEMPROP_DROPDOWN_ITEMS, // size_t ct, struct uielemprop_dropdown_item*
    UIELEMPROP_DROPDOWN_INSITEMS, // size_t at, size_t ct, struct uielemprop_dropdown_item*
    UIELEMPROP_DROPDOWN_DELITEMS, // size_t at, size_t ct
    UIELEMPROP_DROPDOWN_ITEM_ICON, // size_t i, struct ui_icon*
    UIELEMPROP_DROPDOWN_ITEM_TEXT, // size_t i, char*
    UIELEMPROP_DROPDOWN_ITEM_TEXT_FONT, // size_t i, size_t ct, char**
    UIELEMPROP_DROPDOWN_ITEM_TEXT_FRMT, // size_t i, struct ui_textfrmt*
    UIELEMPROP_DROPDOWN_ITEM_TEXT_INTERPESC, // size_t i, unsigned

    UIELEMPROP_LIST_SIZE, // size_t w, size_t h
    UIELEMPROP_LIST_HEADERS, // size_t ct, struct uielemprop_list_header*
    UIELEMPROP_LIST_HEADER, // size_t i, struct uielemprop_list_header*
    UIELEMPROP_LIST_HEADER_ICON, // size_t i, struct ui_icon*
    UIELEMPROP_LIST_HEADER_TEXT, // size_t i, char*
    UIELEMPROP_LIST_HEADER_TEXT_FONT, // size_t i, size_t ct, char**
    UIELEMPROP_LIST_HEADER_TEXT_FRMT, // size_t i, struct ui_textfrmt*
    UIELEMPROP_LIST_HEADER_TEXT_INTERPESC, // size_t i, unsigned
    UIELEMPROP_LIST_HEADER_ALIGN_X, // size_t i, enum ui_align
    UIELEMPROP_LIST_HEADER_ALIGN_Y, // size_t i, enum ui_align
    UIELEMPROP_LIST_CELLS, // size_t x, size_t y, size_t w, size_t h, struct uielemprop_list_cell*
    UIELEMPROP_LIST_CELL, // size_t x, size_t y, struct uielemprop_list_cell*
    UIELEMPROP_LIST_CELL_ICON, // size_t x, size_t y, struct ui_icon*
    UIELEMPROP_LIST_CELL_TEXT, // size_t x, size_t y, char*
    UIELEMPROP_LIST_CELL_TEXT_FONT, // size_t x, size_t y, size_t ct, char**
    UIELEMPROP_LIST_CELL_TEXT_FRMT, // size_t x, size_t y, struct ui_textfrmt*
    UIELEMPROP_LIST_CELL_TEXT_INTERPESC, // size_t x, size_t y, unsigned
    UIELEMPROP_LIST_CELL_ALIGN_X, // size_t x, size_t y, enum ui_align
    UIELEMPROP_LIST_CELL_ALIGN_Y, // size_t x, size_t y, enum ui_align
    UIELEMPROP_LIST_SELMODE, // enum uielem_list_selmode

    UIELEMPROP_TABS_ITEMS, // size_t ct, struct uielemprop_tabs_item*
    UIELEMPROP_TABS_ITEM, // size_t i, struct uielemprop_tabs_item*
    UIELEMPROP_TABS_ITEM_ICON, // size_t i, struct ui_icon*
    UIELEMPROP_TABS_ITEM_TEXT, // size_t i, char*
    UIELEMPROP_TABS_ITEM_TEXT_FONT, // size_t i, size_t ct, char**
    UIELEMPROP_TABS_ITEM_TEXT_FRMT, // size_t i, struct ui_textfrmt*
    UIELEMPROP_TABS_ITEM_TEXT_INTERPESC, // size_t i, unsigned

    UIELEMPROP_MENU_ITEMS, // size_t ct, struct uielemprop_menu_item*
    UIELEMPROP_MENU_ITEM, // size_t i ... -1, struct uielemprop_menu_item*
    UIELEMPROP_MENU_ITEM_ICON, // size_t i ... -1, struct ui_icon*
    UIELEMPROP_MENU_ITEM_TEXT, // size_t i ... -1, char*
    UIELEMPROP_MENU_ITEM_TEXT_FONT, // size_t i ... -1, size_t ct, char**
    UIELEMPROP_MENU_ITEM_TEXT_FRMT, // size_t i ... -1, struct ui_textfrmt*
    UIELEMPROP_MENU_ITEM_TEXT_INTERPESC, // size_t i ... -1, unsigned

    UIELEMPROP_MEDIA_COLOR, // uint32_t
    UIELEMPROP_MEDIA_BACKGROUND, // char*
    UIELEMPROP_MEDIA_BORDER, // char*
    UIELEMPROP_MEDIA_IMAGE, // struct rc_image*
    UIELEMPROP_MEDIA_VIDEO, // struct ui_video*
    UIELEMPROP_MEDIA_SOUND, // struct ui_sound*
    UIELEMPROP_MEDIA_POSMODE, // enum uielem_media_posmode
    UIELEMPROP_MEDIA_FILTMODE, // enum uielem_media_filtmode
    UIELEMPROP_MEDIA_PAUSE, // unsigned
    UIELEMPROP_MEDIA_SEEK, // double seconds
    UIELEMPROP_MEDIA_SPEED, // double
    UIELEMPROP_MEDIA_VOL, // double

    UIELEMPROP__COUNT
};

enum uicontainerprop {
    UICONTAINERPROP_CALLBACK, // uicontainercallback
    UICONTAINERPROP_HIDDEN, // unsigned
    UICONTAINERPROP_X, // double
    UICONTAINERPROP_Y, // double
    UICONTAINERPROP_Z, // double
    UICONTAINERPROP_W, // double
    UICONTAINERPROP_H, // double
    UICONTAINERPROP_SCROLL_X, // enum uicontainer_scroll
    UICONTAINERPROP_SCROLL_Y, // enum uicontainer_scroll
    UICONTAINERPROP_BACKGROUND, // char*
    UICONTAINERPROP_BORDER, // char*
    UICONTAINERPROP__COUNT
};

struct ui_textfrmt {
    uint32_t fgc;
    uint32_t bgc;
    uint32_t shc;
    uint8_t pt;
    uint8_t b : 1;
    uint8_t i : 1;
    uint8_t u : 1;
    uint8_t s : 1;
    uint8_t hasfgc : 1;
    uint8_t hasbgc : 1;
    uint8_t hasshc : 1;
    uint8_t : 1;
};

PACKEDENUM ui_imageposmode {
    UI_IMAGEPOSMODE_STRETCH,
    UI_IMAGEPOSMODE_TILE
};
PACKEDENUM ui_imagefiltmode {
    UI_IMAGEFILTMODE_NEAREST,
    UI_IMAGEFILTMODE_LINEAR
};
struct ui_image {
    struct rc_image* image;
    enum ui_imageposmode posmode;
    enum ui_imagefiltmode filtmode;
};

struct ui_icon {
    struct rc_image* image;
    float scale;
    enum ui_imagefiltmode filtmode;
};

enum ui_align {
    UI_ALIGN_LEFT = -1,
    UI_ALIGN_TOP = -1,
    UI_ALIGN_CENTER = 0,
    UI_ALIGN_RIGHT = 1,
    UI_ALIGN_BOTTOM = 1
};

enum uielem_list_selmode {
    UIELEM_LIST_SELMODE_CELL,
    UIELEM_LIST_SELMODE_ROW,
    UIELEM_LIST_SELMODE_COL
};

enum uielem_media_posmode {
    UIELEM_MEDIA_POSMODE_STRETCH,
    UIELEM_MEDIA_POSMODE_FILL,
    UIELEM_MEDIA_POSMODE_FIT,
    UIELEM_MEDIA_POSMODE_CENTER,
    UIELEM_MEDIA_POSMODE_TILE
};
enum uielem_media_filtmode {
    UIELEM_MEDIA_FILTMODE_NEAREST,
    UIELEM_MEDIA_FILTMODE_LINEAR
};

struct uielem {
    uint8_t valid : 1;
    uint8_t hidden : 1;
    uint8_t delete : 1;
    uint8_t : 5;
    size_t container;
    enum uielemtype type;
    size_t dataindex;
};

enum uicontainer_scroll {
    UICONTAINER_SCROLL_HIDDEN,
    UICONTAINER_SCROLL_ONDEMAND,
    UICONTAINER_SCROLL_SHOWN
};

struct uicontainer {
    uint8_t valid : 1;
    uint8_t hidden : 1;
    uint8_t delete : 1;
    uint8_t : 5;
    volatile unsigned refct;
    size_t parent;
    struct VLB(size_t) children;
    struct VLB(struct uielem) elems;
};

struct uistate {
    volatile unsigned refct;
    float w;
    float h;
    float scale;
    struct VLB(struct uicontainer) containers;
    struct VLB(size_t) containersorder;
};

extern struct uistate uistate;

size_t newUIElemContainer(size_t idtreedepth, const char** idtree, ... /* props */);
size_t getUIElemContainer(size_t idtreedepth, const char** idtree);
void rlsUIElemContainer(size_t);
void editUIElemContainer(size_t, ... /* props */);
void delUIElemContainer(size_t);

size_t newUIElem(size_t container, const char* id, enum uielemtype, ... /* props */);
size_t getUIElem(size_t container, const char* id);
void rlsUIElem(size_t container, size_t);
void editUIElem(size_t, ... /* props */);
void delUIElem(size_t);

#endif
