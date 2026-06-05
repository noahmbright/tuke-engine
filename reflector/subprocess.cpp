#include "subprocess.h"
#include "reflector.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int create_tempfile(char *filepath_template) {
  // the Xs are replaced by mkstemp and are required
  const char *base = "/tmp/shXXXXXX";
  if (strlen(base) + 1 > TEMPLATE_FILE_LENGTH) {
    errno = ENAMETOOLONG;
    return -1;
  }
  strcpy(filepath_template, base);
  int fd = mkstemp(filepath_template);
  if (fd >= 0) {
    // Close now; we'll reopen as needed. Also mark CLOEXEC so children don't inherit.
    // cloexec is close on exec
    fcntl(fd, F_SETFD, FD_CLOEXEC);
  }
  return fd;
}

static int read_file_to_heap(const char *path, u8 **out_buf, size_t *out_len) {
  *out_buf = NULL;
  *out_len = 0;

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    close(fd);
    return -1;
  }
  if (st.st_size <= 0) {
    close(fd);
    errno = EINVAL;
    return -1;
  }

  size_t len = (size_t)st.st_size;
  u8 *buf = (u8 *)malloc(len);
  if (!buf) {
    close(fd);
    return -1;
  }

  size_t off = 0;
  while (off < len) {
    ssize_t n = read(fd, buf + off, len - off);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      free(buf);
      close(fd);
      return -1;
    }
    if (n == 0) {
      break; // EOF (unexpected but handled)
    }
    off += (size_t)n;
  }
  close(fd);

  if (off != len) {
    free(buf);
    errno = EIO;
    return -1;
  }

  *out_buf = buf;
  *out_len = len;
  return 0;
}

// Could potentially optimize this with cleverer buffering.
// Could avoid text copy by searching for boundaries and doing fwrite.
static void dump_source_with_line_numbers(const char *source_string) {
  const u32 BUF_SIZE = 1024;
  char buf[BUF_SIZE];

  u32 line_no = 1;
  const char *c = source_string;

  while (*c) {
    u32 i = 0;
    while (*c != '\n' && *c != '\0') {
      assert(i < BUF_SIZE - 1); //  - 1 for null terminator
      buf[i++] = *c++;
    }
    buf[i] = '\0';

    printf("%u %s\n", line_no, buf);
    c += (*c == '\n');
    line_no++;
  }
}

// Scatter.
// Make tempfiles, write GLSL source to them, spawn glslValidator on them in one loop.
static void start_spirv_compilation(CompileJob *jobs, const GLSLSource *sources, u32 num_sources) {
  for (u32 i = 0; i < num_sources; i++) {
    char *glsl_path = jobs[i].glsl_path;
    char *spirv_path = jobs[i].spirv_path;
    jobs[i].glsl_source_str = sources[i].string;

    // Make temp for GLSL source that we will invoke the compiler on.
    int glsl_fd = create_tempfile(glsl_path);
    if (glsl_fd < 0) {
      perror("mkstemp input");
      return;
    }

    // Write GLSL to input file
    const u8 *cur_pos = (const u8 *)sources[i].string;
    u32 bytes_left = sources[i].length;
    while (bytes_left) {
      ssize_t bytes_written = write(glsl_fd, cur_pos, bytes_left);
      if (bytes_written < 0) {
        if (errno == EINTR) {
          continue;
        }

        perror("write GLSL");
        close(glsl_fd);
        unlink(glsl_path);
        return;
      }

      cur_pos += bytes_written;
      bytes_left -= bytes_written;
    }

    // Ensure data reaches disk before compiler reads it (paranoid but safe).
    fsync(glsl_fd);
    close(glsl_fd);

    // Make a tempfile for the compiled bytecode.
    int spirv_fd = create_tempfile(spirv_path);
    close(spirv_fd);
    if (spirv_fd < 0) {
      perror("mkstemp output");
      return;
    }

    // Invoke compiler
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      return;
    }

    if (pid == 0) {
      // we're the child, we are going to invoke the validator
      // Child: exec glslangValidator
      // glslangValidator -S <stage> -V -o <out> <in>
      char *stage_str = (char *)shader_stage_to_string[sources[i].stage];
      char *const argv[] = {
          (char *)"glslangValidator", (char *)"-S", stage_str, (char *)"-V", (char *)"-o", spirv_path, glsl_path, NULL
      };
      execvp("glslangValidator", argv);

      // If we get here, exec failed
      perror("execvp glslangValidator");
      _exit(127);
    } else {
      jobs[i].pid = pid;
    }
  }
}

// Gather
static bool finish_spirv_compilation(const CompileJob *jobs, SpirVBytesArray *bytes_arrays, u32 num_jobs) {
  for (u32 i = 0; i < num_jobs; i++) {
    int status = 0;
    // TODO reap zombies properly
    if (waitpid(jobs[i].pid, &status, 0) < 0) {
      perror("waitpid");
      return false;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
      fprintf(stderr, "glslangValidator exited %d\n", WEXITSTATUS(status));
      dump_source_with_line_numbers(jobs[i].glsl_source_str);
      return false;
    }
    if (!WIFEXITED(status)) {
      fprintf(stderr, "glslangValidator killed by signal %d\n", WTERMSIG(status));
      return false;
    }

    const char *glsl_path = jobs[i].glsl_path;
    const char *spirv_path = jobs[i].spirv_path;

    // Read SPIR-V
    u8 *bytes = NULL;
    size_t len = 0;
    if (read_file_to_heap(spirv_path, &bytes, &len) != 0) {
      perror("read SPIR-V");
      unlink(glsl_path);
      unlink(spirv_path);
      return false;
    }

    // Validate 4-byte word alignment
    if ((len & 3u) != 0u) {
      fprintf(stderr, "SPIR-V length not multiple of 4: %zu\n", len);
      free(bytes);
      unlink(glsl_path);
      unlink(spirv_path);
      return false;
    }

    // Cleanup temp files
    unlink(glsl_path);
    unlink(spirv_path);

    // Success — hand ownership to caller
    bytes_arrays[i].bytes = bytes;
    bytes_arrays[i].length = (u32)len;
  }

  return true;
}

bool compile_to_spirv(const GLSLSource *sources, SpirVBytesArray *bytes_arrays, u32 num_sources) {
  CompileJob jobs[MAX_NUM_SHADERS];
  start_spirv_compilation(jobs, sources, num_sources);
  bool success = finish_spirv_compilation(jobs, bytes_arrays, num_sources);
  return success;
}
