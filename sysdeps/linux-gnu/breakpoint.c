#include "config.h"

#include <sys/ptrace.h>
#include <string.h>
#include <error.h>
#include <errno.h>

#include "common.h"
#include "arch.h"

#ifdef ARCH_HAVE_ENABLE_BREAKPOINT
extern void arch_enable_breakpoint(pid_t, Breakpoint *);
void
enable_breakpoint(pid_t pid, Breakpoint *sbp) {
	debug(DEBUG_PROCESS, "enable_breakpoint: pid=%d, addr=%p, symbol=%s",
	      pid, sbp->addr, breakpoint_name(sbp));
	arch_enable_breakpoint(pid, sbp);
}
#else

static unsigned char break_insn[] = BREAKPOINT_VALUE;

void
enable_breakpoint(pid_t pid, Breakpoint *sbp) {
	unsigned int i, j;

	debug(DEBUG_PROCESS, "enable_breakpoint: pid=%d, addr=%p, symbol=%s",
	      pid, sbp->addr, breakpoint_name(sbp));

	for (i = 0; i < 1 + ((BREAKPOINT_LENGTH - 1) / sizeof(long)); i++) {
		long a = ptrace(PTRACE_PEEKTEXT, pid,
				sbp->addr + i * sizeof(long), 0);
		for (j = 0;
		     j < sizeof(long)
		     && i * sizeof(long) + j < BREAKPOINT_LENGTH; j++) {
			unsigned char *bytes = (unsigned char *)&a;

			sbp->orig_value[i * sizeof(long) + j] = bytes[j];
			bytes[j] = break_insn[i * sizeof(long) + j];
		}
		ptrace(PTRACE_POKETEXT, pid, sbp->addr + i * sizeof(long), a);
	}

	/* When using probe symbols, we need to bump the
	 * semaphore.  */
	SymBreakpoint * symbp = sbp->symbps;
	for (; symbp != NULL; symbp = symbp->next) {
		struct library_symbol * libsym = symbp->libsym;
		if (libsym != NULL
		    && libsym->sym_type == LS_ST_PROBE
		    && libsym->st_probe.sema != 0) {
			Process * proc = pid2proc(pid);
			uint16_t sema = 0;
			umovebytes(proc, libsym->st_probe.sema,
				   &sema, sizeof(sema));
			++sema;
			ustorebytes(proc, libsym->st_probe.sema,
				    &sema, sizeof(sema));
		}
	}
}
#endif				/* ARCH_HAVE_ENABLE_BREAKPOINT */

#ifdef ARCH_HAVE_DISABLE_BREAKPOINT
extern void arch_disable_breakpoint(pid_t, const Breakpoint *sbp);
void
disable_breakpoint(pid_t pid, const Breakpoint *sbp) {
	debug(DEBUG_PROCESS, "disable_breakpoint: pid=%d, addr=%p, symbol=%s",
	      pid, sbp->addr, breakpoint_name(sbp));
	arch_disable_breakpoint(pid, sbp);
}
#else
void
disable_breakpoint(pid_t pid, const Breakpoint *sbp) {
	unsigned int i, j;

	debug(DEBUG_PROCESS, "disable_breakpoint: pid=%d, addr=%p, symbol=%s",
	      pid, sbp->addr, breakpoint_name(sbp));

	for (i = 0; i < 1 + ((BREAKPOINT_LENGTH - 1) / sizeof(long)); i++) {
		long a = ptrace(PTRACE_PEEKTEXT, pid,
				sbp->addr + i * sizeof(long), 0);
		for (j = 0;
		     j < sizeof(long)
		     && i * sizeof(long) + j < BREAKPOINT_LENGTH; j++) {
			unsigned char *bytes = (unsigned char *)&a;

			bytes[j] = sbp->orig_value[i * sizeof(long) + j];
		}
		ptrace(PTRACE_POKETEXT, pid, sbp->addr + i * sizeof(long), a);
	}

	/* When using probe symbols, we need to decrease the
	 * semaphore again.  */
	SymBreakpoint * symbp = sbp->symbps;
	for (; symbp != NULL; symbp = symbp->next) {
		struct library_symbol * libsym = symbp->libsym;
		if (libsym != NULL
		    && libsym->sym_type == LS_ST_PROBE
		    && libsym->st_probe.sema != 0) {
			Process * proc = pid2proc(pid);
			uint16_t sema = 1;
			umovebytes(proc, libsym->st_probe.sema,
				   &sema, sizeof(sema));
			--sema;
			ustorebytes(proc, libsym->st_probe.sema,
				    &sema, sizeof(sema));
		}
	}
}
#endif				/* ARCH_HAVE_DISABLE_BREAKPOINT */
