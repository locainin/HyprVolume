#ifndef HYPRVOLUME_SYSTEM_VOLUME_PROC_H
#define HYPRVOLUME_SYSTEM_VOLUME_PROC_H

#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

typedef enum {
  // Command execution and read succeeded
  OSD_VOLUME_PROC_ERR_NONE = 0,
  // Caller passed null or invalid buffers
  OSD_VOLUME_PROC_ERR_INVALID_ARG,
  // Pipe creation failed
  OSD_VOLUME_PROC_ERR_PIPE,
  // posix_spawn file action init failed
  OSD_VOLUME_PROC_ERR_SPAWN_ACTIONS_INIT,
  // posix_spawn file action setup failed
  OSD_VOLUME_PROC_ERR_SPAWN_ACTIONS_PREP,
  // posix_spawn failed and spawn_error is set
  OSD_VOLUME_PROC_ERR_SPAWN,
  // poll call failed
  OSD_VOLUME_PROC_ERR_POLL,
  // Read phase timed out
  OSD_VOLUME_PROC_ERR_READ_TIMEOUT,
  // poll returned invalid descriptor state
  OSD_VOLUME_PROC_ERR_POLL_STATE,
  // read call failed
  OSD_VOLUME_PROC_ERR_READ,
  // Output exceeded line buffer
  OSD_VOLUME_PROC_ERR_OUTPUT_TRUNCATED,
  // Output stream was empty
  OSD_VOLUME_PROC_ERR_OUTPUT_EMPTY,
  // waitpid or forced reap failed
  OSD_VOLUME_PROC_ERR_WAIT,
  // Child did not exit via normal or signaled path
  OSD_VOLUME_PROC_ERR_EXIT_UNEXPECTED,
  // Child ended due to signal and term_signal is set
  OSD_VOLUME_PROC_ERR_EXIT_SIGNALED,
  // Child exited non-zero and exit_code is set
  OSD_VOLUME_PROC_ERR_EXIT_NONZERO
} OSDVolumeProcError;

typedef struct {
  // Process phase result classification
  OSDVolumeProcError error;
  // Child exit code when error is OSD_VOLUME_PROC_ERR_EXIT_NONZERO
  int exit_code;
  // Child signal number when error is OSD_VOLUME_PROC_ERR_EXIT_SIGNALED
  int term_signal;
  // posix_spawn error code when error is OSD_VOLUME_PROC_ERR_SPAWN
  int spawn_error;
} OSDVolumeProcStatus;

// Test seam wrappers for poll, waitpid, and kill behavior
typedef int (*OSDVolumePollFn)(struct pollfd *fds, nfds_t nfds, int timeout_ms);
typedef pid_t (*OSDVolumeWaitpidFn)(pid_t pid, int *status, int options);
typedef int (*OSDVolumeKillFn)(pid_t pid, int signal_num);

// Executes wpctl get-volume and reads one stdout line into caller buffer
bool osd_volume_run_wpctl_get_volume_line(const char *wpctl_path, char *line, size_t line_size,
                                          OSDVolumeProcStatus *out_status);

// Passing null restores the default libc-backed wrapper
void osd_volume_proc_set_poll_fn(OSDVolumePollFn poll_fn);
void osd_volume_proc_set_waitpid_fn(OSDVolumeWaitpidFn waitpid_fn);
void osd_volume_proc_set_kill_fn(OSDVolumeKillFn kill_fn);

#endif
