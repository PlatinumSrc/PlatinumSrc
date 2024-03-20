null := /dev/null

ifneq ($(MKSUB),y)

MODULE := engine
SRCDIR := src
OBJDIR := obj
EXTDIR := external
OUTDIR := .

ifeq ($(OS),Windows_NT)
    CROSS := win32
endif

ifeq ($(CROSS),)
    CC ?= gcc
    CC := $(PREFIX)$(CC)
    LD := $(CC)
    AR ?= ar
    AR := $(PREFIX)$(AR)
    STRIP ?= strip
    STRIP := $(PREFIX)$(STRIP)
    ifneq ($(M32),y)
        PLATFORM := $(subst $() $(),_,$(subst /,_,$(shell uname -o 2> $(null) || uname -s; uname -m)))
    else
        PLATFORM := $(subst $() $(),_,$(subst /,_,$(shell i386 uname -o 2> $(null) || i386 uname -s; i386 uname -m)))
    endif
    USEGL := y
else ifeq ($(CROSS),win32)
    ifneq ($(M32),y)
        ifneq ($(OS),Windows_NT)
            PREFIX := x86_64-w64-mingw32-
        endif
        PLATFORM := Windows_x86_64
    else
        ifneq ($(OS),Windows_NT)
            PREFIX := i686-w64-mingw32-
        endif
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
    USEGL := y
else ifeq ($(CROSS),emscr)
    ifeq ($(MODULE),engine)
    else ifneq ($(MODULE),editor)
        $(error Invalid module: $(MODULE))
    endif
    PLATFORM := Emscripten
    CC := emcc
    LD := $(CC)
    AR := emar
    EMULATOR := emrun
    EMUPATHFLAG := --
    NOSTRIP := y
    NOMT := y
    USEGL11 := y
    USELIBGL := y
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
    PLATFORM := NXDK
    _LD := $(PREFIX)$(_LD)
    CC := nxdk-cc
    LD := nxdk-link
    AR ?= ar
    AR := $(PREFIX)$(AR)
    OBJCOPY ?= objcopy
    OBJCOPY := $(PREFIX)$(OBJCOPY)
    EMULATOR := xemu
    EMUPATHFLAG := -dvd_path
    CXBE := $(NXDK_DIR)/tools/cxbe/cxbe
    EXTRACT_XISO := $(NXDK_DIR)/tools/extract-xiso/build/extract-xiso
    XBE_TITLE := PlatinumSrc
    XBE_TITLEID := PQ-001
    XBE_VERSION := $(shell grep '#define PSRC_BUILD ' $(SRCDIR)/psrc/version.h | sed 's/#define .* //')
    XBE_XTIMAGE := icons/engine.xpr
    XISO := $(OUTDIR)/$(XBE_TITLE).xiso.iso
    XISODIR := $(OUTDIR)/xiso
    NOLTO := y
    NOSIMD := y
    USESTDTHREAD := y
    USEGL11 := y
    USELIBGL := y
    USEXGU := y
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
else ifeq ($(CROSS),ps2)
    ifndef PS2DEV
        $(error Please define the PS2DEV environment variable)
    endif
    ifndef PS2SDK
        $(error Please define the PS2SDK environment variable)
    endif
    ifndef GSKIT
        $(error Please define the GSKIT environment variable)
    endif
    PREFIX := mips64r5900el-ps2-elf-
    CC ?= gcc
    CC := $(PREFIX)$(CC)
    LD := $(CC)
    AR ?= ar
    AR := $(PREFIX)$(AR)
    STRIP ?= strip
    STRIP := $(PREFIX)$(STRIP)
    EMULATOR := pcsx2
    EMUPATHFLAG := --
    USEGSKIT := y
