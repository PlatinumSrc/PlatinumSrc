MODULE ?= engine
CROSS ?= 
SRCDIR ?= src
OBJDIR ?= obj
OUTDIR ?= .

MODULECFLAGS := -DMODULEID_ENGINE=0 -DMODULEID_SERVER=1
ifeq ($(MODULE),engine)
    MODULEID := 0
    MODULECFLAGS += -DMODULE_ENGINE
    MODULECFLAGS += -DMODULE_SERVER
else ifeq ($(MODULE),server)
    MODULEID := 1
    MODULECFLAGS += -DMODULE_SERVER
else
    .PHONY: error
    error:
	    @echo Invalid module: $(MODULE)
	    @exit 1
endif

ifndef OS
    ifeq ($(CROSS),)
        CC ?= gcc
        STRIP ?= strip
        WINDRES ?= true
    else ifeq ($(CROSS),win32)
        ifndef M32
            CC = x86_64-w64-mingw32-gcc
            STRIP = x86_64-w64-mingw32-strip
            WINDRES = x86_64-w64-mingw32-windres
        else
            CC = i686-w64-mingw32-gcc
            STRIP = i686-w64-mingw32-strip
            WINDRES = i686-w64-mingw32-windres
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

CFLAGS += -Wall -Wextra -pthread -ffast-math
CPPFLAGS += -D_DEFAULT_SOURCE -D_GNU_SOURCE $(MODULECFLAGS) -DMODULEID=$(MODULEID) -DMODULE=$(MODULE)
LDLIBS += -lm
LDFLAGS += 
ifeq ($(CROSS),win32)
    WRFLAGS += $(MODULECFLAGS) -DMODULEID=$(MODULEID) -DMODULE=$(MODULE)
endif
ifeq ($(MODULE),engine)
    ifeq ($(CROSS),)
        LDLIBS += -lX11 -lSDL2
    else ifeq ($(CROSS),win32)
        LDLIBS += -l:libSDL2.a -lole32 -loleaut32 -limm32 -lsetupapi -lversion -lgdi32
    endif
endif
ifeq ($(CROSS),)
    LDLIBS += -lpthread
else ifeq ($(CROSS),win32)
    LDLIBS += -l:libwinpthread.a -lws2_32 -lwinmm
endif
ifdef DEBUG
    CFLAGS += -Og -g
    CPPFLAGS += -DDEBUG=$(DEBUG)
    ifeq ($(CROSS),win32)
        WRFLAGS += -DDEBUG=$(DEBUG)
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
ifeq ($(MODULE),engine)
    BIN := psrc
else ifeq ($(MODULE),server)
    BIN := psrv
endif
ifeq ($(CROSS),win32)
    BIN := $(BIN).exe
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
define null
echo -n > /dev/null
endef
else ifeq ($(SHCMD),win32)
define null
echo. > NUL
endef
endif

default: bin

$(OBJDIR):
	@$(call mkdir,obj)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo Compiling $(notdir $<)...
	@$(CC) $(CFLAGS) $(CPPFLAGS) $< -Wuninitialized -c -o $@
	@echo Compiled $(notdir $<)

$(BIN): $(OBJECTS)
	@echo Building $(notdir $@)...
	@$(CC) $(LDFLAGS) $(LDLIBS) $^ -o $@

objects: $(OBJDIR) $(OBJECTS)

bin: objects $(BIN)
	@$(null)

run: bin
	@$(call run,$(BIN))

clean:
	@$(call rmdir,$(OBJDIR))
	@$(call rm,$(BIN))

.PHONY: objects bin run clean
