null := /dev/null

ifndef MKSUB

MODULE ?= engine
SRCDIR ?= src
OBJDIR ?= obj
EXTDIR ?= external
OUTDIR ?= .

ifeq ($(CROSS),)
    CC ?= gcc
    CC := $(PREFIX)$(CC)
    LD := $(CC)
    AR ?= ar
    AR := $(PREFIX)$(AR)
    STRIP ?= strip
    STRIP := $(PREFIX)$(STRIP)
    ifndef M32
        PLATFORM := $(subst $() $(),_,$(subst /,_,$(shell uname -o 2> $(null) || uname -s; uname -m)))
    else
        PLATFORM := $(subst $() $(),_,$(subst /,_,$(shell i386 uname -o 2> $(null) || i386 uname -s; i386 uname -m)))
    endif
else ifeq ($(CROSS),freebsd)
    FREEBSD_VERSION := 12.4
    ifndef M32
        CC := clang -target x86_64-unknown-freebsd$(FREEBSD_VERSION)
    else
        CC := clang -target i686-unknown-freebsd$(FREEBSD_VERSION)
    endif
    CC := $(PREFIX)$(CC)
    LD := $(CC)
    AR ?= ar
    AR := $(PREFIX)$(AR)
    STRIP ?= $(PREFIX)strip
    ifndef M32
        PLATFORM := FreeBSD_$(FREEBSD_VERSION)_x86_64
    else
        PLATFORM := FreeBSD_$(FREEBSD_VERSION)_i686
    endif
    CC += --sysroot=$(EXTDIR)/$(PLATFORM)
else ifeq ($(CROSS),win32)
    ifndef M32
        PREFIX := x86_64-w64-mingw32-
        PLATFORM := Windows_x86_64
    else
        PREFIX := i686-w64-mingw32-
        PLATFORM := Windows_i686
    endif
    CC ?= gcc
    CC := $(PREFIX)$(CC)
    LD := $(CC)
    AR ?= ar
    AR := $(PREFIX)$(AR)
    STRIP ?= strip
    STRIP := $(PREFIX)$(STRIP)
    WINDRES ?= windres
    WINDRES := $(PREFIX)$(WINDRES)
else ifeq ($(CROSS),emscr)
    ifeq ($(MODULE),engine)
    else ifneq ($(MODULE),editor)
        $(error Invalid module: $(MODULE))
    endif
    PLATFORM := Emscripten
    CC := emcc
    LD := $(CC)
    AR := emar
    NOSTRIP := y
    NOMT := y
    NOGL33 := y
    NOGLES30 := y
    EMULATOR := emrun
    EMUPATHFLAG := --
else ifeq ($(CROSS),nxdk)
    ifndef NXDK_DIR
        $(error Please define the NXDK_DIR environment variable)
    endif
    ifneq ($(MODULE),engine)
        $(error Invalid module: $(MODULE))
    endif
    ifdef CC
        _CC := $(CC)
    else
        _CC := gcc
    endif
    _CC := $(PREFIX)$(_CC)
    ifdef LD
        _LD := $(LD)
    else
        _LD := ld
    endif
    _LD := $(PREFIX)$(_LD)
    CC := nxdk-cc
    LD := nxdk-link
    AR ?= ar
    AR := $(PREFIX)$(AR)
    OBJCOPY ?= objcopy
    OBJCOPY := $(PREFIX)$(OBJCOPY)
    CXBE := $(NXDK_DIR)/tools/cxbe/cxbe
    EXTRACT_XISO := $(NXDK_DIR)/tools/extract-xiso/build/extract-xiso
    PLATFORM := NXDK
    XBE_TITLE := PlatinumSrc
    XBE_TITLEID := PQ-001
    XBE_VERSION := $(shell grep '#define PSRC_BUILD ' $(SRCDIR)/psrc/version.h | sed 's/#define .* //')
    XBE_XTIMAGE := icons/engine.xpr
    NOLTO := y
    NOSIMD := y
    NOGL33 := y
    NOGLES30 := y
    EMULATOR := xemu
    EMUPATHFLAG := -dvd_path
    XISO := $(OUTDIR)/$(XBE_TITLE).xiso.iso
    XISODIR := $(OUTDIR)/xiso
    MKENV.NXDK := $(MKENV.NXDK) NXDK_ONLY=y NXDK_SDL=y LIB="nxdk-lib -llvmlibempty"
    MKENV.NXDK := $(MKENV.NXDK) LIBSDLIMAGE_SRCS="" LIBSDLIMAGE_OBJS=""
    MKENV.NXDK := $(MKENV.NXDK) FREETYPE_SRCS="" FREETYPE_OBJS=""
    MKENV.NXDK := $(MKENV.NXDK) SDL_TTF_SRCS="" SDL_TTF_OBJS=""
    MKENV.NXDK := $(MKENV.NXDK) SDL2TEST_SRCS="" SDL2TEST_OBJS=""
    MKENV.NXDK := $(MKENV.NXDK) LIBCXX_SRCS="" LIBCXX_OBJS=""
    MKENV.NXDK := $(MKENV.NXDK) LIBPNG_SRCS="" LIBPNG_OBJS=""
    MKENV.NXDK := $(MKENV.NXDK) LIBJPEG_TURBO_OBJS="" LIBJPEG_TURBO_SRCS=""
    MKENV.NXDK := $(MKENV.NXDK) ZLIB_OBJS="" ZLIB_SRCS=""
    _default: default
	    @:
else
    $(error Invalid cross-compilation target: $(CROSS))
endif

ifeq ($(MODULE),engine)
else ifeq ($(MODULE),server)
else ifeq ($(MODULE),editor)
else
    $(error Invalid module: $(MODULE))
endif

PLATFORMDIRNAME := $(MODULE)
ifdef USE_DISCORD_GAME_SDK
    PLATFORMDIRNAME := $(PLATFORMDIRNAME)_discordgsdk
endif
ifndef DEBUG
    PLATFORMDIR := release/$(PLATFORM)
else
    ifdef ASAN
        PLATFORMDIRNAME := $(PLATFORMDIRNAME)_asan
    endif
    PLATFORMDIR := debug/$(PLATFORM)
endif
PLATFORMDIR := $(PLATFORMDIR)/$(PLATFORMDIRNAME)
_OBJDIR := $(OBJDIR)/$(PLATFORMDIR)

ifeq ($(OS),Windows_NT)
    CC := gcc
    LD := $(CC)
    CROSS := win32
    WINDRES := windres
endif

ifeq ($(CROSS),win32)
    SOSUF := .dll
else ifneq ($(CROSS),nxdk)
    SOSUF := .so
endif

_CFLAGS := $(CFLAGS) -I$(EXTDIR)/$(PLATFORM)/include -I$(EXTDIR)/include -fno-exceptions -Wall -Wextra -Wuninitialized
_CPPFLAGS := $(CPPFLAGS) -DMODULE=$(MODULE)
_LDFLAGS := $(LDFLAGS) -L$(EXTDIR)/$(PLATFORM)/lib -L$(EXTDIR)/lib
_LDLIBS := $(LDLIBS)
_WRFLAGS := $(WRFLAGS)
ifneq ($(CROSS),nxdk)
    _CFLAGS += -Wundef -fvisibility=hidden
    ifndef NOFASTMATH
        _CFLAGS += -ffast-math
    endif
    _CPPFLAGS += -D_DEFAULT_SOURCE -D_GNU_SOURCE
    _LDLIBS += -lm
    ifeq ($(CROSS),win32)
        _LDFLAGS += -static -static-libgcc
    else ifeq ($(CROSS),emscr)
        _LDFLAGS += -sALLOW_MEMORY_GROWTH -sEXIT_RUNTIME -sEXPORTED_RUNTIME_METHODS=ccall
        _LDFLAGS += --shell-file $(SRCDIR)/psrc/emscr_shell.html
        _LDLIBS += -lidbfs.js
        ifndef NOGL
            _LDFLAGS += -sLEGACY_GL_EMULATION -sGL_UNSAFE_OPTS=0
            _LDLIBS += -lGL
        endif
        _LDFLAGS += --embed-file common/ --embed-file engine/ --embed-file games/ --embed-file mods/
    endif
    ifdef M32
        _CPPFLAGS += -DM32
        ifeq ($(CROSS),win32)
            _WRFLAGS += -DM32
        endif
    endif
    ifndef NOMT
        ifneq ($(CROSS),win32)
            _CFLAGS += -pthread
            _LDLIBS += -lpthread
        else
            ifdef WINPTHREAD
                _CFLAGS += -pthread
                _CPPFLAGS += -DPSRC_COMMON_THREADING_WINPTHREAD
                _LDLIBS += -l:libwinpthread.a
            endif
        endif
    endif
    ifneq ($(CROSS),emscr)
        ifdef DEBUG
            _LDFLAGS += -Wl,-R$(EXTDIR)/$(PLATFORM)/lib -Wl,-R$(EXTDIR)/lib
        endif
    endif
    ifdef NATIVE
        _CFLAGS += -march=native -mtune=native
    endif
