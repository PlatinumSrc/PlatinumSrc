#ifndef PSRC_ENGINE_UI_H
#define PSRC_ENGINE_UI_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "../common/threading.h"
#include "../common/string.h"

enum __attribute__((packed)) uielemtype {
    UIELEMTYPE__INVALID,
    UIELEMTYPE_CONTAINER,
    UIELEMTYPE_BOX,
    UIELEMTYPE_TEXT,
    UIELEMTYPE_LINK,
    UIELEMTYPE_BUTTON,
    UIELEMTYPE_TEXTBOX,
    UIELEMTYPE_MULTITEXTBOX,
    UIELEMTYPE_CHECKBOX,
    UIELEMTYPE_SLIDER,
    UIELEMTYPE_PROGRESSBAR,
    UIELEMTYPE_IMAGE,
    UIELEMTYPE_DROPDOWN,
    UIELEMTYPE_LIST,
    UIELEMTYPE__COUNT
};

enum uiattr {
    UIATTR_END,
    // general
    UIATTR_HIDDEN, // unsigned
    UIATTR_DISABLED, // unsigned
    UIATTR_CALLBACK, // void (*)(enum uievent, struct uievent_data*), void*
    UIATTR_PARENT, // struct uielemptr*
    UIATTR_ANCHOR, // struct uielemptr*
    UIATTR_X, // char*
    UIATTR_Y, // char*
    UIATTR_WIDTH, // char*
    UIATTR_HEIGHT, // char*
    UIATTR_MAXWIDTH, // char*
    UIATTR_MAXHEIGHT, // char*
    // element-specific
    UIATTR_CONTAINER_FORCESCROLLBAR, // unsigned
    UIATTR_BOX_COLOR, // struct uidata_color*
    UIATTR_TEXT_TEXT, // char*, struct uidata_textfrmt*
    UIATTR_TEXT_INTERPESC, // unsigned
    UIATTR_TEXT_FONT, // char*
    UIATTR_TEXT_SIZE, // unsigned
    UIATTR_TEXT_BORDER, // enum uiborder
    UIATTR_LINK_TEXT, // char*, struct uidata_textfrmt*, struct uidata_textfrmt* click, struct uidata_textfrmt* hover
    UIATTR_LINK_FONT, // char*
    UIATTR_LINK_SIZE, // unsigned
    UIATTR_LINK_BORDER, // enum uiborder
    UIATTR_BUTTON_TEXT, // char*
    UIATTR_TEXTBOX_TEXT, // char*
    UIATTR_TEXTBOX_EDITABLE, // unsigned
    UIATTR_TEXTBOX_SHADOWTEXT, // char*
    UIATTR_TEXTBOX_PASSWORD, // unsigned
    UIATTR_MULTITEXTBOX_TEXT, // char*
    UIATTR_MULTITEXTBOX_EDITABLE, // unsigned
    UIATTR_MULTITEXTBOX_SHADOWTEXT, // char*
    UIATTR_CHECKBOX_CHECKED, // unsigned
    UIATTR_SLIDER_SEGMENTS, // int
    UIATTR_SLIDER_FLUID, // unsigned
    UIATTR_PROGRESSBAR_PERCENTAGE, // double
    UIATTR_IMAGE_RCPATH, // char*
    UIATTR_DROPDOWN_ITEMS, // int, char**, int start
    UIATTR_LIST_ITEMS, // int cols, char** colnames, float* seppos, int rows, char**
    UIATTR_LIST_ROW, // int, char**
    UIATTR_LIST_NEWROW, // char**
    UIATTR_LIST_ITEM, // int col, int row, char*
    UIATTR_LIST_COLSIZE, // int, float*
    UIATTR_LIST_SORTBY, // int col, unsigned rev
    UIATTR_LIST_HIDECOLNAMES // unsigned
};

enum uiborder {
    UIBORDER_NONE,
    UIBORDER_RAISED,
    UIBORDER_SUNKEN,
    UIBORDER_HIGHLIGHT
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

struct uielem_container {
    int scroll;
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
    struct charbuf text;
    char* shadowtext;
    uint8_t editable : 1;
    uint8_t password : 1;
    unsigned curstart;
    unsigned curend;
};
struct uielem_multitextbox {
    struct charbuf* text;
    char* shadowtext;
    unsigned len;
    unsigned size;
    uint8_t editable : 1;
    unsigned curstart;
    unsigned curend;
    int scroll;
};
struct uielem_checkbox {
    uint8_t checked : 1;
};
struct uielem_slider {
    float amount;
    int steps;
};
struct uielem_progressbar {
    float amount;
};
struct uielem_image {
    char* rcpath;
};
struct uielem_dropdown {
    int items;
    char* itemdata;
    int curitem;
    int hoveritem;
    uint8_t open : 1;
};
struct uielem_list {
    int rows;
    int columns;
    int currow;
    int hoverrow;
    int sortcolumn;
    uint8_t sortreversed : 1;
    uint8_t hidecolnames : 1;
    float* seppos; // # of cols - 1
    char** colnames;
    char** data;
    int scroll;
};

enum uievent {
    UIEVENT_SIZEREQ,
    UIEVENT_BUTTON_USE,
    UIEVENT_LINK_USE,
    UIEVENT_TEXTBOX_ENTER,
    UIEVENT_CHECKBOX_CHECK,
    UIEVENT_CHECKBOX_UNCHECK,
    UIEVENT_SLIDER_MOVE, // Fires only when released if UIATTR_SLIDER_FLUID is disabled
    UIEVENT_DROPDOWN_SELECT,
    UIEVENT_LIST_SELECTROW,
    UIEVENT_LIST_SORT // Fires when the list is changed
};

struct uievent_data {
    int element;
    union {
        struct {
            int anchor[2];
            int parent[2];
            int elem[2];
        } sizereq;
        union {
            struct {
                char* text; // dup to use outside of callback
            } enter;
        } textbox;
        union {
            struct {
                float to;
            } move;
        } slider;
        union {
            struct {
                int item;
            } select;
        } dropdown;
        union {
            struct {
                int row;
                char** data;
            } selectrow;
            struct {
                int column;
                uint8_t reversed : 1;
                int columns;
                int rows;
                char** data;
            } sort;
        } list;
    };
};

struct uielem {
    enum uielemtype type;
    int refs;
    uint8_t hidden : 1;
    uint8_t disabled : 1;
    void (*callback)(enum uievent, struct uievent_data*, void*);
    void* userdata;
    char* size[2];
    char* maxsize[2];
    int calcsize[2];
    int calcmaxsize[2];
    int anchor;
    int parent;
    struct {
        int* data;
        unsigned len;
        unsigned size;
    } children;
};

struct uistate {
    #ifndef PSRC_NOMT
    struct accesslock lock;
    #endif
    int activeelem;
    int hoverelem;
    struct {
        unsigned len;
        unsigned size;
        struct uielem* data;
    } elems;
};

bool createUI(struct uistate*);
int newUIElem(struct uistate*, enum uielemtype, enum uiattr, ...);
void editUIElem(struct uistate*, int, enum uiattr, ...);
void deleteUIElem(struct uistate*, int);
void clearUIElems(struct uistate*);
void doUIEvents(struct uistate*);
void destroyUI(struct uistate*);

#endif
