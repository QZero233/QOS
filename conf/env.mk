# env.mk - configuration variables for the JOS lab

# '$(V)' controls whether the lab makefiles print verbose commands (the
# actual shell commands run by Make), as well as the "overview" commands
# (such as '+ cc lib/readline.c').
#
# For overview commands only, the line should read 'V = @'.
# For overview and verbose commands, the line should read 'V ='.
V = @

# If your system-standard GNU toolchain is ELF-compatible, then comment
# out the following line to use those tools (as opposed to the i386-jos-elf
# tools that the 6.828 make system looks for by default).
#
GCCPREFIX=i386-elf-

# If the makefile cannot find your QEMU binary, uncomment the
# following line and set it to the full path to QEMU.
#
QEMU=D:\OS\QEMU\qemu-system-i386.exe

# Command of 'mkdir -p $(@D)' (on Windows, we do not use -p, and if dir exists, it will report error)
MAKE_DIR_D = @if not exist $(subst /,\,$(@D)) (mkdir $(subst /,\,$(@D)))
#MAKE_DIR_D = @mkdir $(subst /,\,$(@D)) || true

# Add -fno-stack-protector or not
# Assign the variable with 1 to add
# NOTE: I make -fno-stack-protector option enabled, because I've checked my i386-elf-gcc and it supports this option
# If anything goes wrong, disable it
GCC_ENABLE_NO_STACK_PROTECT = 1

# Add -D qemu.log for QEMUOPT or not
# Assign 1 to add
QEMU_SUPPORT_LOG = 1