else
    _CPPFLAGS += -DPB_HAL_FONT
endif
ifdef NOSIMD
    _CPPFLAGS += -DPSRC_NOSIMD
endif
ifdef NOMT
    _CPPFLAGS += -DPSRC_NOMT
endif
ifdef NOGL
    _CPPFLAGS += -DPSRC_ENGINE_RENDERER_NOGL
else
    ifdef NOGL11
        _CPPFLAGS += -DPSRC_ENGINE_RENDERER_NOGL11
    endif
    ifdef NOGL33
        _CPPFLAGS += -DPSRC_ENGINE_RENDERER_NOGL33
    endif
    ifdef NOGLES30
        _CPPFLAGS += -DPSRC_ENGINE_RENDERER_NOGLES30
    endif
endif
ifndef DEBUG
    _CPPFLAGS += -DNDEBUG
    ifndef O
        O := 2
    endif
    _CFLAGS += -O$(O)
    ifeq ($(CROSS),emscr)
        _LDFLAGS += -O$(O)
    endif
    ifndef NOLTO
        _CFLAGS += -flto
        ifneq ($(CROSS),nxdk)
            _LDFLAGS += -flto
        else
            MKENV.NXDK := $(MKENV.NXDK) LTO=y
        endif
    endif
else
    _CPPFLAGS += -DPSRC_DBGLVL=$(DEBUG)
    ifneq ($(CROSS),nxdk)
        _CFLAGS += -Og -g
    else
        _CFLAGS += -g -gdwarf-4
        _LDFLAGS += -debug
        MKENV.NXDK := $(MKENV.NXDK) DEBUG=y
    endif
    ifeq ($(CROSS),win32)
        _WRFLAGS += -DPSRC_DBGLVL=$(DEBUG)
    endif
    NOSTRIP := y
    ifdef ASAN
        _CFLAGS += -fsanitize=address
        _LDFLAGS += -fsanitize=address
    endif
endif

define so
$(1)$(SOSUF)
endef

CPPFLAGS.dir.psrc_common := 
ifeq ($(CROSS),nxdk)
    CPPFLAGS.dir.psrc_common += -DPSRC_COMMON_THREADING_STDC
endif
LDLIBS.dir.psrc_common := 
ifeq ($(CROSS),win32)
    LDLIBS.dir.psrc_common += -lwinmm
endif

LDLIBS.dir.psrc_server := 
ifeq ($(CROSS),win32)
    LDLIBS.dir.psrc_server += -lws2_32
endif

CPPFLAGS.dir.minimp3 := -DMINIMP3_NO_STDIO
ifdef NOSIMD
    CPPFLAGS.dir.minimp3 += -DMINIMP3_NO_SIMD
endif

CPPFLAGS.dir.minimp3 := -DMINIMP3_NO_STDIO
ifdef NOSIMD
    CPPFLAGS.dir.minimp3 += -DMINIMP3_NO_SIMD
endif

CPPFLAGS.dir.schrift := 
ifeq ($(CROSS),nxdk)
    CPPFLAGS.dir.schrift += -DSCHRIFT_NO_FILE_MAPPING
endif

CPPFLAGS.dir.stb := -DSTBI_ONLY_PNG -DSTBI_ONLY_JPEG -DSTBI_ONLY_TGA -DSTBI_ONLY_BMP
ifdef NOSIMD
    CPPFLAGS.dir.stb += -DSTBI_NO_SIMD
endif
ifdef NOMT
    CPPFLAGS.dir.stb += -DSTBI_NO_THREAD_LOCALS
endif

