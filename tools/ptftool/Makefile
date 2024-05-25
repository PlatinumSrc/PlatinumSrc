SRCDIR := src
OBJDIR := obj
OUTDIR := .
PSRCDIR := ../../src/psrc

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

TARGET := $(OUTDIR)/ptftool

CC ?= gcc
LD := $(CC)
STRIP ?= strip

LDLIBS += -lm

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
null := /dev/null
define inc
$$(patsubst $(null)\:,,$$(patsubst $(null),,$$(wildcard $$(shell $(CC) $(_CFLAGS) $(_CPPFLAGS) -x c -MM $(null) $$(wildcard $(1)) -MT $(null)))))
endef

default: build

$(OUTDIR):
	@$(call mkdir,$@)

$(OBJDIR):
	@$(call mkdir,$@)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(call inc,$(SRCDIR)/%.c) | $(OBJDIR) $(OUTDIR)
	@echo Compiling $<...
	@$(CC) $(CFLAGS) -Wall -Wextra -I$(PSRCDIR) $(CPPFLAGS) $< -c -o $@
	@echo Compiled $<

$(TARGET): $(OBJECTS) | $(OUTDIR)
	@echo Linking $@...
	@$(LD) $(LDFLAGS) $^ $(LDLIBS) -o $@
ifneq ($(NOSTRIP),y)
	@$(STRIP) -s -R '.comment' -R '.note.*' -R '.gnu.build-id' $@ || exit 0
endif
	@echo Linked $@

build: $(TARGET)
	@:

clean:
	@$(call rmdir,$(OBJDIR))

distclean: clean
	@$(call rm,$(TARGET))

.PHONY: build clean distclean
