CFLAGS?=-O3 --opt-code-speed
PLATFORM?=ti8x

CC=$(shell which zcc z88dk.zcc | head -1)
LD=$(CC)
SHELL=bash

BUILD=build

wilder_card=$(wildcard $(1)/**/$(2)) $(wildcard $(1)/$(2))
define source_directory
$(eval $(1)=$(2))
$(eval $(1)_FILES=$(call wilder_card,$(2),*.s) $(call wilder_card,$(2),*.c))
$(eval $(1)_OBJECT_FILES=$$(subst $(2)/,$$(BUILD)/$(2)/,$$(patsubst %.s,%.o,$$(patsubst %.c,%.o,$$($(1)_FILES)))))
$(eval vpath %.c $(2))
$(eval vpath %.s $(2))
endef

$(call source_directory,SRC,src)

# It's extremely important that the source file paths are absolute, otherwise the
# extension will have trouble mapping the paths.
define source_compile
	mkdir -p "$(dir $@)"
	$(CC) +$(PLATFORM) $(CFLAGS) $(1) -o "$@" "$(realpath $<)"
endef

.ONESHELL:

all: $(BUILD)/gdb.lib

clean:
	rm -rf build

$(BUILD):
	@mkdir -p "$@"

$(BUILD)/gdb.lib: $(BUILD)/gdb.lst $(SRC_OBJECT_FILES)
	cd $(BUILD) && z88dk.z88dk-z80asm -d -xgdb "@$(subst $(BUILD)/,,$<)"

$(BUILD)/gdb.lst: | $(BUILD)
	echo > "$@"
	for each in $(SRC_FILES) ; do
		echo $${each%.*} >> "$@"
	done

.PRECIOUS: $(BUILD)/%.o
$(BUILD)/%.o: $(BUILD)/%.o.asm | $(BUILD)
	$(call source_compile,-c)

.PRECIOUS: $(BUILD)/%.o.asm
$(BUILD)/%.o.asm: %.s | $(BUILD)
	$(call source_compile,-S)

.PRECIOUS: $(BUILD)/%.o.asm
$(BUILD)/%.o.asm: %.c | $(BUILD)
	$(call source_compile,-S)