CPPFLAGS.lib.discord_game_sdk := 
LDLIBS.lib.discord_game_sdk := 
ifdef USE_DISCORD_GAME_SDK
    CPPFLAGS.lib.discord_game_sdk += -DUSE_DISCORD_GAME_SDK
    LDLIBS.lib.discord_game_sdk += -l:$(call so,discord_game_sdk)
endif

CFLAGS.lib.SDL2 := 
CPPFLAGS.lib.SDL2 := 
LDLIBS.lib.SDL2 := 
ifeq ($(CROSS),win32)
    CPPFLAGS.lib.SDL2 += -DSDL_MAIN_HANDLED
    LDLIBS.lib.SDL2 += -l:libSDL2.a -lole32 -loleaut32 -limm32 -lsetupapi -lversion -lgdi32 -lwinmm
else ifeq ($(CROSS),emscr)
    CFLAGS.lib.SDL2 += -sUSE_SDL=2
    LDLIBS.lib.SDL2 += -sUSE_SDL=2
else ifneq ($(CROSS),nxdk)
    LDLIBS.lib.SDL2 += -lSDL2
endif

ifeq ($(MODULE),engine)
    _CPPFLAGS += -DMODULE_ENGINE
    _WRFLAGS += -DMODULE_ENGINE
    _CFLAGS += $(CFLAGS.lib.SDL2)
    _CPPFLAGS += $(CPPFLAGS.dir.stb) $(CPPFLAGS.dir.minimp3) $(CPPFLAGS.dir.schrift) $(CPPFLAGS.dir.psrc_common)
    _CPPFLAGS += $(CPPFLAGS.lib.SDL2) $(CPPFLAGS.lib.discord_game_sdk)
    _LDLIBS +=  $(LDLIBS.dir.psrc_engine) $(LDLIBS.dir.psrc_common)
    _LDLIBS +=  $(LDLIBS.lib.discord_game_sdk) $(LDLIBS.lib.SDL2)
else ifeq ($(MODULE),server)
    _CPPFLAGS += -DMODULE_SERVER
    _WRFLAGS += -DMODULE_SERVER
    _CPPFLAGS += $(CPPFLAGS.lib.discord_game_sdk)
    _LDLIBS += $(LDLIBS.dir.psrc_common) $(LDLIBS.lib.discord_game_sdk)
else ifeq ($(MODULE),editor)
    _CPPFLAGS += -DMODULE_EDITOR
    _WRFLAGS += -DMODULE_EDITOR
    _CFLAGS += $(CFLAGS.lib.SDL2)
    _CPPFLAGS += $(CPPFLAGS.dir.stb) $(CPPFLAGS.dir.minimp3) $(CPPFLAGS.dir.schrift) $(CPPFLAGS.dir.psrc_common)
    _CPPFLAGS += $(CPPFLAGS.lib.SDL2) $(CPPFLAGS.lib.discord_game_sdk)
    _LDLIBS += $(LDLIBS.dir.psrc_engine) $(LDLIBS.dir.psrc_common)
    _LDLIBS += $(LDLIBS.lib.discord_game_sdk) $(LDLIBS.lib.SDL2)
endif

ifeq ($(CROSS),nxdk)

__CFLAGS := $(_CFLAGS) $(_CPPFLAGS)
__CFLAGS := $(filter-out -Wall,$(__CFLAGS))
__CFLAGS := $(filter-out -Wextra,$(__CFLAGS))
__CFLAGS := $(filter-out -Wuninitialized,$(__CFLAGS))
__LDFLAGS := $(_LDFLAGS)

include $(NXDK_DIR)/lib/Makefile
include $(NXDK_DIR)/lib/net/Makefile
include $(NXDK_DIR)/lib/sdl/SDL2/Makefile.xbox
include $(NXDK_DIR)/lib/sdl/Makefile

_CFLAGS += $(NXDK_CFLAGS)
_LDFLAGS += $(NXDK_LDFLAGS)

endif

ifeq ($(MODULE),server)
BIN := psrc-server
else ifeq ($(MODULE),editor)
BIN := psrc-editor
else
BIN := psrc
endif
ifdef DEBUG
    BIN := $(BIN).debug
    ifdef ASAN
        BIN := $(BIN).asan
    endif
