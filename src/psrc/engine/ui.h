#ifndef PSRC_ENGINE_UI_H
#define PSRC_ENGINE_UI_H

// TODO: size_t -> int

#include "../resource.h"
#include "../threading.h"
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
    UIELEMTYPE_MENUBAR,
    UIELEMTYPE_MENU,
    UIELEMTYPE_MEDIA,
    UIELEMTYPE_MODELVIEWER,
    UIELEMTYPE_SEPARATOR,
    UIELEMTYPE__COUNT
};

enum uielemprop {
    UIELEMPROP_COMMON_CALLBACK, // uielemcb, void*
    UIELEMPROP_COMMON_HIDDEN, // unsigned
    UIELEMPROP_COMMON_DISABLED, // unsigned
    UIELEMPROP_COMMON_X, // double
    UIELEMPROP_COMMON_Y, // double
    UIELEMPROP_COMMON_W, // double
    UIELEMPROP_COMMON_H, // double
    UIELEMPROP_COMMON_TOOLTIP, // char*
    UIELEMPROP_COMMON_TOOLTIP_FONT, // size_t ct, char**, unsigned pt
    UIELEMPROP_COMMON_TOOLTIP_FRMT, // struct ui_textfrmt*
    UIELEMPROP_COMMON_TOOLTIP_INTERPESC, // unsigned
    UIELEMPROP_COMMON_TOOLTIP_INTERPESC_FONT, // unsigned

    UIELEMPROP_BOX_COLOR, // uint32_t
    UIELEMPROP_BOX_BACKGROUND, // char*
    UIELEMPROP_BOX_BORDER, // char*
    UIELEMPROP_BOX_IMAGE, // struct ui_image*
    UIELEMPROP_BOX_ICON, // struct ui_icon*

    UIELEMPROP_LABEL_TEXT, // char*
    UIELEMPROP_LABEL_TEXT_FONT, // size_t ct, char**, unsigned pt
    UIELEMPROP_LABEL_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_LABEL_TEXT_INTERPESC, // unsigned
    UIELEMPROP_LABEL_TEXT_INTERPESC_FONT, // unsigned
    UIELEMPROP_LABEL_TEXT_INTERPESC_LINK, // unsigned
    UIELEMPROP_LABEL_ALIGN_X, // enum ui_align
    UIELEMPROP_LABEL_ALIGN_Y, // enum ui_align

    UIELEMPROP_TEXTBOX_TEXT, // char*
    UIELEMPROP_TEXTBOX_PLACEHOLDER, // char*
    UIELEMPROP_TEXTBOX_TEXT_FONT, // size_t ct, char**, unsigned pt
    UIELEMPROP_TEXTBOX_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_TEXTBOX_TEXT_INTERPESC, // unsigned
    UIELEMPROP_TEXTBOX_PASSWORD, // unsigned
    UIELEMPROP_TEXTBOX_MULTILINE, // unsigned
    UIELEMPROP_TEXTBOX_EDITABLE, // unsigned
    UIELEMPROP_TEXTBOX_SELECTABLE, // unsigned

    UIELEMPROP_BUTTON_PRESSED, // unsigned
    UIELEMPROP_BUTTON_ICON, // struct ui_icon*
    UIELEMPROP_BUTTON_TEXT, // char*
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
    UIELEMPROP_PROGRESSBAR_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_PROGRESSBAR_TEXT_INTERPESC, // unsigned
    UIELEMPROP_PROGRESSBAR_ALIGN_X, // enum ui_align
    UIELEMPROP_PROGRESSBAR_ALIGN_Y, // enum ui_align

    UIELEMPROP_SCROLLBAR_AMOUNT, // size_t
    UIELEMPROP_SCROLLBAR_SCROLL, // size_t

