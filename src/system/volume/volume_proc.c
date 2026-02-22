#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "system/volume/volume_proc.h"

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define OSD_WPCTL_IO_TIMEOUT_MS 1500
#define OSD_WPCTL_WAIT_POLL_MS 25

extern char **environ;

// Function pointers are replaceable for deterministic failure-path tests
static OSDVolumePollFn g_osd_volume_poll_fn = poll;
static OSDVolumeWaitpidFn g_osd_volume_waitpid_fn = waitpid;
static OSDVolumeKillFn g_osd_volume_kill_fn = kill;

// Status helper ensures every failure path returns structured context
static void set_status(OSDVolumeProcStatus *status, OSDVolumeProcError error, int exit_code, int term_signal,
                       int spawn_error) {
  if (status == NULL) {
    return;
  }

  status->error = error;
  status->exit_code = exit_code;
  status->term_signal = term_signal;
  status->spawn_error = spawn_error;
}

void osd_volume_proc_set_poll_fn(OSDVolumePollFn poll_fn) {
  // Null restores real libc poll wrapper
  if (poll_fn == NULL) {
    g_osd_volume_poll_fn = poll;
    return;
  }

  g_osd_volume_poll_fn = poll_fn;
}

void osd_volume_proc_set_waitpid_fn(OSDVolumeWaitpidFn waitpid_fn) {
  // Null restores real libc waitpid wrapper
  if (waitpid_fn == NULL) {
    g_osd_volume_waitpid_fn = waitpid;
    return;
  }

  g_osd_volume_waitpid_fn = waitpid_fn;
}

void osd_volume_proc_set_kill_fn(OSDVolumeKillFn kill_fn) {
  // Null restores real libc kill wrapper
  if (kill_fn == NULL) {
    g_osd_volume_kill_fn = kill;
    return;
  }

  g_osd_volume_kill_fn = kill_fn;
}

// Waits for child exit with a deadline then force kills on timeout
static bool wait_for_child_process_with_timeout(pid_t child_pid, int *status, unsigned int timeout_ms,
                                                bool *timed_out) {
  unsigned int waited_ms = 0U;

  if (status == NULL || timed_out == NULL) {
    return false;
  }

  *timed_out = false;

  // Phase one waits for graceful child completion with a bounded deadline
  while (waited_ms <= timeout_ms) {
    pid_t wait_result = g_osd_volume_waitpid_fn(child_pid, status, WNOHANG);

    if (wait_result == child_pid) {
      return true;
    }
    if (wait_result < 0) {
      if (errno == EINTR) {
        // EINTR still consumes deadline budget to keep worst-case bounded
        if (waited_ms == timeout_ms) {
          break;
        }
        (void)g_osd_volume_poll_fn(NULL, 0U, (int)OSD_WPCTL_WAIT_POLL_MS);
        waited_ms += OSD_WPCTL_WAIT_POLL_MS;
        continue;
      }
      return false;
    }

    if (waited_ms == timeout_ms) {
      break;
    }

    (void)g_osd_volume_poll_fn(NULL, 0U, (int)OSD_WPCTL_WAIT_POLL_MS);
    waited_ms += OSD_WPCTL_WAIT_POLL_MS;
  }

  *timed_out = true;
  // Child ignored deadline so force terminate and attempt bounded reap
  if (g_osd_volume_kill_fn(child_pid, SIGKILL) < 0 && errno != ESRCH) {
    return false;
  }

  waited_ms = 0U;
  // Phase two reaps killed child with a second bounded wait window
  while (waited_ms <= timeout_ms) {
    pid_t wait_result = g_osd_volume_waitpid_fn(child_pid, status, WNOHANG);

    if (wait_result == child_pid) {
      return true;
    }
    if (wait_result < 0) {
      if (errno == EINTR) {
        // Keep timeout accounting consistent under signal pressure
        if (waited_ms == timeout_ms) {
          break;
        }
        (void)g_osd_volume_poll_fn(NULL, 0U, (int)OSD_WPCTL_WAIT_POLL_MS);
        waited_ms += OSD_WPCTL_WAIT_POLL_MS;
        continue;
      }
      return false;
    }

    if (waited_ms == timeout_ms) {
      break;
    }

    (void)g_osd_volume_poll_fn(NULL, 0U, (int)OSD_WPCTL_WAIT_POLL_MS);
    waited_ms += OSD_WPCTL_WAIT_POLL_MS;
  }

  return false;
}

