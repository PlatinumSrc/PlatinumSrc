ifndef MKSUB

MODULE ?= engine
SRCDIR ?= src
OBJDIR ?= obj
EXTDIR ?= external
LIBDIR ?= $(EXTDIR)/lib
INCDIR ?= $(EXTDIR)/include
OUTDIR ?= .

ifeq ($(CROSS),)
    CC ?= gcc
    LD := $(CC)
    AR ?= ar
    STRIP ?= strip
    ifndef M32
        PLATFORM := $(subst $() $(),_,$(subst /,_,$(shell uname -o)_$(shell uname -m)))
    else
        PLATFORM := $(subst $() $(),_,$(subst /,_,$(shell i386 uname -o)_$(shell i386 uname -m)))
    endif
else ifeq ($(CROSS),freebsd)
    FREEBSD_VERSION := 12.4
    ifndef M32
        CC := clang -target x86_64-unknown-freebsd$(FREEBSD_VERSION)
    else
        CC := clang -target i686-unknown-freebsd$(FREEBSD_VERSION)
    endif
    LD := $(CC)
    AR ?= ar
    STRIP ?= strip
    ifndef M32
        PLATFORM := FreeBSD_$(FREEBSD_VERSION)_x86_64
    else
        PLATFORM := FreeBSD_$(FREEBSD_VERSION)_i686
    endif
    CC += --sysroot=$(EXTDIR)/$(PLATFORM)
else ifeq ($(CROSS),win32)
    ifndef M32
        CC := x86_64-w64-mingw32-gcc
        LD := $(CC)
        AR := x86_64-w64-mingw32-ar
        STRIP := x86_64-w64-mingw32-strip
        WINDRES := x86_64-w64-mingw32-windres
        PLATFORM := Windows_x86_64
    else
        CC := i686-w64-mingw32-gcc
        LD := $(CC)
        AR := i686-w64-mingw32-ar
        STRIP := i686-w64-mingw32-strip
        WINDRES := i686-w64-mingw32-windres
        PLATFORM := Windows_i686
    endif
else ifeq ($(CROSS),nxdk)
    ifndef NXDK_DIR
        $(error Please define the NXDK_DIR environment variable)
    endif
    ifneq ($(MODULE),engine)
        $(error Invalid module: $(MODULE))
    endif

    _CC := $(CC)
    _LD := $(LD)
    CC := nxdk-cc
    LD := nxdk-link
    AR ?= ar
    STRIP ?= strip

    CXBE := $(NXDK_DIR)/tools/cxbe/cxbe
    EXTRACT_XISO := $(NXDK_DIR)/tools/extract-xiso/build/extract-xiso

    PLATFORM := NXDK

    XBE_TITLE := PlatinumSrc
    XBE_TITLEID := PQ-001
    XBE_VERSION := $(shell grep '#define PSRC_BUILD ' $(SRCDIR)/psrc/version.h | sed 's/#define .* //')
    XBE_XTIMAGE := icons/engine.xpr

    XISO ?= $(XBE_TITLE).xiso.iso
    XISODIR ?= $(OUTDIR)/xiso

    MKENV.NXDK := $(MKENV.NXDK) NXDK_ONLY=y NXDK_SDL=y LIB="nxdk-lib -llvmlibempty"
    MKENV.NXDK := $(MKENV.NXDK) LIBSDLIMAGE_SRCS="" LIBSDLIMAGE_OBJS=""
    MKENV.NXDK := $(MKENV.NXDK) FREETYPE_SRCS="" FREETYPE_OBJS=""
    MKENV.NXDK := $(MKENV.NXDK) SDL_TTF_SRCS="" SDL_TTF_OBJS=""
    MKENV.NXDK := $(MKENV.NXDK) SDL2TEST_SRCS="" SDL2TEST_OBJS=""
    MKENV.NXDK := $(MKENV.NXDK) LIBCXX_SRCS="" LIBCXX_OBJS=""
    MKENV.NXDK := $(MKENV.NXDK) LIBPNG_SRCS="" LIBPNG_OBJS=""
    MKENV.NXDK := $(MKENV.NXDK) LIBJPEG_TURBO_OBJS="" LIBJPEG_TURBO_SRCS=""

    NOSTRIP := y
    NOLTO := y

    EMULATOR ?= xemu -dvd_path

    _default: default
	    @$(nop)
