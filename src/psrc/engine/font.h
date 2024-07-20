#ifndef PSRC_ENGINE_FONT_H
#define PSRC_ENGINE_FONT_H

#include <stdint.h>
#include <stdbool.h>
#include "../../schrift/schrift.h"

#include "../attribs.h"

#define FA_BOLD (1 << 0)
#define FA_ITALIC (1 << 1)
#define FA_UNDERLINE (1 << 2)
#define FA_STRIKETHROUGH (1 << 3)

#define FONTCOLOR(r, g, b, a) ((((r) & 255) << 24) | (((g) & 255) << 16) | (((b) & 255) << 8) | ((a) & 255))

struct font {
    SFT_Font* font;
};

#pragma pack(push, 1)
struct fontpage_glyph {
    uint16_t x;
    uint16_t y;
    uint8_t w;
    uint8_t h;
};
#pragma pack(pop)
struct fontpage {
    uint8_t uses;
    unsigned page;
    uint8_t pt;
    uint8_t attr;
    struct fontpage_glyph glyphs[128];
    uint8_t size; // 2^n
    uint8_t* pixels;
};

struct fontcache {
    struct font* font;
    unsigned pagecount;
    unsigned maxpages;
    struct fontpage* pages;
};

PACKEDENUM fontdrawopcode {
    FDOP_PAGE,  // page
    FDOP_FGC,   // color
    FDOP_BGC,   // color
    FDOP_GLYPH, // glyph
    FDOP_X,     // coord
    FDOP_Y      // coord
};
#pragma pack(push, 1)
struct fontdrawop_page {
    uint8_t pt;
    uint8_t attr;
};
struct fontdrawop_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};
struct fontdrawop_glyph {
    uint8_t index;
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;
};
struct fontdrawop_coord {
    uint16_t value;
};
struct fontdrawop {
    enum fontdrawopcode opcode;
    union {
        struct fontdrawop_page page;
        struct fontdrawop_color color;
        struct fontdrawop_glyph glyph;
        struct fontdrawop_coord coord;
    };
};
#pragma pack(pop)

enum fontdrawjust {
    FDJ_LEFT,
    FDJ_CENTER,
    FDJ_RIGHT
};
enum fontdrawattr {
    FDA_DONE,
    FDA_X,     // uint16_t
    FDA_Y,     // uint16_t
    FDA_PT,    // unsigned
    FDA_ATTR,  // unsigned
    FDA_FGC,   // uint32_t
    FDA_BGC,   // uint32_t
    FDA_VJUST, // enum fontdrawjust
    FDA_HJUST  // enum fontdrawjust
};

struct font* openFont(char*);
void closeFont(struct font*);

struct fontcache* newFontCache(struct font*, int maxpages);
struct fontpage* getFontPage(struct fontcache*, uint8_t pt, uint8_t attr);
void returnFontPage(struct fontpage*);
void deleteFontCache(struct fontcache*);

struct fontdrawing* createFontDrawing(struct fontcache*, uint16_t minw, uint16_t minh, uint16_t maxw, uint16_t maxh);
void setFontDrawingPt(struct fontdrawing*, uint8_t pt);
void setFontDrawingAttr(struct fontdrawing*, enum fontdrawattr, ...);
void addFontDrawingText(struct fontdrawing*, char*);
void resetFontDrawOp(struct fontdrawing*);
bool nextFontDrawOp(struct fontdrawing*, struct fontdrawop* op);
void deleteFontDrawing(struct fontdrawing*);


#endif
