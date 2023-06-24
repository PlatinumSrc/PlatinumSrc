MODULE ?= engine
CROSS ?= 
SRCDIR ?= src
OBJDIR ?= obj
LIBDIR ?= lib
OUTDIR ?= .

ifndef OS
    ifeq ($(CROSS),)
        CC ?= gcc
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
            STRIP = x86_64-w64-mingw32-strip
            WINDRES = x86_64-w64-mingw32-windres
            PLATFORMDIR := Windows_x86_64
        else
            CC = i686-w64-mingw32-gcc
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
    PLATFORMDIR := debug/$(PLATFORMDIR)
endif
PLATFORMDIR := $(PLATFORMDIR)/$(MODULE)
OBJDIR := $(OBJDIR)/$(PLATFORMDIR)
LIBDIR := $(LIBDIR)/$(PLATFORMDIR)

CFLAGS += -Wall -Wextra -Wuninitialized -pthread -ffast-math -I$(SRCDIR)
CPPFLAGS += -D_DEFAULT_SOURCE -D_GNU_SOURCE
LDLIBS += -lm
LDFLAGS += 
ifeq ($(CROSS),win32)
    WRFLAGS += 
endif
ifeq ($(CROSS),)
    LDLIBS += -lpthread
else ifeq ($(CROSS),win32)
    LDLIBS += -l:libwinpthread.a
endif
ifeq ($(MODULE),engine)
    ifeq ($(CROSS),)
        LDLIBS += -lX11 -lSDL2
    else ifeq ($(CROSS),win32)
        CPPFLAGS += -DSDL_MAIN_HANDLED
        LDLIBS += -l:libSDL2.a -lole32 -loleaut32 -limm32 -lsetupapi -lversion -lgdi32 -lws2_32 -lwinmm
    endif
else ifeq ($(MODULE),toolbox)
endif
ifdef DEBUG
    CFLAGS += -Og -g
    CPPFLAGS += -DDBGLVL=$(DEBUG)
    ifeq ($(CROSS),win32)
        WRFLAGS += -DDBGLVL=$(DEBUG)
    endif
else
    CFLAGS += -O2
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

SOURCES := $(wildcard $(SRCDIR)/*.c)
DEPENDS := $(wildcard $(SRCDIR)/*.h)
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))
ifeq ($(MODULE),toolbox)
BIN := ptoolbox
else
BIN := psrc
endif
ifeq ($(CROSS),win32)
    BIN := $(BIN).exe
endif

export MODULE
export CROSS
export DEBUG
export M32
export NATIVE
export NOLTO
export CFLAGS
export CPPFLAGS
ifeq ($(CROSS),win32)
    export WRFLAGS
endif

ifeq ($(SHCMD),unix)
define esctext
$(subst ','\'',$(1))
endef
else
define mkpath
$(subst /,\,$(1))
endef
endif

ifeq ($(SHCMD),unix)
define mkdir
if [ ! -d '$(call esctext,$(1))' ]; then echo 'Creating $(call esctext,$(1))...'; mkdir -p '$(call esctext,$(1))'; fi; true
endef
define rm
if [ -f '$(call esctext,$(1))' ]; then echo 'Removing $(call esctext,$(1))...'; rm -f '$(call esctext,$(1))'; fi; true
endef
define rmdir
if [ -d '$(call esctext,$(1))' ]; then echo 'Removing $(call esctext,$(1))...'; rm -rf '$(call esctext,$(1))'; fi; true
endef
define run
./'$(call esctext,$(1))'
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

define lib
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

ifndef MKSUB
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(call inc,$(SRCDIR)/%.c) | $(OBJDIR)
else
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(call inc,$(SRCDIR)/%.c) | $(OBJDIR) $(OUTDIR)
endif
	@echo Compiling $(notdir $<)...
	@$(CC) $(CFLAGS) $(CPPFLAGS) $< -c -o $@
	@echo Compiled $(notdir $<)

ifndef MKSUB
ifeq ($(MODULE),engine)
$(BIN): $(OBJECTS) $(call lib,engine) $(call lib,glad) $(call lib,stb) $(call lib,miniaudio)
else ifeq ($(MODULE),toolbox)
$(BIN): $(OBJECTS) $(call lib,toolbox) $(call lib,tinyobj)
else
$(BIN): $(OBJECTS)
endif
	@echo Linking $(notdir $(BIN))...
	@$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
	@echo Linked $(notdir $(BIN))
else
$(BIN): $(OBJECTS)
	@echo Building $(notdir $(BIN))...
	@$(AR) rcs $@ $^
	@echo Built $(notdir $(BIN))
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
