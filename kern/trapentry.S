/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(trap_divide_zero_handler,T_DIVIDE)
TRAPHANDLER_NOEC(trap_debug_handler,T_DEBUG)
TRAPHANDLER_NOEC(trap_nmi_handler,T_NMI)
TRAPHANDLER_NOEC(trap_brkpt_handler,T_BRKPT)
TRAPHANDLER_NOEC(trap_oflow_handler,T_OFLOW)
TRAPHANDLER_NOEC(trap_bound_handler,T_BOUND)
TRAPHANDLER_NOEC(trap_illop_handler,T_ILLOP)
TRAPHANDLER_NOEC(trap_device_handler,T_DEVICE)
TRAPHANDLER(trap_dblflt_handler,T_DBLFLT)
TRAPHANDLER(trap_tss_handler,T_TSS)
TRAPHANDLER(trap_tsegnp_handler,T_SEGNP)
TRAPHANDLER(trap_stack_handler,T_STACK)
TRAPHANDLER(trap_gpflt_handler,T_GPFLT)
TRAPHANDLER(trap_pgflt_handler,T_PGFLT)
TRAPHANDLER_NOEC(trap_fperr_handler,T_FPERR)
TRAPHANDLER(trap_align_handler,T_ALIGN)
TRAPHANDLER(trap_mchk_handler,T_MCHK)
TRAPHANDLER_NOEC(trap_smiderr_handler,T_SIMDERR)

TRAPHANDLER_NOEC(trap_syscall_handler,T_SYSCALL)

TRAPHANDLER_NOEC(irq_timer_handler,IRQ_OFFSET+IRQ_TIMER)
TRAPHANDLER_NOEC(irq_kbd_handler,IRQ_OFFSET+IRQ_KBD)
TRAPHANDLER_NOEC(irq_serial_handler,IRQ_OFFSET+IRQ_SERIAL)
TRAPHANDLER_NOEC(irq_spurious_handler,IRQ_OFFSET+IRQ_SPURIOUS)
TRAPHANDLER_NOEC(irq_ide_handler,IRQ_OFFSET+IRQ_IDE)
TRAPHANDLER_NOEC(irq_error_handler,IRQ_OFFSET+IRQ_ERROR)

/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
	pushl %ds
	pushl %es
	pushal

	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es

	pushl %esp
	call trap