    UIELEMPROP_DROPDOWN_ITEMS, // size_t ct, struct uielem_dropdown_item*
    UIELEMPROP_DROPDOWN_INSITEMS, // size_t at, size_t ct, struct uielem_dropdown_item*
    UIELEMPROP_DROPDOWN_DELITEMS, // size_t at, size_t ct
    UIELEMPROP_DROPDOWN_ITEM_ICON, // size_t i, struct ui_icon*
    UIELEMPROP_DROPDOWN_ITEM_TEXT, // size_t i, char*
    UIELEMPROP_DROPDOWN_ITEM_TEXT_FRMT, // size_t i, struct ui_textfrmt*
    UIELEMPROP_DROPDOWN_ITEM_TEXT_INTERPESC, // size_t i, unsigned

    UIELEMPROP_LIST_SIZE, // size_t w, size_t h
    UIELEMPROP_LIST_HEADERS, // size_t ct, struct uielem_list_header*
    UIELEMPROP_LIST_HEADER, // size_t i, struct uielem_list_header*
    UIELEMPROP_LIST_HEADER_ICON, // size_t i, struct ui_icon*
    UIELEMPROP_LIST_HEADER_TEXT, // size_t i, char*
    UIELEMPROP_LIST_HEADER_TEXT_FRMT, // size_t i, struct ui_textfrmt*
    UIELEMPROP_LIST_HEADER_TEXT_INTERPESC, // size_t i, unsigned
    UIELEMPROP_LIST_HEADER_ALIGN_X, // size_t i, enum ui_align
    UIELEMPROP_LIST_HEADER_ALIGN_Y, // size_t i, enum ui_align
    UIELEMPROP_LIST_CELLS, // size_t x, size_t y, size_t w, size_t h, struct uielem_list_cell*
    UIELEMPROP_LIST_CELL, // size_t x, size_t y, struct uielem_list_cell*
    UIELEMPROP_LIST_CELL_ICON, // size_t x, size_t y, struct ui_icon*
    UIELEMPROP_LIST_CELL_TEXT, // size_t x, size_t y, char*
    UIELEMPROP_LIST_CELL_TEXT_FONT, // size_t x, size_t y, size_t ct, char**
    UIELEMPROP_LIST_CELL_TEXT_FRMT, // size_t x, size_t y, struct ui_textfrmt*
    UIELEMPROP_LIST_CELL_TEXT_INTERPESC, // size_t x, size_t y, unsigned
    UIELEMPROP_LIST_CELL_TEXT_INTERPESC_FONT, // size_t x, size_t y, unsigned
    UIELEMPROP_LIST_CELL_TEXT_INTERPESC_LINK, // size_t x, size_t y, unsigned
    UIELEMPROP_LIST_CELL_ALIGN_X, // size_t x, size_t y, enum ui_align
    UIELEMPROP_LIST_CELL_ALIGN_Y, // size_t x, size_t y, enum ui_align
    UIELEMPROP_LIST_SELMODE, // enum uielem_list_selmode

    UIELEMPROP_TABS_ITEMS, // size_t ct, struct uielem_tabs_item*
    UIELEMPROP_TABS_ITEM, // size_t i, struct uielem_tabs_item*
    UIELEMPROP_TABS_ITEM_ICON, // size_t i, struct ui_icon*
    UIELEMPROP_TABS_ITEM_TEXT, // size_t i, char*
    UIELEMPROP_TABS_ITEM_TEXT_FRMT, // size_t i, struct ui_textfrmt*
    UIELEMPROP_TABS_ITEM_TEXT_INTERPESC, // size_t i, unsigned

    UIELEMPROP_MENUBAR_ITEMS, // size_t ct, struct uielem_menubar_item*
    UIELEMPROP_MENUBAR_ITEM, // size_t i, struct uielem_menubar_item*

