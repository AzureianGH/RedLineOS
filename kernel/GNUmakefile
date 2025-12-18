# Nuke built-in rules.
.SUFFIXES:

# This is the name that our final executable will have.
# Change as needed.
override OUTPUT := kernel

# Install prefix; /usr/local is a good, standard default pick.
PREFIX := /usr/local

# User controllable toolchain and toolchain prefix.
TOOLCHAIN := x86_64-elf
TOOLCHAIN_PREFIX := 
ifneq ($(TOOLCHAIN),)
    ifeq ($(TOOLCHAIN_PREFIX),)
        TOOLCHAIN_PREFIX := $(TOOLCHAIN)-
    endif
endif

# User controllable C compiler command.
ifneq ($(TOOLCHAIN_PREFIX),)
    CC := $(TOOLCHAIN_PREFIX)gcc
else
    CC := cc
endif

# User controllable linker command.
LD := $(TOOLCHAIN_PREFIX)ld

# Symbol dumper for ksym generation.
NM := $(TOOLCHAIN_PREFIX)nm

# Defaults overrides for variables if using "llvm" as toolchain.
ifeq ($(TOOLCHAIN),llvm)
    CC := clang
    LD := ld.lld
endif

# User controllable C flags.
CFLAGS := -g -O2 -pipe -std=gnu23

# User controllable C preprocessor flags. We set none by default.
CPPFLAGS :=

# User controllable nasm flags.
NASMFLAGS := -g

# User controllable linker flags. We set none by default.
LDFLAGS :=

# Check if CC is Clang.
override CC_IS_CLANG := $(shell ! $(CC) --version 2>/dev/null | grep -q '^Target: '; echo $$?)

# If the C compiler is Clang, set the target as needed.
ifeq ($(CC_IS_CLANG),1)
    override CC += \
        -target x86_64-unknown-none-elf
endif

# Internal C flags that should not be changed by the user.
override CFLAGS += \
    -Wall \
    -Wextra \
    -std=gnu11 \
    -nostdinc \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fno-PIC \
    -ffunction-sections \
    -fdata-sections \
    -m64 \
    -march=x86-64 \
    -mabi=sysv \
    -mno-80387 \
    -msse2 \
    -mno-red-zone \
    -mcmodel=kernel

# Internal C preprocessor flags that should not be changed by the user.
override CPPFLAGS := \
    -I src \
    -I include \
    -isystem include \
    $(CPPFLAGS) \
    -DLIMINE_API_REVISION=3 \
    -MMD \
    -MP

# Internal nasm flags that should not be changed by the user.
override NASMFLAGS := \
    -f elf64 \
    $(patsubst -g,-g -F dwarf,$(NASMFLAGS)) \
    -Wall

# Internal linker flags that should not be changed by the user.
override LDFLAGS += \
    -m elf_x86_64 \
    -nostdlib \
    -static \
    -z max-page-size=0x1000 \
    --gc-sections \
    -T linker-scripts/x86_64.lds

# Use "find" to glob all *.c, *.S, and *.asm files in the tree and obtain the
# object and header dependency file names.
override SRCFILES := $(filter-out src/generated/ksyms.c,$(shell find -L src cc-runtime/src -type f 2>/dev/null | LC_ALL=C sort))
override CFILES := $(filter %.c,$(SRCFILES))
override ASFILES := $(filter %.S,$(SRCFILES))
override NASMFILES := $(filter %.asm,$(SRCFILES))
override OBJ := $(addprefix obj/,$(CFILES:.c=.c.o) $(ASFILES:.S=.S.o) $(NASMFILES:.asm=.asm.o))
override HEADER_DEPS := $(addprefix obj/,$(CFILES:.c=.c.d) $(ASFILES:.S=.S.d))

# Generated symbol table source and its object. Always include it in the final link
# to provide addresses for all functions (global and static).
GENERATED_KSYMS := src/generated/ksyms.c
OBJ_KSYMS := obj/src/generated/ksyms.c.o
override OBJ += $(OBJ_KSYMS)
OBJ_NO_KSYMS := $(filter-out $(OBJ_KSYMS),$(OBJ))

# Default target. This must come first, before header dependencies.
.PHONY: all
all: bin/$(OUTPUT)

# Include header dependencies.
-include $(HEADER_DEPS)

# First link pass: build a kernel without the generated symbol table so we can
# extract every function symbol (including static ones).
bin/$(OUTPUT).stage1: GNUmakefile linker-scripts/x86_64.lds $(OBJ_NO_KSYMS)
	mkdir -p "$(dir $@)"
	$(LD) $(LDFLAGS) $(OBJ_NO_KSYMS) -o $@

# Generate the symbol table source from the stage1 kernel.
$(GENERATED_KSYMS): bin/$(OUTPUT).stage1
	@mkdir -p "$(dir $@)"
	@printf "/* Auto-generated; do not edit. */\n#include <symbols.h>\n#include <stddef.h>\n\nextern const struct ksym ksym_table[];\nextern const size_t ksym_count;\n\nconst struct ksym ksym_table[] = {\n" > $@
	@$(NM) -n -a $< | awk '($$2 ~ /^[tT]$$/) && ($$3 !~ /^\./) && ($$3 != "") { printf("    { 0x%s, \"%s\" },\n", $$1, $$3); }' >> $@
	@printf "};\n\nconst size_t ksym_count = sizeof(ksym_table) / sizeof(ksym_table[0]);\n" >> $@

# Link rules for the final executable using the generated symbol table.
bin/$(OUTPUT): GNUmakefile linker-scripts/x86_64.lds $(OBJ) $(GENERATED_KSYMS)
	mkdir -p "$(dir $@)"
	$(LD) $(LDFLAGS) $(OBJ) -o $@

# Compilation rules for *.c files.
obj/%.c.o: %.c GNUmakefile
	mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Compilation rules for *.S files.
obj/%.S.o: %.S GNUmakefile
	mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Compilation rules for *.asm (nasm) files.
obj/%.asm.o: %.asm GNUmakefile
	mkdir -p "$(dir $@)"
	nasm $(NASMFLAGS) $< -o $@

# Remove object files and the final executable.
.PHONY: clean
clean:
	rm -rf bin obj $(GENERATED_KSYMS)

# Install the final built executable to its final on-root location.
.PHONY: install
install: all
	install -d "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)"
	install -m 644 bin/$(OUTPUT) "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/"

# Try to undo whatever the "install" target did.
.PHONY: uninstall
uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/$(OUTPUT)"
	-rmdir "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)"
