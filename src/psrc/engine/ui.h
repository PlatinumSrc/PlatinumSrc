#ifndef PSRC_ENGINE_UI_H
#define PSRC_ENGINE_UI_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "../aux/threading.h"

enum uielemtype {
    UIELEMTYPE_CONTAINER,
    UIELEMTYPE_BOX,
    UIELEMTYPE_TEXT,
    UIELEMTYPE_LINK,
    UIELEMTYPE_BUTTON,
    UIELEMTYPE_TEXTBOX,
    UIELEMTYPE_CHECKBOX,
    UIELEMTYPE_SLIDER,
    UIELEMTYPE_PROGRESSBAR,
    UIELEMTYPE_SCROLLBAR,
    UIELEMTYPE_SELECTBOX,
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
    UIATTR_ALIGNMENT, // char*
    UIATTR_SCROLL, // double, double
    UIATTR_PARENT, // int
    UIATTR_ANCHOR, // int
    // element-specific
    UIATTR_CONTAINER_FORCESCROLLBAR, // unsigned
    UIATTR_BOX_COLOR, // struct uidata_color*
    UIATTR_TEXT_TEXT, // char*, struct uidata_textfrmt*
    UIATTR_TEXT_INTERPESC, // unsigned
    UIATTR_TEXT_FONT, // char*
    UIATTR_TEXT_SIZE, // unsigned
    UIATTR_TEXT_BORDER, // enum uiborder
    UIATTR_LINK_TEXT, // char*, struct uidata_textfrmt*, struct uidata_textfrmt*, struct uidata_textfrmt*, struct uidata_textfrmt*
    UIATTR_LINK_FONT, // char*
    UIATTR_LINK_SIZE, // unsigned
    UIATTR_LINK_BORDER, // enum uiborder
    UIATTR_BUTTON_TEXT, // char*
    UIATTR_TEXTBOX_TEXT, // char*
    UIATTR_TEXTBOX_EDITABLE, // unsigned
    UIATTR_TEXTBOX_SHADOWTEXT, // char*
    UIATTR_TEXTBOX_MULTILINE, // unsigned
    UIATTR_TEXTBOX_PASSWORD, // unsigned
    UIATTR_CHECKBOX_CHECKED, // unsigned
    UIATTR_SLIDER_AMOUNT, // double
    UIATTR_SLIDER_FLUID, // unsigned
    UIATTR_PROGRESSBAR_PERCENTAGE, // double
    UIATTR_SELECTBOX_ITEMS, // int, char**
    UIATTR_SELECTBOX_ITEM, // unsigned
};

enum uiborder {
    UIBORDER_NONE,
    UIBORDER_RAISED,
    UIBORDER_SUNKEN,
    UIBORDER_HIGHLIGHT,
};

struct uidata_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};
struct uidata_textfrmt {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    struct {
        uint8_t bold : 1;
        uint8_t italic : 1;
        uint8_t underline : 1;
        uint8_t strikethrough : 1;
    };
};

enum uievent {
    UIEVENT_BUTTON_USE, // May happen on RELEASE instead of PRESS
    UIEVENT_BUTTON_PRESS,
    UIEVENT_BUTTON_RELEASE,
    UIEVENT_LINK_USE,
    UIEVENT_LINK_PRESS,
    UIEVENT_LINK_RELEASE,
    UIEVENT_TEXTBOX_ENTER, // Does not fire if multiline is enabled
    UIEVENT_CHECKBOX_CHECK,
    UIEVENT_CHECKBOX_UNCHECK,
    UIEVENT_SLIDER_MOVE, // Fires only when released if UIATTR_SLIDER_FLUID is disabled
    UIEVENT_SELECTBOX_PICK,
};

struct uievent_data {
    int element;
    void* userdata;
    union {
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
                unsigned item;
            } pick;
        } selectbox;
    };
};

struct uielem_container {
    uint8_t forcescrollbar : 1;
};
struct uielem_box {
    struct uidata_color color;
};
struct uielem_text {
    char* text;
    char* font;
    uint8_t interpesc : 1;
    struct uidata_textfrmt frmt;
    enum uiborder border;
};
struct uielem_link {
    char* text;
    char* font;
    struct uidata_textfrmt frmt;
    struct uidata_textfrmt frmt_hover;
    struct uidata_textfrmt frmt_use;
    enum uiborder border;
};
struct uielem_button {
    char* text;
    struct uidata_textfrmt frmt;
};
struct uielem_textbox {
    char* text;
    uint8_t editable : 1;
    unsigned len;
    unsigned size;
    unsigned curstart;
    unsigned curend;
};
struct uielem_checkbox {
    uint8_t checked : 1;
};
struct uielem_slider {
    float amount;
};
struct uielem_progressbar {
    float amount;
};
struct uielem_selectbox {
    int item;
    unsigned items;
    char** itemdata;
};

struct uielem {
    uint8_t hidden : 1;
    uint8_t disabled : 1;
    void (*callback)(enum uievent, struct uievent_data*);
    void* userdata;
    int anchor;
    struct {
        unsigned len;
        unsigned size;
        int* data;
    } children;
    struct {
        char* pos;
        char* size;
        char* maxsize;
        char* margin;
        char* padding;
        char* alignment;
    } dim;
    struct {
        struct {
            float x;
            float y;
        } pos;
        struct {
            float x;
            float y;
        } size;
        struct {
            float x;
            float y;
        } maxsize;
        struct {
            float top;
            float bottom;
            float left;
            float right;
        } margin;
        struct {
            float top;
            float bottom;
            float left;
            float right;
        } padding;
        struct {
            int8_t x;
            int8_t y;
        } alignment;
    } interpdim;
    struct {
        float coords[4][2];
    } calcdim;
    union {
        struct uielem_container container;
        struct uielem_box box;
        struct uielem_text text;
        struct uielem_link link;
        struct uielem_button button;
        struct uielem_textbox textbox;
        struct uielem_checkbox checkbox;
        struct uielem_slider slider;
        struct uielem_progressbar progressbar;
        struct uielem_selectbox selectbox;
    } data[];
};

struct uistate {
    struct accesslock lock;
    struct {
        unsigned len;
        unsigned size;
        struct uielem* data;
    } elems;
    struct {
        unsigned len;
        unsigned size;
        struct uielem* data;
    } orphans;
};

extern struct uistate uistate;

bool initUI(void);
void termUI(void);
int newUIElem(enum uielemtype, enum uiattr, ...);
void editUIElem(int, enum uiattr, ...);
void deleteUIElem(int);
void clearUIElems(void);
void doUIEvents(void);

#endif
