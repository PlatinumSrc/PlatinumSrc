#ifndef PSRC_ENGINE_UI_H
#define PSRC_ENGINE_UI_H

#include <stdint.h>

enum uielem {
    UIELEM_WINDOW, // Cuts contents to size (probably will use glScissor)
    UIELEM_CONTAINER,
    UIELEM_TEXT,
    UIELEM_LINK,
    UIELEM_BUTTON,
    UIELEM_TEXTBOX,
    UIELEM_CHECKBOX,
    UIELEM_SLIDER,
    UIELEM_PROGRESSBAR,
    UIELEM_SCROLLBAR,
    UIELEM_LIST,
    UIELEM_DROPDOWN,
    UIELEM_SELECTBOX,
    UIELEM_TABS,
    // TODO: Add UIELEM_HUD_*
};

enum uiattr {
    UIATTR_END,
    // general
    UIATTR_HIDDEN, // unsigned
    UIATTR_DISABLED, // unsigned
    UIATTR_CALLBACK, // void (*)(enum uievent, struct uievent_data*), void*
    UIATTR_POSITION, // char*
    UIATTR_SIZE, // char*
    UIATTR_MAXSIZE, // char*
    UIATTR_MARGIN, // char*
    UIATTR_PADDING, // char*
    // element-specific
    UIATTR_WINDOW_TITLE, // char*
    UIATTR_WINDOW_NOCLOSE, // unsigned
    UIATTR_WINDOW_NORESIZE, // unsigned
    UIATTR_TEXT_TEXT, // char*, struct uidata_textfrmt*
    UIATTR_TEXT_INTERPESC, // unsigned
    UIATTR_TEXT_FONT, // char*
    UIATTR_TEXT_SIZE, // unsigned
    UIATTR_LINK_TEXT, // char*, struct uidata_textfrmt*, struct uidata_textfrmt*, struct uidata_textfrmt*
    UIATTR_LINK_FONT, // char*
    UIATTR_LINK_SIZE, // unsigned
    UIATTR_BUTTON_TEXT, // char*
    UIATTR_TEXTBOX_TEXT, // char*
    UIATTR_TEXTBOX_SHADOWTEXT, // char*
    UIATTR_TEXTBOX_MULTILINE, // unsigned
    UIATTR_TEXTBOX_PASSWORD, // unsigned
    UIATTR_CHECKBOX_CHECKED, // unsigned
    UIATTR_SLIDER_AMOUNT, // double
    UIATTR_SLIDER_FLUID, // unsigned
    UIATTR_PROGRESSBAR_PERCENTAGE, // double
    UIATTR_SCROLLBAR_TOTAL, // double
    UIATTR_SCROLLBAR_AUTOHIDE, // unsigned
    UIATTR_LIST_ITEMS, // unsigned, char**
    UIATTR_DROPDOWN_TEXT, // char*
    UIATTR_DROPDOWN_OPEN, // unsigned
    UIATTR_SELECTBOX_ITEMS, // unsigned, char**
    UIATTR_SELECTBOX_ITEM, // unsigned
    UIATTR_TABS_ITEMS, // unsigned, char**
    UIATTR_TABS_ITEM, // unsigned
};

struct uidata_textfrmt {
    uint8_t color[4];
    struct {
        uint8_t bold : 1;
        uint8_t italic : 1;
        uint8_t underline : 1;
        uint8_t strikethrough : 1;
    };
};

enum uievent {
    UIEVENT_WINDOW_MOVE,
    UIEVENT_WINDOW_RESIZE,
    UIEVENT_WINDOW_CLOSE,
    UIEVENT_BUTTON_USE, // May happen on RELEASE instead of PRESS
    UIEVENT_BUTTON_PRESS,
    UIEVENT_BUTTON_RELEASE,
    UIEVENT_LINK_USE,
    UIEVENT_TEXTBOX_ENTER, // Does not fire if multiline is enabled
    UIEVENT_CHECKBOX_CHECK,
    UIEVENT_CHECKBOX_UNCHECK,
    UIEVENT_SLIDER_MOVE, // Fires only when released if UIATTR_SLIDER_FLUID is disabled
    UIEVENT_SCROLLBAR_MOVE,
    UIEVENT_DROPDOWN_OPEN,
    UIEVENT_DROPDOWN_CLOSE,
    UIEVENT_SELECTBOX_PICK,
    UIEVENT_TABS_PICK,
};

struct uievent_data {
    int element;
    void* userdata;
    union {
        union {
            struct {
                float x;
                float y;
            } move;
            struct {
                float width;
                float height;
            } resize;
        } window;
        union {
            struct {
                char* text; // dup to use outside of callback
            } enter;
        } textbox;
        union {
            struct {
                float amount;
            } move;
        } slider;
        union {
            struct {
                float amount;
            } move;
        } scrollbar;
        union {
            struct {
                unsigned item;
            } pick;
        } selectbox;
        union {
            struct {
                unsigned item;
            } pick;
        } tabs;
    };
};

#endif