// Maps raw wait status into normalized process error results
static bool finalize_child_exit_status(pid_t child_pid, OSDVolumeProcStatus *status) {
  int wait_status = 0;
  bool timed_out = false;

  if (!wait_for_child_process_with_timeout(child_pid, &wait_status, OSD_WPCTL_IO_TIMEOUT_MS, &timed_out)) {
    set_status(status, OSD_VOLUME_PROC_ERR_WAIT, 0, 0, 0);
    return false;
  }

  if (timed_out) {
    // Timeout classification survives even if child eventually exits
    set_status(status, OSD_VOLUME_PROC_ERR_READ_TIMEOUT, 0, 0, 0);
    return false;
  }

  // Signal and non-zero exit cases are propagated with details
  if (WIFSIGNALED(wait_status)) {
    set_status(status, OSD_VOLUME_PROC_ERR_EXIT_SIGNALED, 0, WTERMSIG(wait_status), 0);
    return false;
  }

  if (!WIFEXITED(wait_status)) {
    set_status(status, OSD_VOLUME_PROC_ERR_EXIT_UNEXPECTED, 0, 0, 0);
    return false;
  }

  if (WEXITSTATUS(wait_status) != 0) {
    set_status(status, OSD_VOLUME_PROC_ERR_EXIT_NONZERO, WEXITSTATUS(wait_status), 0, 0);
    return false;
  }

  set_status(status, OSD_VOLUME_PROC_ERR_NONE, 0, 0, 0);
  return true;
}

