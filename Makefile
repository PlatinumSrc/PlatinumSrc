ifndef MKSUB

MODULE ?= engine
CROSS ?= 
SRCDIR ?= src
OBJDIR ?= obj
LIBDIR ?= external/lib
INCDIR ?= external/inc
OUTDIR ?= .

ifndef OS
    ifeq ($(CROSS),)
        CC ?= gcc
        LD = $(CC)
        AR ?= ar
        STRIP ?= strip
        WINDRES ?= true
        ifndef M32
            PLATFORM := $(subst $() $(),_,$(subst /,_,$(shell uname -s)_$(shell uname -m)))
        else
            PLATFORM := $(subst $() $(),_,$(subst /,_,$(shell i386 uname -s)_$(shell i386 uname -m)))
        endif
        SOSUF = .so
    else ifeq ($(CROSS),win32)
        ifndef M32
            CC = x86_64-w64-mingw32-gcc
            LD = $(CC)
            AR = x86_64-w64-mingw32-ar
            STRIP = x86_64-w64-mingw32-strip
            WINDRES = x86_64-w64-mingw32-windres
            PLATFORM := Windows_x86_64
        else
            CC = i686-w64-mingw32-gcc
            LD = $(CC)
            AR = i686-w64-mingw32-ar
            STRIP = i686-w64-mingw32-strip
            WINDRES = i686-w64-mingw32-windres
            PLATFORM := Windows_i686
        endif
        SOSUF = .dll
    else ifeq ($(CROSS),xbox)
        ifndef NXDK_DIR
            .PHONY: error
            error:
	            @echo Please define the NXDK_DIR environment variable
	            @exit 1
        endif
        ifneq ($(MODULE),engine)
            .PHONY: error
            error:
	            @echo Invalid module: $(MODULE)
	            @exit 1
        endif

        CC ?= gcc
        LD = $(CC)
        AR ?= ar
        STRIP ?= strip
        WINDRES ?= true
        SYSCC := $(CC)
        SYSLD := $(LD)
        CC := nxdk-cc
        LD := nxdk-link

        CXBE = $(NXDK_DIR)/tools/cxbe/cxbe
        EXTRACT_XISO = $(NXDK_DIR)/tools/extract-xiso/build/extract-xiso

        PLATFORM := Xbox

        XBE_TITLE := PlatinumSrc
        XBE_TITLEID := PQ-001
        XBE_VERSION := $(shell grep '#define PSRC_BUILD ' src/version.h | sed 's/#define .* //')
        XBE_XTIMAGE := icons/engine.xpr

        XISO ?= $(XBE_TITLE).xiso.iso
        XISODIR ?= $(OUTDIR)/xiso

        XBOX.MKENV := $(XBOX.MKENV) NXDK_ONLY=y NXDK_SDL=y LIB="nxdk-lib -llvmlibempty"
        XBOX.MKENV := $(XBOX.MKENV) LIBSDLIMAGE_SRCS="" LIBSDLIMAGE_OBJS=""
        XBOX.MKENV := $(XBOX.MKENV) FREETYPE_SRCS="" FREETYPE_OBJS=""
        XBOX.MKENV := $(XBOX.MKENV) SDL_TTF_SRCS="" SDL_TTF_OBJS=""
        XBOX.MKENV := $(XBOX.MKENV) SDL2TEST_SRCS="" SDL2TEST_OBJS=""
        XBOX.MKENV := $(XBOX.MKENV) LIBCXX_SRCS="" LIBCXX_OBJS=""
        XBOX.MKENV := $(XBOX.MKENV) LIBPNG_SRCS="" LIBPNG_OBJS=""
        XBOX.MKENV := $(XBOX.MKENV) LIBJPEG_TURBO_OBJS="" LIBJPEG_TURBO_SRCS=""

        NOSTRIP = y

        _default: default
	        @$(nop)
    else
        .PHONY: error
        error:
	        @echo Invalid cross-compilation target: $(CROSS)
	        @exit 1
    endif
    SHCMD = unix
