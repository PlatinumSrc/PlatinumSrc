SRCDIR := src
OBJDIR := obj
OUTDIR := .
PSRCDIR := ../../src/psrc

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

BIN := platinum
ifeq ($(OS),Windows_NT)
    BIN := $(BIN).exe
endif

TARGET := $(OUTDIR)/$(BIN)

CC ?= gcc
LD := $(CC)
STRIP ?= strip
_CC := $(TOOLCHAIN)$(CC)
_LD := $(TOOLCHAIN)$(LD)
_STRIP := $(TOOLCHAIN)$(STRIP)

CFLAGS += -O2

.SECONDEXPANSION:

define mkdir
if [ ! -d '$(1)' ]; then echo 'Creating $(1)/...'; mkdir -p '$(1)'; fi; true
endef
define rm
if [ -f '$(1)' ]; then echo 'Removing $(1)/...'; rm -f '$(1)'; fi; true
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
	@$(_CC) $(CFLAGS) -Wall -Wextra -I$(PSRCDIR) $(CPPFLAGS) $< -c -o $@
	@echo Compiled $<

$(TARGET): $(OBJECTS) | $(OUTDIR)
	@echo Linking $@...
	@$(_LD) $(LDFLAGS) $^ $(LDLIBS) -o $@
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

.PHONY: build clean distclean
