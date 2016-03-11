arch ?= i386
kernel := build/kernel-$(arch).bin
iso := build/os-$(arch).iso

linker_script := src/arch/$(arch)/linker.ld
grub_cfg := src/arch/$(arch)/grub.cfg
assembly_source_files := $(wildcard src/arch/$(arch)/*.asm)
assembly_object_files := $(patsubst src/arch/$(arch)/%.asm, \
    build/arch/$(arch)/%.o, $(assembly_source_files))

c_source_files := $(wildcard src/*.c)
c_object_files := $(patsubst src/%.c, \
    build/%.o, $(c_source_files))
cflags := -nostdlib -nostdinc -fno-builtin -fno-stack-protector -m32

sources := main.o monitor.o common.o isr.o descriptor_tables.o

.PHONY: all clean run iso

all: $(kernel)

clean:
	@rm -r build

run: $(iso)
	@qemu-system-i386 -cdrom $(iso)

iso: $(iso)

$(iso): $(kernel) $(grub_cfg)
	@mkdir -p build/isofiles/boot/grub
	@cp $(kernel) build/isofiles/boot/kernel.bin
	@cp $(grub_cfg) build/isofiles/boot/grub
	@grub-mkrescue -o $(iso) build/isofiles 2> /dev/null
	@rm -r build/isofiles

$(kernel): $(assembly_object_files) $(linker_script) $(c_object_files)
	@ld -m elf_i386 -n --gc-sections -T $(linker_script) -o $(kernel) $(assembly_object_files) $(c_object_files)

build/%.o: src/%.c
	gcc $(cflags) -c -o $@ $<

# compile assembly files
build/arch/$(arch)/%.o: src/arch/$(arch)/%.asm
	@mkdir -p $(shell dirname $@)
	nasm -felf $< -o $@
