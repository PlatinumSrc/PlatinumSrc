SRCDIR ?= src
OBJDIR ?= obj
OUTDIR ?= .
PSRCDIR ?= ../../src

SOURCES ?= $(wildcard $(SRCDIR)/*.c)
OBJECTS ?= $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

BIN ?= out

TARGET := $(OUTDIR)/$(BIN)
ifeq ($(OS),Windows_NT)
    TARGET := $(TARGET).exe
endif

CC ?= gcc
LD := $(CC)
STRIP ?= strip
_CC := $(TOOLCHAIN)$(CC)
_LD := $(TOOLCHAIN)$(LD)
_STRIP := $(TOOLCHAIN)$(STRIP)

_CFLAGS := $(CFLAGS) -Wall -Wextra -Wuninitialized -Wundef
_CPPFLAGS += -DPSRC_REUSABLE -I$(PSRCDIR)
_LDFLAGS := $(LDFLAGS)
_LDLIBS := $(LDLIBS)
ifneq ($(DEBUG),y)
    _CPPFLAGS += -DNDEBUG
    O ?= 2
else
    _CFLAGS += -g -Wdouble-promotion -fno-omit-frame-pointer -std=c11 -pedantic
    O ?= g
endif
_CFLAGS += -O$(O)
ifeq ($(ASAN),y)
    _CFLAGS += -fsanitize=address
    _LDFLAGS += -fsanitize=address
endif

.SECONDEXPANSION:

define mkdir
if [ ! -d '$(1)' ]; then echo 'Creating $(1)/...'; mkdir -p '$(1)'; fi; true
endef
define rm
if [ -f '$(1)' ]; then echo 'Removing $(1)...'; rm -f '$(1)'; fi; true
endef
define rmdir
if [ -d '$(1)' ]; then echo 'Removing $(1)/...'; rm -rf '$(1)'; fi; true
endef

deps.filter := %.c %.h
deps.option := -MM
define deps
$$(filter $$(deps.filter),,$$(shell $(_CC) $(_CFLAGS) $(_CPPFLAGS) -E $(deps.option) $(1)))
endef

default: build

$(OUTDIR):
	@$(call mkdir,$@)

$(OBJDIR):
	@$(call mkdir,$@)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(call deps,$(SRCDIR)/%.c) | $(OBJDIR) $(OUTDIR)
	@echo Compiling $<...
	@$(_CC) $(_CFLAGS) $(_CPPFLAGS) $< -c -o $@
	@echo Compiled $<

$(TARGET): $(OBJECTS) | $(OUTDIR)
	@echo Linking $@...
	@$(_LD) $(_LDFLAGS) $^ $(_LDLIBS) -o $@
ifneq ($(NOSTRIP),y)
	@$(_STRIP) -s -R '.comment' -R '.note.*' -R '.gnu.build-id' $@ || exit 0
endif
	@echo Linked $@

build: $(TARGET)
	@:

clean:
	@$(call rmdir,$(OBJDIR))

distclean: clean
	@$(call rm,$(TARGET))

.PHONY: default build clean distclean
