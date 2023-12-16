#ifndef PSRC_ENGINE_UI_H
#define PSRC_ENGINE_UI_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "../utils/threading.h"

enum __attribute__((packed)) uielemtype {
    UIELEMTYPE_CONTAINER,
    UIELEMTYPE_BOX,
    UIELEMTYPE_TEXT,
    UIELEMTYPE_LINK,
    UIELEMTYPE_BUTTON,
    UIELEMTYPE_TEXTBOX,
    UIELEMTYPE_CHECKBOX,
    UIELEMTYPE_SLIDER,
    UIELEMTYPE_PROGRESSBAR,
    UIELEMTYPE_DROPDOWN,
    UIELEMTYPE_LIST,
};

enum uiattr {
    UIATTR_END,
    // general
    UIATTR_HIDDEN, // unsigned
    UIATTR_DISABLED, // unsigned
    UIATTR_CALLBACK, // void (*)(enum uievent, struct uievent_data*), void*
    UIATTR_PARENT, // struct uielemptr*
    UIATTR_ANCHOR, // struct uielemptr*
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
    UIATTR_DROPDOWN_ITEMS, // int, char**
    UIATTR_LIST_ITEMS, // int, char**, int, char***
    UIATTR_LIST_ROW, // int, char**
    UIATTR_LIST_NEWROW, // char**
    UIATTR_LIST_ITEM, // int, int, char*
    UIATTR_LIST_SORTBY, // int, unsigned
    UIATTR_LIST_HIDECOLNAMES, // unsigned
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
    struct uidata_color color;
    struct {
        uint8_t bold : 1;
        uint8_t italic : 1;
        uint8_t underline : 1;
        uint8_t strikethrough : 1;
    };
};

struct uielemptr {
    int index;
    uint64_t id;
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
    enum __attribute__((packed)) uiborder border;
};
struct uielem_link {
    char* text;
    char* font;
    struct uidata_textfrmt frmt;
    struct uidata_textfrmt frmt_hover;
    struct uidata_textfrmt frmt_use;
    enum __attribute__((packed)) uiborder border;
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
struct uielem_list {
    int columns;
    int rows;
    int sortcolumn;
    uint8_t sortreversed : 1;
    uint8_t hidecolnames : 1;
    float* seppos; // # of cols - 1
    float* rowpos; // # of rows - 1
    char** colnames;
    char*** data;
};

enum uievent {
    UIEVENT_BOUNDSREQ,
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
    UIEVENT_LIST_SEPPOSREQ,
    UIEVENT_LIST_SORT, // Fires when the list is changed
};

struct uievent_data {
    struct uielemptr element;
    union {
        struct {
            float elem[2][2];
            float parent[2][2];
        } boundsreq;
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
                int item;
            } select;
        } dropdown;
        union {
            struct {
                float* seppos;
            } sepposreq;
            struct {
                int column;
                uint8_t reversed : 1;
                int columns;
                int rows;
                char*** data;
            } sort;
        } list;
    };
};

struct uielem {
    uint64_t id;
    uint8_t hidden : 1;
    uint8_t disabled : 1;
    void (*callback)(enum uievent, struct uievent_data*, void*);
    void* userdata;
    struct uielemptr anchor;
    struct {
        unsigned len;
        unsigned size;
        struct uielemptr* data;
    } children;
    float coords[2][2];
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
        struct uielem_list list;
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
        struct uielemptr* data;
    } orphans;
};

extern struct uistate uistate;

bool initUI(void);
bool newUIElem(struct uielemptr*, enum uielemtype, enum uiattr, ...);
void editUIElem(struct uielemptr*, enum uiattr, ...);
void deleteUIElem(struct uielemptr*);
void clearUIElems(void);
void doUIEvents(void);
void termUI(void);

#endif
