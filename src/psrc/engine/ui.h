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
    UIELEMPROP_COMMON_TOOLTIP_FONTS, // size_t ct, char**
    UIELEMPROP_COMMON_TOOLTIP_FRMT, // struct ui_textfrmt*
    UIELEMPROP_COMMON_TOOLTIP_INTERPESC, // unsigned

    UIELEMPROP_BOX_COLOR, // uint32_t
    UIELEMPROP_BOX_IMAGE, // struct ui_image*

    UIELEMPROP_LABEL_TEXT, // char*
    UIELEMPROP_LABEL_TEXT_FONTS, // size_t ct, char**
    UIELEMPROP_LABEL_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_LABEL_TEXT_INTERPESC, // unsigned
    UIELEMPROP_LABEL_ALIGN_X, // enum ui_align
    UIELEMPROP_LABEL_ALIGN_Y, // enum ui_align

    UIELEMPROP_TEXTBOX_TEXT, // char*
    UIELEMPROP_TEXTBOX_TEXT_FONTS, // size_t ct, char**
    UIELEMPROP_TEXTBOX_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_TEXTBOX_TEXT_INTERPESC, // unsigned
    UIELEMPROP_TEXTBOX_SHADOWTEXT, // char*
    UIELEMPROP_TEXTBOX_SHADOWTEXT_FONTS, // size_t ct, char**
    UIELEMPROP_TEXTBOX_SHADOWTEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_TEXTBOX_SHADOWTEXT_INTERPESC, // unsigned
    UIELEMPROP_TEXTBOX_PASSWORD, // unsigned
    UIELEMPROP_TEXTBOX_MULTILINE, // unsigned
    UIELEMPROP_TEXTBOX_EDITABLE, // unsigned
    UIELEMPROP_TEXTBOX_SELECTABLE, // unsigned

    UIELEMPROP_BUTTON_PRESSED, // unsigned
    UIELEMPROP_BUTTON_ICON, // struct ui_icon*
    UIELEMPROP_BUTTON_TEXT, // char*
    UIELEMPROP_BUTTON_TEXT_FONTS, // size_t ct, char**
    UIELEMPROP_BUTTON_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_BUTTON_TEXT_INTERPESC, // unsigned
    UIELEMPROP_BUTTON_ALIGN_X, // enum ui_align
    UIELEMPROP_BUTTON_ALIGN_Y, // enum ui_align

    UIELEMPROP_CHECKBOX_CHECKED, // unsigned

    UIELEMPROP_RADIO_SELECTED, // unsigned

    UIELEMPROP_PROGRESSBAR_AMOUNT, // size_t
    UIELEMPROP_PROGRESSBAR_PROGRESS, // size_t
    UIELEMPROP_PROGRESSBAR_ICON, // struct ui_icon*
    UIELEMPROP_PROGRESSBAR_TEXT, // char*
    UIELEMPROP_PROGRESSBAR_TEXT_FONTS, // size_t ct, char**
    UIELEMPROP_PROGRESSBAR_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_PROGRESSBAR_TEXT_INTERPESC, // unsigned
    UIELEMPROP_PROGRESSBAR_ALIGN_X, // enum ui_align
    UIELEMPROP_PROGRESSBAR_ALIGN_Y, // enum ui_align

    UIELEMPROP_SCROLLBAR_AMOUNT, // size_t
    UIELEMPROP_SCROLLBAR_SCROLL, // size_t

    UIELEMPROP_DROPDOWN_ITEMS, // size_t ct, struct uielemprop_dropdown_item*
    UIELEMPROP_DROPDOWN_INSITEMS, // size_t at, size_t ct, struct uielemprop_dropdown_item*
    UIELEMPROP_DROPDOWN_DELITEMS, // size_t at, size_t ct
    UIELEMPROP_DROPDOWN_ITEM_ICON, // struct ui_icon*
    UIELEMPROP_DROPDOWN_ITEM_TEXT, // char*
    UIELEMPROP_DROPDOWN_ITEM_TEXT_FONTS, // size_t ct, char**
    UIELEMPROP_DROPDOWN_ITEM_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_DROPDOWN_ITEM_TEXT_INTERPESC, // unsigned

    UIELEMPROP_LIST_SIZE, // size_t w, size_t h
    UIELEMPROP_LIST_HEADERS, // size_t ct, struct uielemprop_list_header*
    UIELEMPROP_LIST_HEADER, // size_t i, struct uielemprop_list_header*
    UIELEMPROP_LIST_HEADER_ICON, // struct ui_icon*
    UIELEMPROP_LIST_HEADER_TEXT, // char*
    UIELEMPROP_LIST_HEADER_TEXT_FONTS, // size_t ct, char**
    UIELEMPROP_LIST_HEADER_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_LIST_HEADER_TEXT_INTERPESC, // unsigned
    UIELEMPROP_LIST_HEADER_ALIGN_X, // enum ui_align
    UIELEMPROP_LIST_HEADER_ALIGN_Y, // enum ui_align
    UIELEMPROP_LIST_CELLS, // size_t x, size_t y, size_t w, size_t h, struct uielemprop_list_cell*
    UIELEMPROP_LIST_CELL, // size_t x, size_t y, struct uielemprop_list_cell*
    UIELEMPROP_LIST_CELL_ICON, // struct ui_icon*
    UIELEMPROP_LIST_CELL_TEXT, // char*
    UIELEMPROP_LIST_CELL_TEXT_FONTS, // size_t ct, char**
    UIELEMPROP_LIST_CELL_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_LIST_CELL_TEXT_INTERPESC, // unsigned
    UIELEMPROP_LIST_CELL_ALIGN_X, // enum ui_align
    UIELEMPROP_LIST_CELL_ALIGN_Y, // enum ui_align
    UIELEMPROP_LIST_SELMODE, // enum uielem_list_selmode

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