// Executes wpctl directly without a shell and captures a single stdout line
bool osd_volume_run_wpctl_get_volume_line(const char *wpctl_path, char *line, size_t line_size,
                                          OSDVolumeProcStatus *out_status) {
  // Pipe carries child stdout and leaves stderr attached to parent stream
  int pipe_fds[2] = {-1, -1};
  pid_t child_pid = -1;
  size_t line_used = 0U;
  bool line_truncated = false;
  posix_spawn_file_actions_t spawn_actions;
  int spawn_result = 0;
  char *const wpctl_argv[] = {"wpctl", "get-volume", "@DEFAULT_AUDIO_SINK@", NULL};

  if (out_status == NULL || wpctl_path == NULL || line == NULL || line_size == 0U) {
    set_status(out_status, OSD_VOLUME_PROC_ERR_INVALID_ARG, 0, 0, 0);
    return false;
  }

  set_status(out_status, OSD_VOLUME_PROC_ERR_NONE, 0, 0, 0);

  if (pipe(pipe_fds) != 0) {
    set_status(out_status, OSD_VOLUME_PROC_ERR_PIPE, 0, 0, 0);
    return false;
  }

  if (posix_spawn_file_actions_init(&spawn_actions) != 0) {
    (void)close(pipe_fds[0]);
    (void)close(pipe_fds[1]);
    set_status(out_status, OSD_VOLUME_PROC_ERR_SPAWN_ACTIONS_INIT, 0, 0, 0);
    return false;
  }

  if (posix_spawn_file_actions_addclose(&spawn_actions, pipe_fds[0]) != 0 ||
      posix_spawn_file_actions_adddup2(&spawn_actions, pipe_fds[1], STDOUT_FILENO) != 0 ||
      posix_spawn_file_actions_addclose(&spawn_actions, pipe_fds[1]) != 0) {
    (void)posix_spawn_file_actions_destroy(&spawn_actions);
    (void)close(pipe_fds[0]);
    (void)close(pipe_fds[1]);
    set_status(out_status, OSD_VOLUME_PROC_ERR_SPAWN_ACTIONS_PREP, 0, 0, 0);
    return false;
  }

  // posix_spawn avoids shell invocation and keeps argument handling explicit
  spawn_result = posix_spawn(&child_pid, wpctl_path, &spawn_actions, NULL, wpctl_argv, environ);
  (void)posix_spawn_file_actions_destroy(&spawn_actions);
  if (spawn_result != 0) {
    (void)close(pipe_fds[0]);
    (void)close(pipe_fds[1]);
    set_status(out_status, OSD_VOLUME_PROC_ERR_SPAWN, 0, 0, spawn_result);
    return false;
  }

  (void)close(pipe_fds[1]);
  line[0] = '\0';

  // Read loop stops at first newline to match single-line parser contract
  while (line_used < (line_size - 1U)) {
    struct pollfd poll_fd;
    int poll_result = 0;
    ssize_t bytes_read = 0;
    size_t offset = 0U;

    poll_fd.fd = pipe_fds[0];
    poll_fd.events = POLLIN | POLLHUP;
    poll_fd.revents = 0;

    do {
      poll_result = g_osd_volume_poll_fn(&poll_fd, 1U, OSD_WPCTL_IO_TIMEOUT_MS);
    } while (poll_result < 0 && errno == EINTR);

    if (poll_result < 0) {
      (void)close(pipe_fds[0]);
      // Keep wait failure status when cleanup fails
      if (!finalize_child_exit_status(child_pid, out_status)) {
        return false;
      }
      set_status(out_status, OSD_VOLUME_PROC_ERR_POLL, 0, 0, 0);
      return false;
    }
    if (poll_result == 0) {
      (void)close(pipe_fds[0]);
      // Timeout diagnostics are canonicalized by status mapping
      if (!finalize_child_exit_status(child_pid, out_status)) {
        return false;
      }
      set_status(out_status, OSD_VOLUME_PROC_ERR_READ_TIMEOUT, 0, 0, 0);
      return false;
    }
    if ((poll_fd.revents & (POLLERR | POLLNVAL)) != 0) {
      (void)close(pipe_fds[0]);
      if (!finalize_child_exit_status(child_pid, out_status)) {
        return false;
      }
      set_status(out_status, OSD_VOLUME_PROC_ERR_POLL_STATE, 0, 0, 0);
      return false;
    }

    bytes_read = read(pipe_fds[0], line + line_used, line_size - 1U - line_used);
    if (bytes_read < 0) {
      if (errno == EINTR) {
        // Interrupted reads retry without resetting buffered bytes
        continue;
      }
      (void)close(pipe_fds[0]);
      if (!finalize_child_exit_status(child_pid, out_status)) {
        return false;
      }
      set_status(out_status, OSD_VOLUME_PROC_ERR_READ, 0, 0, 0);
      return false;
    }

    if (bytes_read == 0) {
      break;
    }

    while (offset < (size_t)bytes_read) {
      if (line[line_used + offset] == '\n') {
        // Newline completes one parser-ready output record
        line_used += offset + 1U;
        goto read_complete;
      }
      offset++;
    }

    line_used += (size_t)bytes_read;
  }

  if (line_used == (line_size - 1U)) {
    line_truncated = true;
  }

read_complete:
  // Hard cap remains enforced even when newline was not observed
  if (line_used >= line_size) {
    line_used = line_size - 1U;
    line_truncated = true;
  }

  line[line_used] = '\0';
  (void)close(pipe_fds[0]);

  // Child exit is checked after stream close to avoid zombie processes
  // Truncation keeps priority over non-zero/signaled exits because those can
  // be a downstream effect of closing the read side early after hard cap hit
  if (line_truncated) {
    if (!finalize_child_exit_status(child_pid, out_status)) {
      if (out_status->error == OSD_VOLUME_PROC_ERR_WAIT ||
          out_status->error == OSD_VOLUME_PROC_ERR_READ_TIMEOUT) {
        return false;
      }
    }

    set_status(out_status, OSD_VOLUME_PROC_ERR_OUTPUT_TRUNCATED, 0, 0, 0);
    return false;
  }

  if (!finalize_child_exit_status(child_pid, out_status)) {
    return false;
  }

  // Empty output fails before parser stage
  if (line_used == 0U) {
    set_status(out_status, OSD_VOLUME_PROC_ERR_OUTPUT_EMPTY, 0, 0, 0);
    return false;
  }

  return true;
}