else
    CC = gcc
    LD = $(CC)
    AR = ar
    STRIP = strip
    WINDRES = windres
    CROSS = win32
    ifndef M32
        PLATFORM := Windows_x86_64
    else
        PLATFORM := Windows_i686
    endif
    SOSUF = .dll
    ifdef MSYS2
        SHCMD = unix
    else
        SHCMD = win32
    endif
    NOLTO = y
endif

ifeq ($(MODULE),engine)
else ifeq ($(MODULE),server)
else ifeq ($(MODULE),editor)
else ifeq ($(MODULE),toolbox)
else
    .PHONY: error
    error:
	    @echo Invalid module: $(MODULE)
	    @exit 1
endif

PLATFORMDIRNAME := $(MODULE)
ifdef USE_DISCORD_GAME_SDK
    PLATFORMDIRNAME := $(PLATFORMDIRNAME)_discordgsdk
endif
ifdef ASAN
    PLATFORMDIRNAME := $(PLATFORMDIRNAME)_asan
endif
ifndef DEBUG
    PLATFORMDIR := release/$(PLATFORM)
else
    PLATFORMDIR := debug/$(PLATFORM)
endif
PLATFORMDIR := $(PLATFORMDIR)/$(PLATFORMDIRNAME)
OBJDIR := $(OBJDIR)/$(PLATFORMDIR)

CFLAGS := $(CFLAGS) -I$(INCDIR)/$(PLATFORM) -I$(INCDIR) -Wall -Wextra -Wuninitialized
CPPFLAGS := $(CPPFLAGS) -D_DEFAULT_SOURCE -D_GNU_SOURCE -DMODULE=$(MODULE)
LDFLAGS := $(LDFLAGS)
LDLIBS := $(LDLIBS)
ifeq ($(CROSS),)
    LDLIBS := $(LDLIBS) -lpthread
else ifeq ($(CROSS),win32)
    LDFLAGS := $(LDFLAGS) -static
    LDLIBS := $(LDLIBS) -l:libwinpthread.a -lwinmm
else ifeq ($(CROSS),xbox)
    CPPFLAGS := -DSTBI_NO_SIMD -DPB_HAL_FONT
endif
ifneq ($(CROSS),xbox)
    CFLAGS := $(CFLAGS) -pthread -ffast-math
    LDFLAGS := $(LDFLAGS) -L$(LIBDIR)/$(PLATFORM) -L$(LIBDIR)
    LDLIBS := $(LDLIBS) -lm
    ifdef DEBUG
        LDFLAGS := $(LDFLAGS) -Wl,-R$(LIBDIR)/$(PLATFORM) -Wl,-R$(LIBDIR)
    endif
endif

define so
$(1)$(SOSUF)
endef

ifeq ($(CROSS),win32)
    CPPFLAGS.psrc_editormain := $(CPPFLAGS.psrc_editormain) -DSDL_MAIN_HANDLED
endif
ifdef USE_DISCORD_GAME_SDK
    CPPFLAGS.psrc_editormain := $(CPPFLAGS.psrc_editormain) -DUSE_DISCORD_GAME_SDK
    LDLIBS.psrc_editormain := $(LDLIBS.psrc_editormain) -l:$(call so,discord_game_sdk)
endif

ifeq ($(CROSS),)
    LDLIBS.psrc_engine := $(LDLIBS.psrc_engine) -lSDL2 -lvorbisfile
else ifeq ($(CROSS),win32)
    LDLIBS.psrc_engine := $(LDLIBS.psrc_engine) -l:libSDL2.a -l:libvorbisfile.a
    LDLIBS.psrc_engine := $(LDLIBS.psrc_engine) -lole32 -loleaut32 -limm32 -lsetupapi -lversion -lgdi32 -lwinmm
endif

ifeq ($(CROSS),win32)
    CPPFLAGS.psrc_enginemain := $(CPPFLAGS.psrc_enginemain) -DSDL_MAIN_HANDLED
endif
ifdef USE_DISCORD_GAME_SDK
    CPPFLAGS.psrc_enginemain := $(CPPFLAGS.psrc_enginemain) -DUSE_DISCORD_GAME_SDK
    LDLIBS.psrc_enginemain := $(LDLIBS.psrc_enginemain) -l:$(call so,discord_game_sdk)
