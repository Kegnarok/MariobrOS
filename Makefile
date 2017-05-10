OS_NAME = MarioBrOS

# Options

GUI = x
GUEST_MEMORY = 512  # Memory that our OS should have
HOST_MEMORY  = 512  # Memory that bochs should use to emulate our OS

# Folders and paths
SRC_DIR   = src
PROGS_SRC_DIR = progs
PROGS_ELF_DIR = iso/progs
BUILD_DIR = build
BOOT_DIR  = iso/boot
EMU_DIR   = emu

DISK_DIR  = disk
LOOP_DEVICE = /dev/loop0
MNT_DIR = /mnt/$(OS_NAME)_tmp
DISK_REF = $(DISK_DIR)/disk_ref.img
DISK_IMG = $(DISK_DIR)/disk.img

# Sources for the kernel
LINKER = $(SRC_DIR)/link.ld
OBJECTS = loader.o kmain.o shell.o process.o scheduler.o syscall.o bitset.o malloc.o paging.o memory.o filesystem.o ata_pio.o gdt.o gdt_asm.o timer.o keyboard.o irq.o irq_asm.o isr.o isr_asm.o idt.o idt_asm.o logging.o printer.o string.o io.o math.o queue.o list.o utils.o
OBJS = $(addprefix $(BUILD_DIR)/,$(OBJECTS))