PACKEDENUM ui_imagemode {
    UI_IMAGEMODE_STRETCH,
    UI_IMAGEMODE_REPEAT
};
PACKEDENUM ui_imagefilt {
    UI_IMAGEMODE_NEAREST,
    UI_IMAGEMODE_LINEAR
};
struct ui_image {
    struct rc_image* image;
    enum ui_imagemode mode;
    enum ui_imagefilt filt;
};

struct ui_icon {
    struct rc_image* image;
    float scale;
    enum ui_imagefilt filt;
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

struct uitheme {
    struct {
        float size;
    } icon;
    struct {
        struct {
            uint32_t color;
            struct ui_image image;
        } bg;
        struct {
            struct {
                uint32_t color;
                struct ui_image image;
                float size;
            } top, bottom, left, right;
            struct {
                uint32_t color;
                struct ui_image image;
            } topleft, bottomleft, topright, bottomright;
        } border;
        struct {
            float top, bottom, left, right;
        } padding;
        struct {
            size_t fontct;
            char** fonts;
            struct ui_textfrmt frmt;
        } text;
    } tooltip;
    struct {
        struct {
            size_t fontct;
            char** fonts;
            struct {
                struct ui_textfrmt frmt;
            } normal, disabled;
        } text;
    } label;
    struct {
        struct {
            struct {
                uint32_t color;
                struct ui_image image;
            } inactive, hover, active, disabled;
        } bg;
        struct {
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } inactive, hover, active, disabled;
                float size;
            } top, bottom, left, right;
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } inactive, hover, active, disabled;
            } topleft, bottomleft, topright, bottomright;
        } border;
        struct {
            float top, bottom, left, right;
        } padding;
        struct {
            size_t fontct;
            char** fonts;
            struct {
                struct {
                    struct ui_textfrmt frmt;
                } inactive, hover, active, disabled;
            } text, shadowtext, seltext;
        };
    } textbox;
    struct {
        struct {
            struct {
                uint32_t color;
                struct ui_image image;
            } inactive, hover, active, disabled;
        } bg;
        struct {
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } inactive, hover, active, disabled;
                float size;
            } top, bottom, left, right;
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } inactive, hover, active, disabled;
            } topleft, bottomleft, topright, bottomright;
        } border;
        struct {
            float top, bottom, left, right;
        } padding;
        struct {
            size_t fontct;
            char** fonts;
            struct {
                struct ui_textfrmt frmt;
            } inactive, hover, active, disabled;
        } text;
    } button;
    struct {
        struct {
            struct {
                uint32_t color;
                struct ui_image image;
            } inactive, hover, active, disabled;
        } bg;
        struct {
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } inactive, hover, active, disabled;
                float size;
            } top, bottom, left, right;
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } inactive, hover, active, disabled;
            } topleft, bottomleft, topright, bottomright;
        } border;
        struct {
            float top, bottom, left, right;
        } padding;
    } checkbox, radio;
    struct {
        struct {
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } inactive, hover, active, disabled;
            } bg;
            struct {
                struct {
                    struct {
                        uint32_t color;
                        struct ui_image image;
                    } inactive, hover, active, disabled;
                    float size;
                } top, bottom, left, right;
                struct {
                    struct {
                        uint32_t color;
                        struct ui_image image;
                    } inactive, hover, active, disabled;
                } topleft, bottomleft, topright, bottomright;
            } border;
            float w, h;
        } track, knob;
    } slider;
    struct {
        struct {
            struct {
                uint32_t color;
                struct ui_image image;
            } empty, filled, disabled;
        } bg;
        struct {
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } normal, disabled;
                float size;
            } top, bottom, left, right;
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } normal, disabled;
            } topleft, bottomleft, topright, bottomright;
        } border;
        struct {
            float top, bottom, left, right;
        } padding;
        struct {
            size_t fontct;
            char** fonts;
            struct {
                struct ui_textfrmt frmt;
            } normal, disabled;
        } text;
    } progressbar;
    struct {
        struct {
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } inactive, hover, active, disabled;
            } bg;
            struct {
                struct {
                    struct {
                        uint32_t color;
                        struct ui_image image;
                    } inactive, hover, active, disabled;
                    float size;
                } top, bottom, left, right;
                struct {
                    struct {
                        uint32_t color;
                        struct ui_image image;
                    } inactive, hover, active, disabled;
                } topleft, bottomleft, topright, bottomright;
            } border;
        } up, down, bar;
    } scrollbar;
    struct {
        struct {
            struct {
                uint32_t color;
                struct ui_image image;
            } inactive, hover, active, disabled;
        } bg;
        struct {
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } inactive, hover, active, disabled;
                float size;
            } top, bottom, left, right;
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } inactive, hover, active, disabled;
            } topleft, bottomleft, topright, bottomright;
        } border;
        struct {
            float top, bottom, left, right;
        } padding;
        struct {
            struct ui_icon icon;
        } expand, collapse;
        struct {
            size_t fontct;
            char** fonts;
            struct {
                struct ui_textfrmt frmt;
            } inactive, hover, active, disabled;
        } text;
    } dropdown;
    struct {
        struct {
            struct {
                struct {
                    uint32_t color;
                    struct ui_image image;
                } inactive, hover, active, disabled;
            } bg;
            struct {
                struct {
                    struct {
                        uint32_t color;
                        struct ui_image image;
                    } inactive, hover, active, disabled;
                    float size;
                } top, bottom, left, right;
                struct {
                    struct {
                        uint32_t color;
                        struct ui_image image;
                    } inactive, hover, active, disabled;
                } topleft, bottomleft, topright, bottomright;
            } border;
            struct {
                float top, bottom, left, right;
            } padding;
            struct {
                struct ui_icon icon;
            } sortup, sortdown;
            struct {
                size_t fontct;
                char** fonts;
                struct {
                    struct ui_textfrmt frmt;
                } inactive, hover, active, disabled;
            } text;
        } header;
        struct {
            struct {
                struct {
                    struct {
                        uint32_t color;
                        struct ui_image image;
                    } inactive, hover, active, disabled;
                } odd, even;
            } bg;
            struct {
                struct {
                    struct {
                        struct {
                            uint32_t color;
                            struct ui_image image;
                        } inactive, hover, active, disabled;
                        float size;
                    } top, bottom, left, right;
                    struct {
                        struct {
                            uint32_t color;
                            struct ui_image image;
                        } inactive, hover, active, disabled;
                    } topleft, bottomleft, topright, bottomright;
                } border;
            } topleft, top, topright, left, center, right, bottomleft, bottom, bottomright;
            struct {
                float top, bottom, left, right;
            } padding;
            struct {
                size_t fontct;
                char** fonts;
                struct {
                    struct ui_textfrmt frmt;
                } inactive, hover, active, disabled;
            } text;
        } cell;
    } list;
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