endif

ifeq ($(CROSS),win32)
    LDLIBS.psrc_server := $(LDLIBS.psrc_server) -lws2_32
endif

ifeq ($(MODULE),engine)
    CPPFLAGS := $(CPPFLAGS) -DMODULE_ENGINE
    WRFLAGS := $(WRFLAGS) -DMODULE_ENGINE
    CPPFLAGS := $(CPPFLAGS) $(CPPFLAGS.psrc_enginemain)
    LDLIBS := $(LDLIBS) $(LDLIBS.psrc_enginemain) $(LDLIBS.psrc_engine) $(LDLIBS.psrc_server)
else ifeq ($(MODULE),server)
    CPPFLAGS := $(CPPFLAGS) -DMODULE_SERVER
    WRFLAGS := $(WRFLAGS) -DMODULE_SERVER
    LDLIBS := $(LDLIBS) $(LDLIBS.psrc_server)
else ifeq ($(MODULE),editor)
    CPPFLAGS := $(CPPFLAGS) -DMODULE_EDITOR
    WRFLAGS := $(WRFLAGS) -DMODULE_EDITOR
    CPPFLAGS := $(CPPFLAGS) $(CPPFLAGS.psrc_editormain)
    LDLIBS := $(LDLIBS) $(LDLIBS.psrc_editormain) $(LDLIBS.psrc_editor) $(LDLIBS.psrc_engine) $(LDLIBS.psrc_server)
else ifeq ($(MODULE),toolbox)
    CPPFLAGS := $(CPPFLAGS) -DMODULE_TOOLBOX
    WRFLAGS := $(WRFLAGS) -DMODULE_TOOLBOX
endif

ifdef DEBUG
    CFLAGS := $(CFLAGS) -Og -g
    CPPFLAGS := $(CPPFLAGS) -DDBGLVL=$(DEBUG)
    ifeq ($(CROSS),win32)
        WRFLAGS := $(WRFLAGS) -DDBGLVL=$(DEBUG)
    endif
    ifneq ($(CROSS),xbox)
        LDFLAGS := $(LDFLAGS) -debug
        XBOX.MKENV := $(XBOX.MKENV) DEBUG=y
    endif
    NOSTRIP = y
    ifdef ASAN
        CFLAGS := $(CFLAGS) -fsanitize=address
        LDFLAGS := $(LDFLAGS) -fsanitize=address
    endif
else
    ifndef O
        O = 2
    endif
    CFLAGS := $(CFLAGS) -O$(O) -fno-exceptions
    ifndef NOLTO
        CFLAGS := $(CFLAGS) -flto=auto
        LDFLAGS := $(LDFLAGS) -flto=auto
        ifneq ($(CROSS),xbox)
            XBOX.MKENV := $(XBOX.MKENV) LTO=y
        endif
    endif
endif
ifneq ($(CROSS),xbox)
    ifdef NATIVE
        CFLAGS := $(CFLAGS) -march=native -mtune=native
    endif
    ifdef M32
        CPPFLAGS := $(CPPFLAGS) -DM32
        ifeq ($(CROSS),win32)
            WRFLAGS := $(WRFLAGS) -DM32
        endif
    endif
endif

ifeq ($(CROSS),xbox)

_CFLAGS := $(filter-out -Wuninitialized,$(filter-out -Wextra,$(filter-out -Wall,$(_CFLAGS) $(CPPFLAGS))))
_LDFLAGS := $(LDFLAGS)

include $(NXDK_DIR)/lib/Makefile
include $(NXDK_DIR)/lib/net/Makefile
include $(NXDK_DIR)/lib/sdl/SDL2/Makefile.xbox
include $(NXDK_DIR)/lib/sdl/Makefile

CFLAGS := $(CFLAGS) $(NXDK_CFLAGS)
LDFLAGS := $(LDFLAGS) $(NXDK_LDFLAGS)

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
ifeq ($(CROSS),xbox)
    BINPATH := $(BINPATH).exe