    UIELEMPROP_MENU_ITEMS, // size_t ct, struct uielem_menu_item*
    UIELEMPROP_MENU_ITEM, // size_t i ... -1, struct uielem_menu_item*
    UIELEMPROP_MENU_ITEM_ICON, // size_t i ... -1, struct ui_icon*
    UIELEMPROP_MENU_ITEM_TEXT, // size_t i ... -1, char*
    UIELEMPROP_MENU_ITEM_TEXT_FRMT, // size_t i ... -1, struct ui_textfrmt*
    UIELEMPROP_MENU_ITEM_TEXT_INTERPESC, // size_t i ... -1, unsigned

    UIELEMPROP_MEDIA_COLOR, // uint32_t
    UIELEMPROP_MEDIA_BACKGROUND, // char*
    UIELEMPROP_MEDIA_BORDER, // char*
    UIELEMPROP_MEDIA_IMAGE, // struct rc_image*
    UIELEMPROP_MEDIA_VIDEO, // struct ui_video*
    UIELEMPROP_MEDIA_SOUND, // struct ui_sound*
    UIELEMPROP_MEDIA_RENDMODE, // enum uielem_media_rendmode
    UIELEMPROP_MEDIA_FILTMODE, // enum uielem_media_filtmode
    UIELEMPROP_MEDIA_PAUSE, // unsigned
    UIELEMPROP_MEDIA_SEEK, // double seconds
    UIELEMPROP_MEDIA_SPEED, // double
    UIELEMPROP_MEDIA_VOL, // double

    UIELEMPROP_MODELVIEWER_MODELS, // size_t ct, struct uielem_modelviewer_model*
    UIELEMPROP_MODELVIEWER_MODEL_RC, // size_t i, struct rc_model*
    UIELEMPROP_MODELVIEWER_MODEL_POS, // size_t i, float[3]
    UIELEMPROP_MODELVIEWER_MODEL_ROT, // size_t i, float[3]
    UIELEMPROP_MODELVIEWER_MODEL_SCALE, // size_t i, float[3]
    UIELEMPROP_MODELVIEWER_MODEL_ANIM, // size_t i, char*
    UIELEMPROP_MODELVIEWER_MODEL_LOOPANIM, // size_t i, unsigned
    UIELEMPROP_MODELVIEWER_LIGHTING, // struct uielem_modelviewer_lighting*
    UIELEMPROP_MODELVIEWER_OFFSET, // float[3]
    UIELEMPROP_MODELVIEWER_ROTATION, // float[3]
    UIELEMPROP_MODELVIEWER_ROTMODE, // enum uielem_modelviewer_rotmode

    UIELEMPROP_SEPARATOR_STYLE, // enum uielem_separator_style

    UIELEMPROP__COUNT
};

enum uicontainerprop {
    UICONTAINERPROP_CALLBACK, // uicontainercb, void*
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
    uint8_t chpt : 1;
};

