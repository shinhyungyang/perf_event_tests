#pragma once

extern unsigned char exit_reason;

enum exit_reasons {
	STILL_RUNNING = 0,
	EXIT_NO_SYSCALLS_ENABLED = 1,
	EXIT_REACHED_COUNT = 2,
	EXIT_NO_FDS = 3,
	EXIT_LOST_PID_SLOT = 4,
	EXIT_PID_OUT_OF_RANGE = 5,
	EXIT_SIGINT = 6,
	EXIT_KERNEL_TAINTED = 7,
	EXIT_SHM_CORRUPTION = 8,
	EXIT_REPARENT_PROBLEM = 9,
	EXIT_NO_FILES = 10,
	EXIT_MAIN_DISAPPEARED = 11,
	EXIT_UID_CHANGED = 12,
	EXIT_FD_INIT_FAILURE = 13,
	EXIT_FORK_FAILURE = 14,

	NUM_EXIT_REASONS = 15
};