endif
BINPATH := $(OUTDIR)/$(BINPATH)

ifeq ($(CROSS),win32)
    ifneq ($(WINDRES),)
        WRSRC := $(SRCDIR)/winver.rc
        WROBJ := $(OBJDIR)/winver.o
    endif
endif

endif

SOURCES := $(wildcard $(SRCDIR)/*.c)
DEPENDS := $(wildcard $(SRCDIR)/*.h)
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

export SHCMD

export MODULE
export CROSS

export CC
export AR

export CFLAGS
export CPPFLAGS

export SRCDIR
export OBJDIR
export OUTDIR
export PLATFORM
export PLATFORMDIR

ifeq ($(SHCMD),win32)
define mkpath
$(subst /,\,$(1))
endef
endif

ifeq ($(SHCMD),unix)
define mkdir
if [ ! -d '$(1)' ]; then echo 'Creating $(1)...'; mkdir -p '$(1)'; fi; true
endef
define rm
if [ -f '$(1)' ]; then echo 'Removing $(1)...'; rm -f '$(1)'; fi; true
endef
define rmdir
if [ -d '$(1)' ]; then echo 'Removing $(1)...'; rm -rf '$(1)'; fi; true
endef
define run
./'$(1)'
endef
else ifeq ($(SHCMD),win32)
define mkdir
if not exist "$(call mkpath,$(1))" echo Creating $(1)... & md "$(call mkpath,$(1))"
endef
define rm
if exist "$(call mkpath,$(1))" echo Removing $(1)... & del /Q "$(call mkpath,$(1))"
endef
define rmdir
if exist "$(call mkpath,$(1))" echo Removing $(1)... & rmdir /S /Q "$(call mkpath,$(1))"
endef
define run
.\\$(1)
endef
endif

ifeq ($(SHCMD),unix)
define nop
echo -n > /dev/null
endef
define null
/dev/null
endef
else ifeq ($(SHCMD),win32)
define nop
echo. > NUL
endef
define null
NUL
endef
endif
ifndef inc.null
define inc.null
$(null)
endef
endif

.SECONDEXPANSION:

define a
$(OBJDIR)/lib$(1).a
endef
define inc
$$(patsubst noexist\:,,$$(patsubst $(inc.null),,$$(wildcard $$(shell $(CC) $(CFLAGS) $(CPPFLAGS) -x c -MM $(inc.null) $$(wildcard $(1)) -MT noexist))))
endef

default: bin

ifndef MKSUB
$(OBJDIR)/lib%.a: $$(wildcard $(SRCDIR)/$(notdir %)/*.c) $(call inc,$(SRCDIR)/$(notdir %)/*.c)
	@$(MAKE) --no-print-directory MKSUB=y SRCDIR=$(SRCDIR)/$(notdir $*) OBJDIR=$(OBJDIR)/$(notdir $*) OUTDIR=$(OBJDIR) BINPATH=$@
endif

ifdef MKSUB
$(OUTDIR):
	@$(call mkdir,$@)
endif

$(OBJDIR):
	@$(call mkdir,$@)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(call inc,$(SRCDIR)/%.c) | $(OBJDIR) $(OUTDIR)
	@echo Compiling $(notdir $<)...
	@$(CC) $(CFLAGS) $(CPPFLAGS) $< -c -o $@
	@echo Compiled $(notdir $<)

ifndef MKSUB

a.dir.psrc_editor = $(call a,psrc_editor) $(call a,psrc_aux) $(a.dir.psrc_toolbox)
a.dir.psrc_editormain = $(call a,psrc_editormain) $(call a,psrc_aux) $(a.dir.psrc_editor) $(a.dir.psrc_engine)

a.dir.psrc_engine = $(call a,psrc_engine) $(call a,psrc_aux) $(call a,stb)
ifneq ($(CROSS),xbox)
    a.dir.psrc_engine += $(call a,glad)
endif
a.dir.psrc_enginemain = $(call a,psrc_enginemain) $(call a,psrc_aux) $(a.dir.psrc_engine) $(a.dir.psrc_server)

a.dir.psrc_server = $(call a,psrc_server) $(call a,psrc_aux)
a.dir.psrc_servermain = $(call a,psrc_servermain) $(call a,psrc_aux) $(a.dir.psrc_server)

a.dir.psrc_toolbox = $(call a,psrc_toolbox) $(call a,psrc_aux) $(call a,tinyobj)
a.dir.psrc_toolboxmain = $(call a,psrc_toolboxmain) $(call a,psrc_aux) $(a.dir.psrc_toolbox)

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
ifeq ($(CROSS),xbox)
	@echo Making NXDK libs...
	@CFLAGS="$(_CFLAGS)"; LDFLAGS="$(_LDFLAGS)"; $(MAKE) --no-print-directory -C $(NXDK_DIR) ${XBOX.MKENV} main.exe $(NXDK_DIR)/lib/xboxkrnl/libxboxkrnl.lib > $(null)
	@echo Made NXDK libs
endif
	@echo Linking $(notdir $@)...
ifeq ($(CROSS),win32)
ifneq ($(WINDRES),)
	@$(WINDRES) $(WRFLAGS) $(WRSRC) -o $(WROBJ)
endif
endif
ifneq ($(CROSS),xbox)
	@$(LD) $(LDFLAGS) $^ $(WROBJ) $(LDLIBS) -o $@
ifndef NOSTRIP
	@$(STRIP) -s -R ".comment" -R ".note.*" -R ".gnu.build-id" $@
endif
else
	@$(LD) $(LDFLAGS) $^ $(NXDK_DIR)/lib/*.lib $(NXDK_DIR)/lib/xboxkrnl/libxboxkrnl.lib $(WROBJ) $(LDLIBS) -out:$@ > $(null)
	@objcopy --long-section-names=enable --rename-section 'XTIMAGE=$$$$XTIMAGE' --rename-section 'XSIMAGE=$$$$XSIMAGE' $@ || exit 0
endif
	@echo Linked $(notdir $@)

else

$(BINPATH): $(OBJECTS) | $(OUTDIR)
	@echo Building $(notdir $@)...
	@$(AR) rcs $@ $^
	@echo Built $(notdir $@)

endif

ifndef MKSUB
ifneq ($(CROSS),xbox)
bin: $(BINPATH)
else
bin: $(XISO)
endif
else
bin: $(BINPATH)
endif
	@$(nop)

run: bin
	@echo Running $(notdir $(BINPATH))...
	@$(call run,$(BINPATH))

clean:
ifeq ($(CROSS),xbox)
	@$(MAKE) --no-print-directory -C $(NXDK_DIR) ${XBOX.MKENV} clean
	@$(call rm,$(XISODIR)/default.xbe)
endif
	@$(call rmdir,$(OBJDIR))
	@$(call rm,$(BINPATH))

.PHONY: clean bin run

ifeq ($(CROSS),xbox)

$(XISODIR):
	@$(call mkdir,$@)

$(CXBE):
	@echo Making NXDK Cxbe tool...
	@CC=$(SYSCC); LD=$(SYSLD); unset CFLAGS; unset CPPFLAGS; unset LDFLAGS; unset LDLIBS; $(MAKE) --no-print-directory -C $(NXDK_DIR) cxbe > $(null)

$(EXTRACT_XISO):
	@echo Making NXDK extract-xiso tool...
	@CC=$(SYSCC); LD=$(SYSLD); unset CFLAGS; unset CPPFLAGS; unset LDFLAGS; unset LDLIBS; $(MAKE) --no-print-directory -C $(NXDK_DIR) extract-xiso > $(null)

$(XISODIR)/default.xbe: $(BINPATH) | $(XISODIR)
	@echo Relinking $@ from $<...
	@$(CXBE) -OUT:$@ -TITLE:$(XBE_TITLE) -TITLEID:$(XBE_TITLEID) -VERSION:$(XBE_VERSION) $< > $(null)

$(XISO): $(XISODIR)/default.xbe $(EXTRACT_XISO) | $(XISODIR)
	@echo Creating $@...
	@$(EXTRACT_XISO) -c $(XISODIR) "$@" > $(null)

endif