else
    $(error Invalid cross-compilation target: $(CROSS))
endif

ifeq ($(MODULE),engine)
else ifeq ($(MODULE),server)
else ifeq ($(MODULE),editor)
else ifeq ($(MODULE),toolbox)
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
else ifeq ($(CROSS),nxdk)
else
    SOSUF := .so
endif

_CFLAGS := $(CFLAGS) -I$(INCDIR)/$(PLATFORM) -I$(INCDIR) -Wall -Wextra -Wuninitialized
_CPPFLAGS := $(CPPFLAGS) -D_DEFAULT_SOURCE -D_GNU_SOURCE -DMODULE=$(MODULE)
_LDFLAGS := $(LDFLAGS)
_LDLIBS := $(LDLIBS)
_WRFLAGS := $(WRFLAGS)
ifneq ($(CROSS),nxdk)
    _LDFLAGS += -L$(LIBDIR)/$(PLATFORM) -L$(LIBDIR)
endif
ifeq ($(CROSS),win32)
    _LDFLAGS += -static
    _LDLIBS := -l:libwinpthread.a -lwinmm
else ifeq ($(CROSS),nxdk)
    _CPPFLAGS += -DPB_HAL_FONT
else
    _LDLIBS += -lpthread
endif
ifndef DEBUG
    _CPPFLAGS += -DNDEBUG
    ifndef O
        O := 2
    endif
    _CFLAGS += -O$(O) -fno-exceptions
    ifndef NOLTO
        _CFLAGS += -flto=auto
        ifneq ($(CROSS),nxdk)
            _LDFLAGS += -flto=auto
        else
            MKENV.NXDK := $(MKENV.NXDK) LTO=y
        endif
    endif
else
    _CPPFLAGS += -DDBGLVL=$(DEBUG)
    ifneq ($(CROSS),nxdk)
        _CFLAGS += -Og -g
    else
        _CFLAGS += -g -gdwarf-4
        _LDFLAGS += -debug
        MKENV.NXDK := $(MKENV.NXDK) DEBUG=y
    endif
    ifeq ($(CROSS),win32)
        _WRFLAGS += -DDBGLVL=$(DEBUG)
    endif
    NOSTRIP := y
    ifdef ASAN
        _CFLAGS += -fsanitize=address
        _LDFLAGS += -fsanitize=address
    endif
endif
ifneq ($(CROSS),nxdk)
    _CFLAGS += -pthread -ffast-math -fvisibility=hidden
    _LDLIBS := -lm
    ifdef DEBUG
        _LDFLAGS += -Wl,-R$(LIBDIR)/$(PLATFORM) -Wl,-R$(LIBDIR)
    endif
    ifdef NATIVE
        _CFLAGS += -march=native -mtune=native
    endif
    ifdef M32
        _CPPFLAGS += -DM32
        ifeq ($(CROSS),win32)
            _WRFLAGS += -DM32
        endif
    endif
endif

define so
$(1)$(SOSUF)
endef

CPPFLAGS.dir.psrc_utils := 
ifeq ($(CROSS),nxdk)
    CPPFLAGS.dir.psrc_utils += -DUTILS_THREADING_STDC
endif
LDLIBS.dir.psrc_utils := 
ifeq ($(CROSS),win32)
    LDLIBS.dir.psrc_utils += -lwinmm
endif

LDLIBS.dir.psrc_server := 
ifeq ($(CROSS),win32)
    LDLIBS.dir.psrc_server += -lws2_32
endif

CPPFLAGS.dir.minimp3 := -DMINIMP3_NO_STDIO
ifeq ($(CROSS),nxdk)
    CPPFLAGS.dir.minimp3 += -DMINIMP3_NO_SIMD
endif

CPPFLAGS.dir.minimp3 := -DMINIMP3_NO_STDIO
ifeq ($(CROSS),nxdk)
    CPPFLAGS.dir.minimp3 += -DMINIMP3_NO_SIMD
