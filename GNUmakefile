#
# This makefile system follows the structuring conventions
# recommended by Peter Miller in his excellent paper:
#
#	Recursive Make Considered Harmful
#	http://aegis.sourceforge.net/auug97.pdf
#
OBJDIR := obj

# Run 'make V=1' to turn on verbose commands, or 'make V=0' to turn them off.
ifeq ($(V),1)
override V =
endif
ifeq ($(V),0)
override V = @
endif

-include conf/lab.mk

-include conf/env.mk

LABSETUP ?= ./

TOP = .

# Cross-compiler jos toolchain
#
# This Makefile will automatically use the cross-compiler toolchain
# installed as 'i386-jos-elf-*', if one exists.  If the host tools ('gcc',
# 'objdump', and so forth) compile for a 32-bit x86 ELF target, that will
# be detected as well.  If you have the right compiler toolchain installed
# using a different name, set GCCPREFIX explicitly in conf/env.mk

# try to infer the correct GCCPREFIX
ifndef GCCPREFIX
GCCPREFIX := $(shell if i386-jos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
	then echo 'i386-jos-elf-'; \
	elif objdump -i 2>&1 | grep 'elf32-i386' >/dev/null 2>&1; \
	then echo ''; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2; \
	echo "*** Is the directory with i386-jos-elf-gcc in your PATH?" 1>&2; \
	echo "*** If your i386-*-elf toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than 'i386-jos-elf-', set your GCCPREFIX" 1>&2; \
	echo "*** environment variable to that prefix and run 'make' again." 1>&2; \
	echo "*** To turn off this error, run 'gmake GCCPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

# try to infer the correct QEMU
ifndef QEMU
QEMU := $(shell if which qemu >/dev/null 2>&1; \
	then echo qemu; exit; \
        elif which qemu-system-i386 >/dev/null 2>&1; \
        then echo qemu-system-i386; exit; \
	else \
	qemu=/Applications/Q.app/Contents/MacOS/i386-softmmu.app/Contents/MacOS/i386-softmmu; \
	if test -x $$qemu; then echo $$qemu; exit; fi; fi; \
	echo "***" 1>&2; \
	echo "*** Error: Couldn't find a working QEMU executable." 1>&2; \
	echo "*** Is the directory containing the qemu binary in your PATH" 1>&2; \
	echo "*** or have you tried setting the QEMU variable in conf/env.mk?" 1>&2; \
	echo "***" 1>&2; exit 1)
endif

# try to generate a unique GDB port
# Here we hard-code it as 27492
# Because I don't know how to write the right command
GDBPORT	:= 27492

CC	:= $(GCCPREFIX)gcc -pipe
GDB	:= $(GCCPREFIX)gdb
AS	:= $(GCCPREFIX)as
AR	:= $(GCCPREFIX)ar
LD	:= $(GCCPREFIX)ld
OBJCOPY	:= $(GCCPREFIX)objcopy
OBJDUMP	:= $(GCCPREFIX)objdump
NM	:= $(GCCPREFIX)nm

# Native commands
NCC	:= gcc $(CC_VER) -pipe
NATIVE_CFLAGS := $(CFLAGS) $(DEFS) $(LABDEFS) -I$(TOP) -MD -Wall
TAR	:= gtar
PERL	:= perl

# Compiler flags
# -fno-builtin is required to avoid refs to undefined functions in the kernel.
# Only optimize to -O1 to discourage inlining, which complicates backtraces.
CFLAGS := $(CFLAGS) $(DEFS) $(LABDEFS) -O1 -fno-builtin -I$(TOP) -MD
CFLAGS += -fno-omit-frame-pointer
CFLAGS += -std=gnu99
CFLAGS += -static
CFLAGS += -fno-pie
CFLAGS += -Wall -Wno-format -Wno-unused -Werror -gstabs -m32 -Wno-address-of-packed-member
# -fno-tree-ch prevented gcc from sometimes reordering read_ebp() before
# mon_backtrace()'s function prologue on gcc version: (Debian 4.7.2-5) 4.7.2
CFLAGS += -fno-tree-ch

CFLAGS += -I$(TOP)/net/lwip/include \
	  -I$(TOP)/net/lwip/include/ipv4 \
	  -I$(TOP)/net/lwip/jos

# Add -fno-stack-protector if the option exists.
#CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
ifeq ($(GCC_ENABLE_NO_STACK_PROTECT),1)
CFLAGS += -fno-stack-protector
endif

# Common linker flags
LDFLAGS := -m elf_i386

# Linker flags for JOS user programs
ULDFLAGS := -T user/user.ld

GCC_LIB := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

# Lists that the */Makefrag makefile fragments will add to
OBJDIRS :=

# Make sure that 'all' is the first target
all:

# Eliminate default suffix rules
.SUFFIXES:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# make it so that no intermediate .o files are ever deleted
.PRECIOUS: %.o $(OBJDIR)/boot/%.o $(OBJDIR)/kern/%.o \
	   $(OBJDIR)/lib/%.o $(OBJDIR)/fs/%.o $(OBJDIR)/net/%.o \
	   $(OBJDIR)/user/%.o

KERN_CFLAGS := $(CFLAGS) -DJOS_KERNEL -gstabs
USER_CFLAGS := $(CFLAGS) -DJOS_USER -gstabs

# Update .vars.X if variable X has changed since the last make run.
#
# Rules that use variable X should depend on $(OBJDIR)/.vars.X.  If
# the variable's value has changed, this will update the vars file and
# force a rebuild of the rule that depends on it.

#The logic here is as follows:
#This statement can be understood as an OR expression, where the left side is similar to echo 8848 | cmp -s .var.xx
#and the right side is similar to echo 8848 > .var_xx.
#The cmp command will return 0 or non-zero as the value of the left command.
#If cmp returns 1, indicating they are the same, then the subsequent statement will not be executed (because it's an OR expression).
#Otherwise, the OR expression is executed to output the latest variable value.
#The same approach can be used to implement this method in Windows.
$(OBJDIR)/.vars.%: FORCE
	$(eval dst_file := $(subst /,\,$@))
	@echo $($*) > .var_tmp
	@(if exist $(dst_file) ( \
		fc /b .var_tmp $(dst_file) > nul &\
		if errorlevel 1 ( \
			copy /y .var_tmp $(dst_file) > nul \
		) \
    ) \
    else ( \
    	echo $($*) > $(dst_file) \
    ))
	@rm .var_tmp
.PRECIOUS: $(OBJDIR)/.vars.%
.PHONY: FORCE


# Include Makefrags for subdirectories
include boot/Makefrag
include kern/Makefrag
include lib/Makefrag
include user/Makefrag
include fs/Makefrag
include net/Makefrag


CPUS ?= 1

PORT7	:= 27493
PORT80	:= 27494

QEMUOPTS = -drive file=$(OBJDIR)/kern/kernel.img,index=0,media=disk,format=raw -serial mon:stdio -gdb tcp::$(GDBPORT)

#QEMUOPTS += $(shell if ($(QEMU) -nographic -help | grep -q '^-D ')(echo '-D qemu.log') )
ifeq ($(QEMU_SUPPORT_LOG),1)
QEMUOPTS += -D qemu.log
endif

IMAGES = $(OBJDIR)/kern/kernel.img
QEMUOPTS += -smp $(CPUS)
QEMUOPTS += -drive file=$(OBJDIR)/fs/fs.img,index=1,media=disk,format=raw
IMAGES += $(OBJDIR)/fs/fs.img

# Here, host forward means when I visit the port $(PORT80) of *localhost*, the traffic will be redirected to the QEMU
# So, just visit 127.0.0.1:$(PORT80) on chrome of Windows will be fine
QEMUOPTS += -netdev user,id=net0,hostfwd=tcp::$(PORT7)-:7,hostfwd=tcp::$(PORT80)-:80,hostfwd=udp::$(PORT7)-:7
QEMUOPTS += -device e1000,netdev=net0
QEMUOPTS += -object filter-dump,id=f1,netdev=net0,file=qemu.pcap
QEMUOPTS += $(QEMUEXTRA)

.gdbinit: .gdbinit.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

gdb:
	$(GDB) -n -x .gdbinit

pre-qemu: .gdbinit
#	QEMU doesn't truncate the pcap file.  Work around this.
	@rm -f qemu.pcap

qemu: $(IMAGES) pre-qemu
	$(QEMU) $(QEMUOPTS)

qemu-nox: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Use Ctrl-a x to exit qemu"
	@echo "***"
	$(QEMU) -nographic $(QEMUOPTS)

qemu-gdb: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Now run 'make gdb'." 1>&2
	@echo "***"
	$(QEMU) $(QEMUOPTS) -S

qemu-nox-gdb: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Now run 'make gdb'." 1>&2
	@echo "***"
	$(QEMU) -nographic $(QEMUOPTS) -S

print-qemu:
	@echo $(QEMU)

print-qemu-opts:
	@echo $(QEMUOPTS)

print-gdbport:
	@echo $(GDBPORT)

# For deleting the build
clean:
	rd /s /q $(OBJDIR) .gdbinit jos.in qemu.log 2>nul || true

realclean: clean
	rd /s /q lab$(LAB).tar.gz \
		jos.out $(wildcard jos.out.*) \
		qemu.pcap $(wildcard qemu.pcap.*) \
		myapi.key || true

distclean: realclean
	rm -rf conf/gcc.mk

# For test runs
prep-net_%: override INIT_CFLAGS+=-DTEST_NO_NS

prep-%:
	$(V)$(MAKE) "INIT_CFLAGS=${INIT_CFLAGS} -DTEST=`case $* in *_*) echo $*;; *) echo user_$*;; esac`" $(IMAGES)

run-%-nox-gdb: prep-% pre-qemu
	$(QEMU) -nographic $(QEMUOPTS) -S

run-%-gdb: prep-% pre-qemu
	$(QEMU) $(QEMUOPTS) -S

run-%-nox: prep-% pre-qemu
	$(QEMU) -nographic $(QEMUOPTS)

run-%: prep-% pre-qemu
	$(QEMU) $(QEMUOPTS)

# For network connections
which-ports:
	@echo "Local port $(PORT7) forwards to JOS port 7 (echo server)"
	@echo "Local port $(PORT80) forwards to JOS port 80 (web server)"

nc-80:
	nc localhost $(PORT80)

nc-7:
	nc localhost $(PORT7)

telnet-80:
	telnet localhost $(PORT80)

telnet-7:
	telnet localhost $(PORT7)

# This magic automatically generates makefile dependencies
# for header files included from C source files we compile,
# and keeps those dependencies up-to-date every time we recompile.
# See 'mergedep.pl' for more information.
$(OBJDIR)/.deps: $(foreach dir, $(OBJDIRS), $(wildcard $(OBJDIR)/$(dir)/*.d))
	$(MAKE_DIR_D)
	@$(PERL) mergedep.pl $@ $^

-include $(OBJDIR)/.deps

always:
	@:

.PHONY: all always \
	handin git-handin tarball tarball-pref clean realclean distclean grade handin-prep handin-check
