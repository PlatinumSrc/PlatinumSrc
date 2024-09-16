DIRS := $(patsubst %/,%,$(dir $(wildcard */Makefile)))

BUILD := $(DIRS:%=build\ %)
CLEAN := $(DIRS:%=clean\ %)
DISTCLEAN := $(DIRS:%=distclean\ %)

all build: $(BUILD)
$(BUILD):
	@$(MAKE) -C $(wordlist 2,2,$@)

clean: $(CLEAN)
$(CLEAN):
	@$(MAKE) -C $(wordlist 2,2,$@) clean

distclean: $(DISTCLEAN)
$(DISTCLEAN):
	@$(MAKE) -C $(wordlist 2,2,$@) distclean

.PHONY: all $(BUILD) $(CLEAN) $(DISTCLEAN)