# Sources for user programs
LINKER_PROG = $(PROGS_SRC_DIR)/link_prog.ld
LIB_PROG_C = $(PROGS_SRC_DIR)/lib.c
LIB_PROG_O = $(patsubst $(PROGS_SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LIB_PROG_C))
PROGS_C   = $(filter-out $(LIB_PROG_C),$(wildcard $(PROGS_SRC_DIR)/*.c))
PROGS_O   = $(patsubst $(PROGS_SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(PROGS_C))
PROGS_ELF = $(patsubst $(PROGS_SRC_DIR)/%.c,$(PROGS_ELF_DIR)/%.elf,$(PROGS_C))

# OS targets
OS_ISO = $(BUILD_DIR)/os.iso
KERNEL_ELF = $(BOOT_DIR)/kernel.elf

BOCHS_CONFIG_CD = config_cd.txt
BOCHS_CONFIG_DISK = config_disk.txt
BOCHS_CONFIG_DEBUGGER = config_db.txt
BOCHS_LOG = $(EMU_DIR)/logb.txt
GRUB_CONFIG = $(BOOT_DIR)/grub/menu.lst
GRUB2_CONFIG = $(BOOT_DIR)/grub/grub.cfg
ELTORITO = boot/grub/stage2_eltorito

# Configuration files

define BOCHS_CONFIG_CONTENT
megs:             32
display_library:  $(GUI)
romimage:         file=/usr/share/bochs/BIOS-bochs-latest
vgaromimage:      file=/usr/share/bochs/VGABIOS-lgpl-latest
log:              $(BOCHS_LOG)
clock:            sync=none, time0=local
cpu:              count=1, ips=1000000
memory:           guest=$(GUEST_MEMORY), host=$(HOST_MEMORY)
com1:             enabled=1, mode=file, dev=$(EMU_DIR)/com1.out
keyboard:         type=mf, serial_delay=200, paste_delay=100000, keymap=$(SRC_DIR)/x11-pc-fr.map
endef

define BOCHS_CONFIG_BOOT_CD
ata0-master:      type=cdrom, path=$(OS_ISO), status=inserted
boot:             cdrom
endef
define BOCHS_CONFIG_BOOT_DISK
ata0:             enabled=1, ioaddr1=0x1f0, ioaddr2=0x3f0, irq=14
ata0-master:      type=disk, path=$(DISK_IMG), mode=flat, translation=auto
boot:             disk
endef

define BOCHS_CONFIG_DB_CONTENT
continue
endef

define GRUB_CONFIG_CONTENT
default=0
timeout=0

title $(OS_NAME)
kernel /boot/kernel.elf
endef

define GRUB2_CONFIG_CONTENT
set timeout=0
menuentry "$(OS_NAME)" {
	multiboot /boot/kernel.elf
}
endef

define NEWLINE


endef

# C compiler and flags
CC =     gcc
CFLAGS =-m32 \
        -nostdlib -nodefaultlibs \
        -ffreestanding \
        -fno-builtin -nostartfiles\
        -fno-stack-protector \
        -funsigned-char -funsigned-bitfields \
        -Wall -Wextra -Werror \
        -O0 \
        -c
# Compiles in 32 bits mode, without any std, with all warnings (and treating warning as errors) \
  and with no linking (and disable optimizations)

# Linker and flags
LD =      ld
LDFLAGS = -melf_i386

# ASM compiler and flags
AS =      nasm
ASFLAGS = -f elf



all: runq

disk: diskq

.PHONY: all runb runq syncdisk disk diskb diskq clean cleandisk mount rsync umount log redisk progs core
.SECONDARY:   # Avoid deletion of intermediate files


$(BUILD_DIR) $(EMU_DIR) $(PROGS_ELF_DIR): # Ensures folders exist
	@mkdir -p $@

# Configuration files writing
ECHO_CONFIG = @echo '$(subst $(NEWLINE),\n,$(1))' > $(2)

$(BOCHS_CONFIG_CD): $(EMU_DIR)
	$(call ECHO_CONFIG,$(BOCHS_CONFIG_CONTENT)$(NEWLINE)$(BOCHS_CONFIG_BOOT_CD),$(EMU_DIR)/$(BOCHS_CONFIG_CD))
$(BOCHS_CONFIG_DISK): $(EMU_DIR)
	$(call ECHO_CONFIG,$(BOCHS_CONFIG_CONTENT)$(NEWLINE)$(BOCHS_CONFIG_BOOT_DISK),$(EMU_DIR)/$(BOCHS_CONFIG_DISK))
$(BOCHS_CONFIG_DEBUGGER): $(EMU_DIR)
	$(call ECHO_CONFIG,$(BOCHS_CONFIG_DB_CONTENT),$(EMU_DIR)/$(BOCHS_CONFIG_DEBUGGER))

$(GRUB_CONFIG):
	$(call ECHO_CONFIG,$(GRUB_CONFIG_CONTENT),$(GRUB_CONFIG))
$(GRUB2_CONFIG):
	$(call ECHO_CONFIG,$(GRUB2_CONFIG_CONTENT),$(GRUB2_CONFIG))


runb: $(OS_ISO) $(BOCHS_CONFIG_CD) $(BOCHS_CONFIG_DEBUGGER)
	bochs -q -f $(EMU_DIR)/$(BOCHS_CONFIG_CD) #-rc $(EMU_DIR)/$(BOCHS_CONFIG_DEBUGGER)

diskb: syncdisk $(BOCHS_CONFIG_DEBUGGER) #bochs
	bochs -q -f $(EMU_DIR)/$(BOCHS_CONFIG_DISK) #-rc $(EMU_DIR)/$(BOCHS_CONFIG_DEBUGGER)

runq: $(OS_ISO)
	qemu-system-i386 -cdrom build/os.iso

diskq: syncdisk #qemu
	qemu-system-i386 -boot c -drive format=raw,file=$(DISK_IMG) -m 512 -s -serial file:$(EMU_DIR)/logq.txt


# Kernel .c and .s compilation into .o
$(BUILD_DIR)/%.o: src/%.c $(BUILD_DIR)
	@$(CC) $< -c -o $@ $(CFLAGS) $(CPPFLAGS)

$(BUILD_DIR)/%.o: src/%.s $(BUILD_DIR)
	@$(AS) $< -o $@ $(ASFLAGS)

# Progs compilation into .o and .elf
$(BUILD_DIR)/%.o: $(PROGS_SRC_DIR)/%.c $(BUILD_DIR)
	@$(CC) $< -c -o $@ $(CFLAGS) $(CPPFLAGS)

$(PROGS_ELF_DIR)/%.elf: $(BUILD_DIR)/%.o $(LIB_PROG_O) $(PROGS_ELF_DIR) $(LINKER_PROG)
	@$(LD) $(LDFLAGS) -T $(LINKER_PROG) $(LIB_PROG_O) $< -o $@


$(OS_ISO):	$(GRUB_CONFIG) core
    # Builds the ISO image from the ISO folder
	genisoimage -R \
                -b $(ELTORITO) \
                -no-emul-boot \
                -boot-load-size 4 \
                -A $(OS_NAME) \
                -input-charset utf8 \
                -quiet \
                -boot-info-table \
                -o $(OS_ISO) \
                iso

core: $(KERNEL_ELF) $(PROGS_ELF)

$(KERNEL_ELF):	$(OBJS) $(LINKER)  # To remake if linker script changed
    # Links the file and produces the .elf in the ISO folder
	$(LD) $(LDFLAGS) -T $(LINKER) $(OBJS) -o $(KERNEL_ELF)



# Disk targets

$(DISK_REF):
	@mkdir -p $(DISK_DIR)
	dd if=/dev/zero of=$(DISK_REF) bs=512 count=131072
	sudo losetup $(LOOP_DEVICE) $(DISK_REF)
	sudo echo "," | sfdisk --quiet $(DISK_REF)
	@sudo partprobe $(LOOP_DEVICE)
	sudo mkfs.ext2 -b 2048 $(LOOP_DEVICE)p1
	@sudo mkdir -p $(MNT_DIR)
	@sudo mount $(LOOP_DEVICE)p1 $(MNT_DIR)
	sudo grub-install --root-directory=$(MNT_DIR) --no-floppy --modules="normal part_msdos ext2 multiboot" --target=i386-pc $(LOOP_DEVICE)
	@sudo umount -d $(MNT_DIR)
	@sudo rm -rf $(MNT_DIR)
	sudo losetup -d $(LOOP_DEVICE)

$(DISK_IMG): $(DISK_REF)
	dd if=$(DISK_REF) of=$(DISK_IMG) bs=512 count=131072

redisk: cleandisk disk

rsync: $(GRUB2_CONFIG) $(BOCHS_CONFIG_DISK)
	sudo rsync -r $(BOOT_DIR)      $(MNT_DIR)
	sudo rsync -r $(PROGS_ELF_DIR) $(MNT_DIR)

mount: $(DISK_IMG)
	@sudo losetup $(LOOP_DEVICE) $(DISK_IMG)
	@sudo partprobe $(LOOP_DEVICE)
	@sudo mkdir -p $(MNT_DIR)
	@sudo mount $(LOOP_DEVICE)p1 $(MNT_DIR)

umount:
	@sudo umount -d $(MNT_DIR)
	@sudo rm -rf $(MNT_DIR)
	@sudo losetup -d $(LOOP_DEVICE)

syncdisk: core mount rsync umount


# Clean targets

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(EMU_DIR)
	rm -f $(BOOT_DIR)/kernel.elf
	rm -rf $(PROGS_ELF_DIR)

cleandisk: clean
	rm -f $(DISK_IMG)

mrproper: cleandisk
	rm -f $(GRUB_CONFIG)
	rm -f $(GRUB2_CONFIG)
	rm -rf $(DISK_DIR)
