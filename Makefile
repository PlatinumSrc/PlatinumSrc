ifndef MKSUB

MODULE ?= engine
CROSS ?= 
SRCDIR ?= src
OBJDIR ?= obj
LIBDIR ?= obj
OUTDIR ?= .

ifndef OS
    ifeq ($(CROSS),)
        CC ?= gcc
        AR ?= ar
        STRIP ?= strip
        WINDRES ?= true
        ifndef M32
            PLATFORMDIR := $(subst $() $(),_,$(subst /,_,$(shell uname -s)_$(shell uname -m)))
        else
            PLATFORMDIR := $(subst $() $(),_,$(subst /,_,$(shell i386 uname -s)_$(shell i386 uname -m)))
        endif
    else ifeq ($(CROSS),win32)
        ifndef M32
            CC = x86_64-w64-mingw32-gcc
            AR = x86_64-w64-mingw32-ar
            STRIP = x86_64-w64-mingw32-strip
            WINDRES = x86_64-w64-mingw32-windres
            PLATFORMDIR := Windows_x86_64
        else
            CC = i686-w64-mingw32-gcc
            AR = i686-w64-mingw32-ar
            STRIP = i686-w64-mingw32-strip
            WINDRES = i686-w64-mingw32-windres
            PLATFORMDIR := Windows_i686
        endif
    else
        .PHONY: error
        error:
		    @echo Invalid cross-compilation target: $(CROSS)
		    @exit 1
    endif
    SHCMD = unix
else
    CC = gcc
    AR = ar
    STRIP = strip
    WINDRES = windres
    CROSS = win32
    ifdef MSYS2
        SHCMD = unix
    else
        SHCMD = win32
    endif
endif
ifeq ($(MODULE),engine)
else ifeq ($(MODULE),server)
else ifeq ($(MODULE),toolbox)
else
    .PHONY: error
    error:
	    @echo Invalid module: $(MODULE)
	    @exit 1
endif
ifndef DEBUG
    PLATFORMDIR := release/$(PLATFORMDIR)
else
    ifndef ASAN
        PLATFORMDIR := debug/$(PLATFORMDIR)
    else
        PLATFORMDIR := debug.asan/$(PLATFORMDIR)
    endif
endif
PLATFORMDIR := $(PLATFORMDIR)/$(MODULE)
OBJDIR := $(OBJDIR)/$(PLATFORMDIR)
LIBDIR := $(LIBDIR)/$(PLATFORMDIR)

CFLAGS += -Wall -Wextra -Wuninitialized -pthread -ffast-math
CPPFLAGS += -D_DEFAULT_SOURCE -D_GNU_SOURCE -DMODULE=$(MODULE)
LDFLAGS += 
LDLIBS += -lm
ifeq ($(CROSS),win32)
    WRFLAGS += 
endif
ifeq ($(CROSS),)
    LDLIBS += -lpthread
else ifeq ($(CROSS),win32)
    LDLIBS += -l:libwinpthread.a
endif
ifeq ($(MODULE),engine)
    CPPFLAGS += -DMODULE_ENGINE
    CPPFLAGS += -DMA_NO_DEVICE_IO
    ifeq ($(CROSS),)
        LDLIBS += -lSDL2 -lSDL2_mixer -lvorbisfile
    else ifeq ($(CROSS),win32)
        CPPFLAGS += -DSDL_MAIN_HANDLED
        LDLIBS += -l:libSDL2.a -l:libvorbisfile.a
        LDLIBS += -lole32 -loleaut32 -limm32 -lsetupapi -lversion -lgdi32 -lws2_32 -lwinmm
    endif
else ifeq ($(MODULE),server)
    CPPFLAGS += -DMODULE_SERVER
    ifeq ($(CROSS),win32)
        LDLIBS += -lwinmm
    endif
else ifeq ($(MODULE),toolbox)
    CPPFLAGS += -DMODULE_TOOLBOX
endif
ifdef DEBUG
    CFLAGS += -Og -g
    CPPFLAGS += -DDBGLVL=$(DEBUG)
    ifeq ($(CROSS),win32)
        WRFLAGS += -DDBGLVL=$(DEBUG)
    endif
    NOSTRIP = y
    ifdef ASAN
        CFLAGS += -fsanitize=address
        LDFLAGS += -fsanitize=address
    endif
else
    ifndef O
        O = 2
    endif
    CFLAGS += -O$(O) -fno-exceptions
    ifndef NOLTO
        CFLAGS += -flto=auto
        LDFLAGS += -flto=auto
    endif
endif
ifdef NATIVE
    CFLAGS += -march=native -mtune=native
endif
ifdef M32
    CPPFLAGS += -DM32
    ifeq ($(CROSS),win32)
        WRFLAGS += -DM32
    endif
endif

ifeq ($(MODULE),toolbox)
BIN := pstools
else ifeq ($(MODULE),server)
BIN := psrc-server
else
BIN := psrc
endif
ifdef DEBUG
    BIN := $(BIN).debug
    ifdef ASAN
        BIN := $(BIN).asan
    endif
endif
ifeq ($(CROSS),win32)
    BIN := $(BIN).exe
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
export LIBDIR
export OUTDIR
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

.SECONDEXPANSION:

define a
$(LIBDIR)/lib$(1).a
endef
define inc
$$(patsubst noexist\:,,$$(patsubst $(null),,$$(wildcard $$(shell $(CC) $(CFLAGS) $(CPPFLAGS) -x c -MM $(null) $$(wildcard $(1)) -MT noexist))))
endef

default: bin

ifndef MKSUB
$(LIBDIR)/lib%.a: $$(wildcard $(SRCDIR)/$(notdir %)/*.c) $(call inc,$(SRCDIR)/$(notdir %)/*.c)
	@$(MAKE) --no-print-directory MKSUB=y SRCDIR=$(SRCDIR)/$(notdir $*) OBJDIR=$(OBJDIR)/$(notdir $*) OUTDIR=$(LIBDIR) BIN=$@
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
ifeq ($(MODULE),engine)
$(BIN): $(OBJECTS) $(call a,psrc_engine) $(call a,psrc_server) $(call a,psrc_aux) $(call a,glad) $(call a,stb) $(call a,miniaudio)
else ifeq ($(MODULE),server)
$(BIN): $(OBJECTS) $(call a,psrc_server) $(call a,psrc_aux)
else ifeq ($(MODULE),toolbox)
$(BIN): $(OBJECTS) $(call a,psrc_toolbox) $(call a,tinyobj)
else
$(BIN): $(OBJECTS)
endif
	@echo Linking $(notdir $@)...
	@$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
ifndef NOSTRIP
	@$(STRIP) -s -R ".comment" -R ".note.*" -R ".gnu.build-id" $@
endif
	@echo Linked $(notdir $@)
else
$(BIN): $(OBJECTS)
	@echo Building $(notdir $@)...
	@$(AR) rcs $@ $^
	@echo Built $(notdir $@)
endif

bin: $(BIN)
	@$(nop)

run: bin
	@echo Running $(notdir $(BIN))...
	@$(call run,$(BIN))

clean:
	@$(call rmdir,$(OBJDIR))
	@$(call rmdir,$(LIBDIR))
	@$(call rm,$(BIN))

.PHONY: clean bin run