endif
BINPATH := $(BIN)
ifeq ($(CROSS),win32)
    BINPATH := $(BINPATH).exe
else ifeq ($(CROSS),emscr)
    BINPATH := $(BINPATH).html
else ifeq ($(CROSS),nxdk)
    BINPATH := $(BINPATH).exe
endif
BINPATH := $(OUTDIR)/$(BINPATH)

ifeq ($(CROSS),win32)
    ifneq ($(WINDRES),)
        WRSRC := $(SRCDIR)/psrc/winver.rc
        WROBJ := $(_OBJDIR)/psrc/winver.o
        _WROBJ = $$(test -f $(WROBJ) && echo $(WROBJ))
    endif
endif

ifeq ($(CROSS),nxdk)
TARGET = $(XISO)
else ifeq ($(CROSS),emscr)
TARGET := index.html
else
TARGET = $(BINPATH)
endif

else

TARGET = $(BINPATH)

endif

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(_OBJDIR)/%.o,$(SOURCES))

export MODULE
export CROSS

export CC
export AR

export _CFLAGS
export _CPPFLAGS

export SRCDIR
export _OBJDIR
export PLATFORM
export PLATFORMDIR

define mkdir
if [ ! -d '$(1)' ]; then echo 'Creating $(1)...'; mkdir -p '$(1)'; fi; true
endef
define rm
if [ -f '$(1)' ]; then echo 'Removing $(1)...'; rm -f '$(1)'; fi; true
endef
define rmdir
if [ -d '$(1)' ]; then echo 'Removing $(1)...'; rm -rf '$(1)'; fi; true
endef
ifndef EMULATOR
define exec
'$(dir $(1))$(notdir $(1))' $(RUNFLAGS)
endef
else
define exec
$(EMULATOR) $(EMUFLAGS) $(EMUPATHFLAG) '$(1)' $(RUNFLAGS)
endef
endif

.SECONDEXPANSION:

define a
$(shell [ -z "$$(ls -A '$(SRCDIR)/$(1)' 2> $(null))" ] || echo '$(_OBJDIR)/$(1).a')
endef
inc.null := $(null)
define inc
$$(patsubst $(inc.null)\:,,$$(patsubst $(inc.null),,$$(wildcard $$(shell $(CC) $(_CFLAGS) $(_CPPFLAGS) -x c -MM $(inc.null) $$(wildcard $(1)) -MT $(inc.null)))))
endef

default: build