endif

CPPFLAGS.dir.schrift := 
ifeq ($(CROSS),nxdk)
    CPPFLAGS.dir.schrift += -DSCHRIFT_NO_FILE_MAPPING
endif

CPPFLAGS.lib.discord_game_sdk := 
LDLIBS.lib.discord_game_sdk := 
ifdef USE_DISCORD_GAME_SDK
    CPPFLAGS.lib.discord_game_sdk += -DUSE_DISCORD_GAME_SDK
    LDLIBS.lib.discord_game_sdk += -l:$(call so,discord_game_sdk)
endif

CPPFLAGS.lib.SDL2 := 
LDLIBS.lib.SDL2 := 
ifeq ($(CROSS),win32)
    CPPFLAGS.lib.SDL2 += -DSDL_MAIN_HANDLED
    LDLIBS.lib.SDL2 += -l:libSDL2.a -lole32 -loleaut32 -limm32 -lsetupapi -lversion -lgdi32 -lwinmm
else ifeq ($(CROSS),nxdk)
else
    LDLIBS.lib.SDL2 += -lSDL2
endif

LDLIBS.lib.zlib := 
ifeq ($(CROSS),win32)
    LDLIBS.lib.zlib += -l:libz.a
else ifeq ($(CROSS),nxdk)
else
    LDLIBS.lib.zlib += -lz
endif

CPPFLAGS.dir.psrc_engine = $(CPPFLAGS.dir.stb) $(CPPFLAGS.dir.minimp3) $(CPPFLAGS.dir.schrift)
CPPFLAGS.dir.psrc_engine += $(CPPFLAGS.lib.SDL2) $(CPPFLAGS.lib.discord_game_sdk)
LDLIBS.dir.psrc_engine = $(LDLIBS.dir.psrc_utils)
LDLIBS.dir.psrc_engine += $(LDLIBS.lib.discord_game_sdk) $(LDLIBS.lib.SDL2) $(LDLIBS.lib.zlib)

ifeq ($(MODULE),engine)
    _CPPFLAGS += -DMODULE_ENGINE
    _WRFLAGS += -DMODULE_ENGINE
    _CPPFLAGS += $(CPPFLAGS.dir.psrc_engine)
    _LDLIBS +=  $(LDLIBS.dir.psrc_engine)
else ifeq ($(MODULE),server)
    _CPPFLAGS += -DMODULE_SERVER
    _WRFLAGS += -DMODULE_SERVER
    _CPPFLAGS += $(CPPFLAGS.lib.discord_game_sdk)
    _LDLIBS += $(LDLIBS.dir.psrc_utils) $(LDLIBS.lib.discord_game_sdk) $(LDLIBS.lib.zlib)
else ifeq ($(MODULE),editor)
    _CPPFLAGS += -DMODULE_EDITOR
    _WRFLAGS += -DMODULE_EDITOR
    _CPPFLAGS += $(CPPFLAGS.dir.psrc_engine)
    _LDLIBS += $(LDLIBS.dir.psrc_engine)
else ifeq ($(MODULE),toolbox)
    _CPPFLAGS += -DMODULE_TOOLBOX
    _WRFLAGS += -DMODULE_TOOLBOX
    _LDLIBS += $(LDLIBS.dir.psrc_utils) $(LDLIBS.lib.zlib)
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
else ifeq ($(MODULE),toolbox)
BIN := psrc-toolbox
else
BIN := psrc
endif
BINPATH := $(BIN)
ifdef DEBUG
    BINPATH := $(BINPATH).debug
    ifdef ASAN
        BINPATH := $(BINPATH).asan
    endif
endif
ifeq ($(CROSS),win32)
    BINPATH := $(BINPATH).exe
endif
ifeq ($(CROSS),nxdk)
    BINPATH := $(BINPATH).exe
endif
BINPATH := $(OUTDIR)/$(BINPATH)

ifeq ($(CROSS),win32)
    ifneq ($(WINDRES),)
        WRSRC := $(SRCDIR)/psrc/winver.rc
        WROBJ := $(_OBJDIR)/psrc/winver.o
        _WROBJ = $(shell test -f $(WROBJ) && echo $(WROBJ))
    endif