else ifeq ($(CROSS),dc)
    ifndef KOS_BASE
        $(error Please run KallistiOS' environment.sh)
    endif
    PLATFORM := Dreamcast
    CC := $(KOS_CC)
    LD := $(CC)
    AR := $(KOS_AR)
    STRIP := $(KOS_STRIP)
    OBJCOPY := $(KOS_OBJCOPY)
    EMULATOR := flycast
    SCRAMBLE := $(KOS_BASE)/utils/scramble/scramble
    MAKEIP := $(KOS_BASE)/utils/makeip/makeip
    MKISOFS := mkisofs
    CDI4DC := $(KOS_BASE)/utils/img4dc/cdi4dc/cdi4dc
    IP_TITLE := PlatinumSrc
    IP_COMPANY := PQCraft
    IP_MRIMAGE := icons/engine.mr
    CDI := $(OUTDIR)/$(IP_TITLE).cdi
    CDIDIR := $(OUTDIR)/cdi
    USESTDTHREAD := y
    USEGL11 := y
    USELIBGL := y
    USESDL1 := y
else
    $(error Invalid cross-compilation target: $(CROSS))
endif

ifeq ($(MODULE),engine)
    USEMINIMP3 := y
else ifeq ($(MODULE),server)
else ifeq ($(MODULE),editor)
    USEMINIMP3 := y
else
    $(error Invalid module: $(MODULE))
endif

PLATFORMDIRNAME := $(MODULE)
ifeq ($(USEDISCORDGAMESDK),y)
    PLATFORMDIRNAME := $(PLATFORMDIRNAME)_discordgsdk
endif
ifndef DEBUG
    PLATFORMDIR := release/$(PLATFORM)
else
    ifeq ($(ASAN),y)
        PLATFORMDIRNAME := $(PLATFORMDIRNAME)_asan
    endif
    PLATFORMDIR := debug/$(PLATFORM)
endif
PLATFORMDIR := $(PLATFORMDIR)/$(PLATFORMDIRNAME)
_OBJDIR := $(OBJDIR)/$(PLATFORMDIR)

ifeq ($(CROSS),win32)
    SOSUF := .dll
else ifneq ($(CROSS),nxdk)
    SOSUF := .so
endif

ifeq ($(USEGL),y)
    USEGL11 := y
    USEGL33 := y
    USEGLES30 := y
else ifeq ($(USEGL11),y)
    USEGL := y
else ifeq ($(USEGL33),y)
    USEGL := y
else ifeq ($(USEGLES30),y)
    USEGL := y
endif

_CFLAGS := $(CFLAGS) -I$(EXTDIR)/$(PLATFORM)/include -I$(EXTDIR)/include -fno-exceptions -Wall -Wextra -Wuninitialized
_CPPFLAGS := $(CPPFLAGS)
_LDFLAGS := $(LDFLAGS) -L$(EXTDIR)/$(PLATFORM)/lib -L$(EXTDIR)/lib
_LDLIBS := $(LDLIBS)
_WRFLAGS := $(WRFLAGS)
ifneq ($(CROSS),nxdk)
    _CFLAGS += -Wundef -fvisibility=hidden
    ifneq ($(NOFASTMATH),y)
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
        ifeq ($(USEGL),y)
            _LDFLAGS += -sLEGACY_GL_EMULATION -sGL_UNSAFE_OPTS=0
        endif
        _LDFLAGS += --embed-file common/ --embed-file engine/ --embed-file games/ --embed-file mods/
    endif
    ifeq ($(USEGL),y)
        ifeq ($(USELIBGL),y)
            _LDLIBS += -lGL
        endif
    endif
    ifneq ($(NOMT),y)
        ifneq ($(USESTDTHREAD),y)
            ifneq ($(CROSS),win32)
                ifneq ($(CROSS),dc)
                    _CFLAGS += -pthread
                endif
                _LDLIBS += -lpthread
            else
                ifeq ($(USEWINPTHREAD),y)
                    _CFLAGS += -pthread
                    _CPPFLAGS += -DPSRC_USEWINPTHREAD
                    _LDLIBS += -l:libwinpthread.a
                endif
            endif
        endif
    endif
    ifneq ($(CROSS),emscr)
        ifeq ($(M32),y)
            _CFLAGS += -m32
            _CPPFLAGS += -DM32
            _LDFLAGS += -m32
            ifeq ($(CROSS),win32)
                _WRFLAGS += -DM32
            endif
        endif
        ifdef DEBUG
            _LDFLAGS += -Wl,-R$(EXTDIR)/$(PLATFORM)/lib -Wl,-R$(EXTDIR)/lib
        endif
        ifeq ($(NATIVE),y)
            _CFLAGS += -march=native -mtune=native
        endif
    endif
else
    _CPPFLAGS += -DPB_HAL_FONT
endif
ifeq ($(NOSIMD),y)
    _CPPFLAGS += -DPSRC_NOSIMD
endif
ifeq ($(NOMT),y)
    _CPPFLAGS += -DPSRC_NOMT
endif
ifeq ($(USEMINIMP3),y)
    _CPPFLAGS += -DPSRC_USEMINIMP3
endif
ifeq ($(USEGL),y)
    _CPPFLAGS += -DPSRC_USEGL
endif
ifeq ($(USEGL11),y)
    _CPPFLAGS += -DPSRC_USEGL11
endif
ifeq ($(USEGL33),y)
    _CPPFLAGS += -DPSRC_USEGL33
endif
ifeq ($(USEGLES30),y)
    _CPPFLAGS += -DPSRC_USEGLES30
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
    ifneq ($(NOLTO),y)
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
        ifndef O
            O := g
        endif
        _CFLAGS += -O$(O) -g
    else
        _CFLAGS += -g -gdwarf-4
        _LDFLAGS += -debug
        MKENV.NXDK := $(MKENV.NXDK) DEBUG=y
    endif
    ifeq ($(CROSS),win32)
        _WRFLAGS += -DPSRC_DBGLVL=$(DEBUG)
    endif
    NOSTRIP := y
    ifeq ($(ASAN),y)
        _CFLAGS += -fsanitize=address
        _LDFLAGS += -fsanitize=address
    endif
endif

define so
$(1)$(SOSUF)
endef

CPPFLAGS.dir.psrc_common := 
ifeq ($(USESTDTHREAD),y)
    CPPFLAGS.dir.psrc_common += -DPSRC_USESTDTHREAD
endif
LDLIBS.dir.psrc_common := 
ifeq ($(CROSS),win32)
    LDLIBS.dir.psrc_common += -lwinmm
endif

LDLIBS.dir.psrc_server := 
ifeq ($(CROSS),win32)
    LDLIBS.dir.psrc_server += -lws2_32
endif

ifeq ($(USEMINIMP3),y)
    CPPFLAGS.dir.minimp3 := -DMINIMP3_NO_STDIO
    ifeq ($(NOSIMD),y)
        CPPFLAGS.dir.minimp3 += -DMINIMP3_NO_SIMD
    endif
endif

CPPFLAGS.dir.schrift := 
ifeq ($(CROSS),nxdk)
    CPPFLAGS.dir.schrift += -DSCHRIFT_NO_FILE_MAPPING
else ifeq ($(CROSS),dc)
    CPPFLAGS.dir.schrift += -DSCHRIFT_NO_FILE_MAPPING
endif

CPPFLAGS.dir.stb := -DSTBI_ONLY_PNG -DSTBI_ONLY_JPEG -DSTBI_ONLY_TGA -DSTBI_ONLY_BMP
ifeq ($(NOSIMD),y)
    CPPFLAGS.dir.stb += -DSTBI_NO_SIMD
endif
ifeq ($(NOMT),y)
    CPPFLAGS.dir.stb += -DSTBI_NO_THREAD_LOCALS
endif

CPPFLAGS.lib.discord_game_sdk := 
LDLIBS.lib.discord_game_sdk := 
ifeq ($(USEDISCORDGAMESDK),y)
    CPPFLAGS.lib.discord_game_sdk += -DPSRC_USEDISCORDGAMESDK
    LDLIBS.lib.discord_game_sdk += -l:$(call so,discord_game_sdk)
endif

CFLAGS.lib.SDL := 
CPPFLAGS.lib.SDL := 
LDLIBS.lib.SDL := 
ifneq ($(USESDL1),y)
    ifeq ($(CROSS),win32)
        CPPFLAGS.lib.SDL += -DSDL_MAIN_HANDLED
        LDLIBS.lib.SDL += -l:libSDL2.a -lole32 -loleaut32 -limm32 -lsetupapi -lversion -lgdi32 -lwinmm
    else ifeq ($(CROSS),emscr)
        CFLAGS.lib.SDL += -sUSE_SDL=2
        LDLIBS.lib.SDL += -sUSE_SDL=2
    else ifneq ($(CROSS),nxdk)
        LDLIBS.lib.SDL += -lSDL2
    endif
else
    CPPFLAGS.lib.SDL += -DPSRC_USESDL1
    ifeq ($(CROSS),win32)
        CPPFLAGS.lib.SDL += -DSDL_MAIN_HANDLED
        LDLIBS.lib.SDL += -l:libSDL.a -liconv -luser32 -lgdi32 -lwinmm -ldxguid
    else ifeq ($(CROSS),emscr)
        CFLAGS.lib.SDL += -sUSE_SDL
        LDLIBS.lib.SDL += -sUSE_SDL
    else ifneq ($(CROSS),nxdk)
        LDLIBS.lib.SDL += -lSDL
    endif
endif

ifeq ($(MODULE),engine)
    _CPPFLAGS += -DPSRC_MODULE_ENGINE
    _WRFLAGS += -DPSRC_MODULE_ENGINE
    _CFLAGS += $(CFLAGS.lib.SDL)
    _CPPFLAGS += $(CPPFLAGS.dir.stb) $(CPPFLAGS.dir.minimp3) $(CPPFLAGS.dir.schrift) $(CPPFLAGS.dir.psrc_common)
    _CPPFLAGS += $(CPPFLAGS.lib.SDL) $(CPPFLAGS.lib.discord_game_sdk)
    _LDLIBS += $(LDLIBS.dir.psrc_engine) $(LDLIBS.dir.psrc_common)
    _LDLIBS += $(LDLIBS.lib.discord_game_sdk) $(LDLIBS.lib.SDL)
else ifeq ($(MODULE),server)
    _CPPFLAGS += -DPSRC_MODULE_SERVER
    _WRFLAGS += -DPSRC_MODULE_SERVER
    _CPPFLAGS += $(CPPFLAGS.lib.discord_game_sdk)
    _LDLIBS += $(LDLIBS.dir.psrc_common) $(LDLIBS.lib.discord_game_sdk)
else ifeq ($(MODULE),editor)
    _CPPFLAGS += -DPSRC_MODULE_EDITOR
    _WRFLAGS += -DPSRC_MODULE_EDITOR
    _CFLAGS += $(CFLAGS.lib.SDL)
    _CPPFLAGS += $(CPPFLAGS.dir.stb) $(CPPFLAGS.dir.minimp3) $(CPPFLAGS.dir.schrift) $(CPPFLAGS.dir.psrc_common)
    _CPPFLAGS += $(CPPFLAGS.lib.SDL) $(CPPFLAGS.lib.discord_game_sdk)
    _LDLIBS += $(LDLIBS.dir.psrc_engine) $(LDLIBS.dir.psrc_common)
    _LDLIBS += $(LDLIBS.lib.discord_game_sdk) $(LDLIBS.lib.SDL)
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

else ifeq ($(CROSS),dc)
    _CFLAGS += $(KOS_CFLAGS) $(KOS_INC_PATHS)
    _LDFLAGS += $(KOS_LDFLAGS) $(KOS_LIB_PATHS)
    _LDLIBS += $(KOS_START) $(KOS_LIBS)
endif

ifeq ($(MODULE),server)
BIN := psrc-server
else ifeq ($(MODULE),editor)
BIN := psrc-editor
else
BIN := psrc
endif
ifdef DEBUG
    BIN := $(BIN)_debug
    ifeq ($(ASAN),y)
        BIN := $(BIN)_asan
    endif
endif
ifeq ($(CROSS),win32)
    BINPATH := $(BIN).exe
else ifeq ($(CROSS),emscr)
    BINPATH := index.html
else ifeq ($(CROSS),nxdk)
    BINPATH := $(BIN).exe
else ifeq ($(CROSS),dc)
    BINPATH := $(BIN).elf
else
    BINPATH := $(BIN)
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
else ifeq ($(CROSS),dc)
TARGET = $(CDI)
else
TARGET = $(BINPATH)
endif

__SRCDIR := $(SRCDIR)
__OBJDIR := $(_OBJDIR)

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
if [ ! -d '$(1)' ]; then echo 'Creating $(1)/...'; mkdir -p '$(1)'; fi; true
endef
define rm
if [ -f '$(1)' ]; then echo 'Removing $(1)/...'; rm -f '$(1)'; fi; true
endef
define rmdir
if [ -d '$(1)' ]; then echo 'Removing $(1)/...'; rm -rf '$(1)'; fi; true
endef
ifndef EMULATOR
define exec
'$(dir $(1))$(notdir $(1))' $(RUNFLAGS)
endef
else
define exec
$(EMULATOR) $(EMUFLAGS) $(EMUPATHFLAG) '$(dir $(1))$(notdir $(1))' $(RUNFLAGS)
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

ifneq ($(MKSUB),y)
define a_path
$(patsubst $(_OBJDIR)/%,%,$(1))
endef
$(_OBJDIR)/%.a: $$(wildcard $(SRCDIR)/$(call a_path,%)/*.c) $(call inc,$(SRCDIR)/$(call a_path,%)/*.c)
	@'$(MAKE)' --no-print-directory MKSUB=y __SRCDIR=$(__SRCDIR) __OBJDIR=$(__OBJDIR) SRCDIR=$(SRCDIR)/$(call a_path,$*) _OBJDIR=$(_OBJDIR)/$(call a_path,$*) OUTDIR=$(_OBJDIR) BINPATH=$@
endif

$(OUTDIR):
	@$(call mkdir,$@)

$(_OBJDIR):
	@$(call mkdir,$@)

$(_OBJDIR)/%.o: $(SRCDIR)/%.c $(call inc,$(SRCDIR)/%.c) | $(_OBJDIR) $(OUTDIR)
	@echo Compiling $(patsubst $(__SRCDIR)/%,%,$<)...
	@$(CC) $(_CFLAGS) $(_CPPFLAGS) $< -c -o $@
	@echo Compiled $(patsubst $(__SRCDIR)/%,%,$<)

ifneq ($(MKSUB),y)

a.dir.psrc_common = $(call a,psrc/common)
ifneq ($(MODULE),server)
a.dir.psrc_common += $(call a,stb) $(call a,schrift)
ifeq ($(USEMINIMP3),y)
a.dir.psrc_common += $(call a,minimp3)
endif
endif

a.dir.psrc_editor = $(call a,psrc/editor) $(a.dir.psrc_engine)

a.dir.psrc_engine = $(call a,psrc/engine) $(a.dir.psrc_server)
ifeq ($(USEGL),y)
ifneq ($(USELIBGL),y)
a.dir.psrc_engine += $(call a,glad)
endif
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
	@echo Linking $@...
ifeq ($(CROSS),win32)
ifneq ($(WINDRES),)
	@$(WINDRES) $(_WRFLAGS) $(WRSRC) -o $(WROBJ) || exit 0
endif
endif
ifeq ($(CROSS),emscr)
	@$(LD) $(_LDFLAGS) $^ $(_LDLIBS) -o $(OUTDIR)/$(BIN).html
	@mv -f $(OUTDIR)/$(BIN).html $(BINPATH)
else ifeq ($(CROSS),nxdk)
	@$(LD) $(_LDFLAGS) $^ '$(NXDK_DIR)'/lib/*.lib '$(NXDK_DIR)'/lib/xboxkrnl/libxboxkrnl.lib $(_LDLIBS) -out:$@ > $(null)
ifneq ($(XBE_XTIMAGE),)
	@$(OBJCOPY) --long-section-names=enable --update-section 'XTIMAGE=$(XBE_XTIMAGE)' $@ || exit 0
endif
	@$(OBJCOPY) --long-section-names=enable --rename-section 'XTIMAGE=$$$$XTIMAGE' --rename-section 'XSIMAGE=$$$$XSIMAGE' $@ || exit 0
else
	@$(LD) $(_LDFLAGS) -Wl,--whole-archive $^ -Wl,--no-whole-archive $(_WROBJ) $(_LDLIBS) -o $@
ifneq ($(NOSTRIP),y)
	@$(STRIP) -s -R '.comment' -R '.note.*' -R '.gnu.build-id' $@ || exit 0
endif
endif
	@echo Linked $@

else

$(BINPATH): $(OBJECTS) | $(OUTDIR)
	@echo Building $(patsubst $(__OBJDIR)/%,%,$@)...
	@$(AR) rcs $@ $^
	@echo Built $(patsubst $(__OBJDIR)/%,%,$@)

endif

build: $(TARGET)
	@:

run: build
	@echo Running $(TARGET)...
	@$(call exec,$(TARGET))

clean:
	@$(call rmdir,$(_OBJDIR))

distclean: clean
ifeq ($(CROSS),nxdk)
	@$(call rm,$(XISODIR)/default.xbe)
	@$(call rm,$(BINPATH))
else ifeq ($(CROSS),emscr)
	@$(call rm,$(basename $(OUTDIR)/$(BIN)).js)
	@$(call rm,$(basename $(OUTDIR)/$(BIN)).wasm)
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
	@'$(CXBE)' -OUT:$@ -TITLE:'$(XBE_TITLE)' -TITLEID:$(XBE_TITLEID) -VERSION:$(XBE_VERSION) $< > $(null)

$(XISO): $(XISODIR)/default.xbe $(EXTRACT_XISO) | $(XISODIR)
	@echo Creating $@...
	@'$(EXTRACT_XISO)' -c $(XISODIR) "$@" > $(null)

else ifeq ($(CROSS),dc)

$(CDIDIR):
	@$(call mkdir,$@)

$(CDIDIR)/1ST_READ.bin: $(BINPATH) | $(CDIDIR)
	@echo Creating $@ from $<...
	@$(OBJCOPY) -R .stack -O binary $< $<.bin
	@'$(SCRAMBLE)' $<.bin $@
	@$(call rm,$<.bin)

$(OUTDIR)/IP.BIN:
ifneq ($(IP_MRIMAGE),)
	@'$(MAKEIP)' -g '$(IP_TITLE)' -c '$(IP_COMPANY)' -l $(IP_MRIMAGE) $@
else
	@'$(MAKEIP)' -g '$(IP_TITLE)' -c '$(IP_COMPANY)' $@
endif

$(CDI): $(CDIDIR)/1ST_READ.bin $(OUTDIR)/IP.BIN
	@echo Creating $@...
	@$(MKISOFS) -f -C 0,11702 -V '$(IP_TITLE)' -G $(OUTDIR)/IP.BIN -rJl -o $(patsubst %.cdi,%.iso,$@) $(CDIDIR)
	@$(CDI4DC) $(patsubst %.cdi,%.iso,$@) $@
	@$(call rm,$(patsubst %.cdi,%.iso,$@))

endif