ifndef MKSUB
define a_path
$(patsubst $(_OBJDIR)/%,%,$(1))
endef
$(_OBJDIR)/%.a: $$(wildcard $(SRCDIR)/$(call a_path,%)/*.c) $(call inc,$(SRCDIR)/$(call a_path,%)/*.c)
	@'$(MAKE)' --no-print-directory MKSUB=y SRCDIR=$(SRCDIR)/$(call a_path,$*) _OBJDIR=$(_OBJDIR)/$(call a_path,$*) OUTDIR=$(_OBJDIR) BINPATH=$@
endif

$(OUTDIR):
	@$(call mkdir,$@)

$(_OBJDIR):
	@$(call mkdir,$@)

$(_OBJDIR)/%.o: $(SRCDIR)/%.c $(call inc,$(SRCDIR)/%.c) | $(_OBJDIR) $(OUTDIR)
	@echo Compiling $(notdir $<)...
	@$(CC) $(_CFLAGS) $(_CPPFLAGS) $< -c -o $@
	@echo Compiled $(notdir $<)

ifndef MKSUB

a.dir.psrc_common = $(call a,psrc/common)
ifneq ($(MODULE),server)
a.dir.psrc_common += $(call a,stb) $(call a,minimp3) $(call a,schrift)
endif

a.dir.psrc_editor = $(call a,psrc/editor) $(a.dir.psrc_engine)

a.dir.psrc_engine = $(call a,psrc/engine) $(a.dir.psrc_server)
ifeq ($(CROSS),emscr)
else ifeq ($(CROSS),nxdk)
else ifndef NOGL
a.dir.psrc_engine += $(call a,glad)
endif

a.dir.psrc_server = $(call a,psrc/server)

a.list = $(call a,psrc)
ifeq ($(MODULE),engine)
a.list += $(a.dir.psrc_engine)
else ifeq ($(MODULE),server)
a.list += $(a.dir.psrc_server)
else ifeq ($(MODULE),editor)
a.list += $(a.dir.psrc_editor)
endif
a.list += $(a.dir.psrc_common) $(call a,psrc)

$(BINPATH): $(OBJECTS) $(a.list)
ifeq ($(CROSS),nxdk)
	@echo Making NXDK libs...
	@export CFLAGS='$(__CFLAGS)'; export LDFLAGS='$(__LDFLAGS)'; '$(MAKE)' --no-print-directory -C '$(NXDK_DIR)' ${MKENV.NXDK} main.exe
	@echo Made NXDK libs
endif
	@echo Linking $(notdir $@)...
ifeq ($(CROSS),win32)
ifneq ($(WINDRES),)
	@$(WINDRES) $(_WRFLAGS) $(WRSRC) -o $(WROBJ) || exit 0
endif
endif
ifneq ($(CROSS),nxdk)
	@$(LD) $(_LDFLAGS) $^ $(_WROBJ) $(_LDLIBS) -o $@
ifndef NOSTRIP
	@$(STRIP) -s -R '.comment' -R '.note.*' -R '.gnu.build-id' $@ || exit 0
endif
else
	@$(LD) $(_LDFLAGS) $^ '$(NXDK_DIR)'/lib/*.lib '$(NXDK_DIR)'/lib/xboxkrnl/libxboxkrnl.lib $(_LDLIBS) -out:$@ > $(null)
ifneq ($(XBE_XTIMAGE),)
	@$(OBJCOPY) --long-section-names=enable --update-section 'XTIMAGE=$(XBE_XTIMAGE)' $@ || exit 0
endif
	@$(OBJCOPY) --long-section-names=enable --rename-section 'XTIMAGE=$$$$XTIMAGE' --rename-section 'XSIMAGE=$$$$XSIMAGE' $@ || exit 0
endif
	@echo Linked $(notdir $@)

else

$(BINPATH): $(OBJECTS) | $(OUTDIR)
	@echo Building $(notdir $@)...
	@$(AR) rcs $@ $^
	@echo Built $(notdir $@)

endif

build: $(TARGET)
	@:

run: build
	@echo Running $(notdir $(TARGET))...
	@$(call exec,$(TARGET))

clean:
	@$(call rmdir,$(_OBJDIR))

distclean: clean
ifeq ($(CROSS),nxdk)
	@$(call rm,$(XISODIR)/default.xbe)
	@$(call rm,$(BINPATH))
else ifeq ($(CROSS),emscr)
	@$(call rm,$(basename $(BINPATH)).js)
	@$(call rm,$(basename $(BINPATH)).wasm)
endif
	@$(call rm,$(TARGET))

externclean:
ifneq ($(CROSS),nxdk)
	@:
else
	@echo Cleaning NXDK...
	@'$(MAKE)' --no-print-directory -C '$(NXDK_DIR)' ${MKENV.NXDK} clean
endif

.PHONY: build run clean distclean externclean

ifeq ($(CROSS),nxdk)

$(XISODIR):
	@$(call mkdir,$@)

$(CXBE):
	@echo Making NXDK Cxbe tool...
	@'$(MAKE)' CC='$(_CC)' LD='$(_LD)' --no-print-directory -C '$(NXDK_DIR)' cxbe > $(null)

$(EXTRACT_XISO):
	@echo Making NXDK extract-xiso tool...
	@'$(MAKE)' CC='$(_CC)' LD='$(_LD)' --no-print-directory -C '$(NXDK_DIR)' extract-xiso > $(null)

$(XISODIR)/default.xbe: $(BINPATH) | $(XISODIR)
	@echo Relinking $@ from $<...
	@'$(CXBE)' -OUT:$@ -TITLE:$(XBE_TITLE) -TITLEID:$(XBE_TITLEID) -VERSION:$(XBE_VERSION) $< > $(null)

$(XISO): $(XISODIR)/default.xbe $(EXTRACT_XISO) | $(XISODIR)
	@echo Creating $@...
	@'$(EXTRACT_XISO)' -c $(XISODIR) "$@" > $(null)

else ifeq ($(CROSS),emscr)

index.html: $(BINPATH)
	@cp -f $(BINPATH) index.html

endif