endif

ifeq ($(CROSS),nxdk)
TARGET = $(XISO)
else
TARGET = $(BINPATH)
endif

else

TARGET = $(BINPATH)

endif

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(_OBJDIR)/%.o,$(SOURCES))

export SHCMD

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
'$(dir $(1))$(notdir $(1))'
endef
else
define exec
$(EMULATOR) '$(1)'
endef
endif

define nop
true
endef
ifndef OS
define null
/dev/null
endef
else
define null
NUL
endef
endif

.SECONDEXPANSION:

define a
$(_OBJDIR)/$(1).a
endef
define inc
$$(patsubst $(null)\:,,$$(patsubst $(null),,$$(wildcard $$(shell $(CC) $(_CFLAGS) $(_CPPFLAGS) -x c -MM $(null) $$(wildcard $(1)) -MT $(null)))))
endef

default: target

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

a.dir.psrc_editor = $(call a,psrc/editor) $(a.dir.psrc_toolbox) $(call a,psrc/game) $(call a,psrc/utils)

a.dir.psrc_editormain = $(call a,psrc/editormain) $(a.dir.psrc_editor) $(a.dir.psrc_engine) $(call a,psrc/game) $(call a,psrc/utils) $(call a,psrc)

a.dir.psrc_engine = $(call a,psrc/engine)
ifneq ($(CROSS),nxdk)
    a.dir.psrc_engine += $(call a,glad)
endif
a.dir.psrc_engine += $(call a,psrc/game) $(call a,stb) $(call a,minimp3) $(call a,schrift) $(call a,psrc/utils)

a.dir.psrc_enginemain = $(call a,psrc/enginemain) $(a.dir.psrc_engine) $(a.dir.psrc_server) $(call a,psrc/game) $(call a,psrc/utils) $(call a,psrc)

a.dir.psrc_server = $(call a,psrc/server) $(call a,psrc/game) $(call a,psrc/utils)

a.dir.psrc_servermain = $(call a,psrc/servermain) $(a.dir.psrc_server) $(call a,psrc/game) $(call a,psrc/utils) $(call a,psrc)

a.dir.psrc_toolbox = $(call a,psrc/toolbox) $(call a,tinyobj) $(call a,psrc/utils)

a.dir.psrc_toolboxmain = $(call a,psrc/toolboxmain) $(a.dir.psrc_toolbox) $(call a,psrc/utils) $(call a,psrc)

ifeq ($(MODULE),engine)
a.list = $(a.dir.psrc_enginemain)
else ifeq ($(MODULE),server)
a.list = $(a.dir.psrc_servermain)
else ifeq ($(MODULE),editor)
a.list = $(a.dir.psrc_editormain)
else ifeq ($(MODULE),toolbox)
a.list = $(a.dir.psrc_toolboxmain)
endif

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
	@objcopy --long-section-names=enable --update-section 'XTIMAGE=$(XBE_XTIMAGE)' $@ || exit 0
endif
	@objcopy --long-section-names=enable --rename-section 'XTIMAGE=$$$$XTIMAGE' --rename-section 'XSIMAGE=$$$$XSIMAGE' $@ || exit 0
endif
	@echo Linked $(notdir $@)

else

$(BINPATH): $(OBJECTS) | $(OUTDIR)
	@echo Building $(notdir $@)...
	@$(AR) rcs $@ $^
	@echo Built $(notdir $@)

endif

target: $(TARGET)
	@$(nop)

run: $(TARGET)
	@echo Running $(notdir $(TARGET))...
	@$(call exec,$(TARGET))

clean:
ifeq ($(CROSS),nxdk)
	@echo Cleaning NXDK...
	@'$(MAKE)' --no-print-directory -C '$(NXDK_DIR)' ${MKENV.NXDK} clean
	@$(call rm,$(XISODIR)/default.xbe)
	@$(call rm,$(BINPATH))
endif
	@$(call rmdir,$(_OBJDIR))
	@$(call rm,$(TARGET))

.PHONY: clean target run

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

endif
