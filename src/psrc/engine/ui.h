#ifndef PSRC_ENGINE_UI_H
#define PSRC_ENGINE_UI_H

#include "../attribs.h"

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
    UIELEMPROP_COMMON_PADDING_TOP, // double
    UIELEMPROP_COMMON_PADDING_BOTTOM, // double
    UIELEMPROP_COMMON_PADDING_LEFT, // double
    UIELEMPROP_COMMON_PADDING_RIGHT, // double

    UIELEMPROP_BOX_COLOR, // uint32_t
    UIELEMPROP_BOX_IMAGE, // struct rc_image*

    UIELEMPROP_LABEL_TEXT, // char*
    UIELEMPROP_LABEL_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_LABEL_TEXT_INTERPESC, // unsigned

    UIELEMPROP_TEXTBOX_TEXT, // char*
    UIELEMPROP_TEXTBOX_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_TEXTBOX_TEXT_INTERPESC, // unsigned
    UIELEMPROP_TEXTBOX_SHADOWTEXT, // char*
    UIELEMPROP_TEXTBOX_SHADOWTEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_TEXTBOX_SHADOWTEXT_INTERPESC, // unsigned
    UIELEMPROP_TEXTBOX_PASSWORD, // unsigned
    UIELEMPROP_TEXTBOX_MULTILINE, // unsigned

    UIELEMPROP_BUTTON_PRESSED, // unsigned
    UIELEMPROP_BUTTON_IMAGE, // struct rc_image*
    UIELEMPROP_BUTTON_TEXT, // char*
    UIELEMPROP_BUTTON_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_BUTTON_TEXT_INTERPESC, // unsigned

    UIELEMPROP_CHECKBOX_CHECKED, // unsigned

    UIELEMPROP_RADIO_SELECTED, // unsigned

    UIELEMPROP_PROGRESSBAR_AMOUNT, // size_t
    UIELEMPROP_PROGRESSBAR_PROGRESS, // size_t
    UIELEMPROP_PROGRESSBAR_TEXT, // char*
    UIELEMPROP_PROGRESSBAR_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_PROGRESSBAR_TEXT_INTERPESC, // unsigned

    UIELEMPROP_SCROLLBAR_AMOUNT, // size_t
    UIELEMPROP_SCROLLBAR_SCROLL, // size_t

    UIELEMPROP_DROPDOWN_ITEMS, // size_t ct, struct uielemprop_dropdown_item*
    UIELEMPROP_DROPDOWN_INSITEMS, // size_t at, size_t ct, struct uielemprop_dropdown_item*
    UIELEMPROP_DROPDOWN_DELITEMS, // size_t at, size_t ct
    UIELEMPROP_DROPDOWN_ITEM_TEXT, // char*
    UIELEMPROP_DROPDOWN_ITEM_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_DROPDOWN_ITEM_TEXT_INTERPESC, // unsigned
    UIELEMPROP_DROPDOWN_ITEM_IMAGE, // struct rc_image*

    UIELEMPROP_LIST_SIZE, // size_t w, size_t h
    UIELEMPROP_LIST_CELLS, // size_t x, size_t y, size_t w, size_t h, struct uielemprop_list_cell*
    UIELEMPROP_LIST_CELL, // size_t x, size_t y, struct uielemprop_list_cell*
    UIELEMPROP_LIST_CELL_TEXT, // char*
    UIELEMPROP_LIST_CELL_TEXT_FRMT, // struct ui_textfrmt*
    UIELEMPROP_LIST_CELL_TEXT_INTERPESC, // unsigned
    UIELEMPROP_LIST_CELL_IMAGE, // struct rc_image*

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

enum uicontainer_scroll {
    UICONTAINER_SCROLL_HIDDEN,
    UICONTAINER_SCROLL_ONDEMAND,
    UICONTAINER_SCROLL_SHOWN
};

struct ui_textfrmt {
    uint32_t fgc;
    uint32_t bgc;
    uint8_t pt;
    uint8_t b : 1;
    uint8_t i : 1;
    uint8_t u : 1;
    uint8_t s : 1;
    uint8_t hasfgc : 1;
    uint8_t hasbgc : 1;
    uint8_t : 2;
};

struct uistate {
    int placeholder;
};

bool newUIState(struct uistate*);
void delUIState(struct uistate*);

size_t newUIElemContainer(struct uistate*, const char* id, ... /* props */);
size_t grabUIElemContainer(struct uistate*, const char* id);
void rlsUIElemContainer(struct uistate*, size_t);
void editUIElemContainer(struct uistate*, size_t, ... /* props */);
void delUIElemContainer(struct uistate*, size_t);

size_t newUIElem(struct uistate*, size_t container, const char* id, enum uielemtype, ... /* props */);
size_t grabUIElem(struct uistate*, size_t container, const char* id);
void rlsUIElem(struct uistate*, size_t container, size_t);
void editUIElem(struct uistate*, size_t, ... /* props */);
void delUIElem(struct uistate*, size_t);

#endif