PACKEDENUM ui_imagerendmode {
    UI_IMAGERENDMODE_STRETCH,
    UI_IMAGERENDMODE_TILE
};
PACKEDENUM ui_imagefiltmode {
    UI_IMAGEFILTMODE_NEAREST,
    UI_IMAGEFILTMODE_LINEAR
};
struct ui_image {
    struct rc_image* image;
    enum ui_imagerendmode rendmode;
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

enum uielem_media_rendmode {
    UIELEM_MEDIA_RENDMODE_STRETCH,
    UIELEM_MEDIA_RENDMODE_FILL,
    UIELEM_MEDIA_RENDMODE_FIT,
    UIELEM_MEDIA_RENDMODE_CENTER,
    UIELEM_MEDIA_RENDMODE_TILE
};
enum uielem_media_filtmode {
    UIELEM_MEDIA_FILTMODE_NEAREST,
    UIELEM_MEDIA_FILTMODE_LINEAR
};

enum uielem_separator_style {
    UIELEM_SEPARATOR_STYLE_GROOVE,
    UIELEM_SEPARATOR_STYLE_RIDGE
};

enum uielem_modelviewer_rotmode {
    UIELEM_MODELVIEWER_ROTMODE_NONE,
    UIELEM_MODELVIEWER_ROTMODE_X,
    UIELEM_MODELVIEWER_ROTMODE_XY
};
struct uielem_modelviewer_model {
    struct rc_model* model;
    float pos[3];
    float rot[3];
    float scale[3];
    char* animation;
    bool loopanim;
};
struct uielem_modelviewer_lighting {
    float direction[3];
    uint32_t color;
    uint32_t ambientcolor;
};

union uielemdata {
    int placeholder;
};

struct uielem;

enum uielemevent {
    UIELEMEVENT_COMMON_UPDATE,
    UIELEMEVENT_COMMON_CONTAINERUPDATE,
    UIELEMEVENT_COMMON_MESH,
    UIELEMEVENT_COMMON_HOVER,
    UIELEMEVENT_COMMON_UNHOVER,
    UIELEMEVENT_COMMON_PRESS,
    UIELEMEVENT_COMMON_RELEASE,
    UIELEMEVENT_COMMON_ACTIVATE
};
union uielemevent_data {
    union {
        struct {
            int placeholder;
        } mesh;
    } common;
};
enum uielemcb_ret {
    UIELEMCB_RET_IGNORED,
    UIELEMCB_RET_HANDLED,
    UIELEMCB_RET_OVERRIDE
};
typedef enum uielemcb_ret (*uielemcb)(void*, enum uielemtype, char* id, size_t i, enum uielemevent, union uielemevent_data*);

struct uielemgroup_statusbits {
    uint32_t valid;
    uint32_t delete;
};
struct uielem {
    uint8_t hidden : 1;
    uint8_t : 7;
    size_t container;
    enum uielemtype type;
    size_t dataindex;
};

enum uicontainer_scroll {
    UICONTAINER_SCROLL_HIDDEN,
    UICONTAINER_SCROLL_ONDEMAND,
    UICONTAINER_SCROLL_SHOWN
};

struct uicontainer;

enum uicontainerevent {
    UICONTAINEREVENT_UPDATE,
    UICONTAINEREVENT_MESH
};
union uicontainerevent_data {
    struct {
        int placeholder;
    } mesh;
};
enum uicontainercb_ret {
    UICONTAINERCB_RET_IGNORED,
    UICONTAINERCB_RET_HANDLED,
    UICONTAINERCB_RET_OVERRIDE
};
typedef enum uicontainercb_ret (*uicontainercb)(void*, char** idtree, size_t i, enum uicontainerevent, union uicontainerevent_data*);

struct uicontainergroup_statusbits {
    uint32_t valid;
    uint32_t delete;
};
struct uicontainer {
    uint8_t valid : 1;
    uint8_t : 7;
    volatile unsigned refct;
    size_t parent;
    struct VLB(size_t) children;
    float w;
    float h;
    float scale;
    struct VLB(struct uielemgroup_statusbits) elemstatus;
    struct VLB(struct uielem) elems;
};

struct uistate {
    #if PSRC_MTLVL >= 2
    struct accesslock lock;
    #endif
    volatile unsigned refct;
    float w;
    float h;
    float scale;
    struct VLB(struct uicontainergroup_statusbits) containerstatus;
    struct VLB(struct uicontainer) containers;
    struct VLB(size_t) containerorder;
};

extern struct uistate uistate;

bool initUI(void);
void quitUI(void);

size_t newUIElemContainer(size_t idtreedepth, const char** idtree, ... /* props */);
size_t getUIElemContainer(size_t idtreedepth, const char** idtree);
void editUIElemContainer(size_t, ... /* props */);
void delUIElemContainer(size_t);
void rlsUIElemContainer(size_t);

size_t newUIElem(size_t container, const char* id, enum uielemtype, ... /* props */);
size_t getUIElem(size_t container, const char* id);
void editUIElem(size_t container, size_t, ... /* props */);
void delUIElem(size_t container, size_t);
void rlsUIElem(size_t container, size_t);

#endif
