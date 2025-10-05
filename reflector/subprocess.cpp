#include "subprocess.h"
#include "codegen.h"
#include "reflector.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>

int write_all(int fd, const void *buffer, u32 length) {
  const u8 *current_position = (const u8 *)buffer;
  while (length) {
    ssize_t bytes_written = write(fd, current_position, length);
    if (bytes_written < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }

    current_position += bytes_written;
    length -= bytes_written;
  }

  return 0;
}

int read_bytes_into_array(SpirVBytesArray *out_spirv_bytes) {
  (void)out_spirv_bytes;
  return 0;
}

int create_tempfile(char *filepath_template, u32 template_cap) {
  // the Xs are replaced by mkstemp and are required
  const char *base = "/tmp/shXXXXXX";
  if (strlen(base) + 1 > template_cap) {
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

int invoke_glslValidator(const char *glsl_in_path, const char *spirv_out_path, ShaderStage stage) {
  // glslangValidator -S <stage> -V -o <out> <in>
  pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }

  const char *stage_string = shader_stage_to_string[stage];

  if (pid == 0) {
    // we're the child, we are going to invoke the validator
    // Child: exec glslangValidator
    char *const argv[] = {(char *)"glslangValidator",
                          (char *)"-S",
                          (char *)stage_string,
                          (char *)"-V",
                          (char *)"-o",
                          (char *)spirv_out_path,
                          (char *)glsl_in_path,
                          NULL};
    execvp("glslangValidator", argv);
    // If we get here, exec failed
    perror("execvp glslangValidator");
    _exit(127);
  } else {
    // Parent: wait
    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
      return -1;
    if (WIFEXITED(status)) {
      return WEXITSTATUS(status);
    }
    // Abnormal termination (signal, etc.)
    return -1;
  }
}

// TODO copied from gpt, must review
static int read_file_to_heap(const char *path, u8 **out_buf, size_t *out_len) {
  *out_buf = NULL;
  *out_len = 0;

  int fd = open(path, O_RDONLY);
  if (fd < 0)
    return -1;

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
      if (errno == EINTR)
        continue;
      free(buf);
      close(fd);
      return -1;
    }
    if (n == 0)
      break; // EOF (unexpected but handled)
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

SpirVBytesArray compile_vulkan_source_to_glsl(GLSLSource glsl_source, ShaderStage stage) {
  SpirVBytesArray bytes_array;
  bytes_array.length = 0;
  bytes_array.bytes = NULL;

  // algorithm:
  // 1) write glsl to glsl source tempfile
  // 2) invoke glslandValidator on the glsl source tempfile, writing a spirv bytes tempfile
  //    - action for me is invoke glsl, glsl does the writing
  // 3) read the compiled bytes back from the spirv bytes tempfile, return allocated buffer and length

  const u32 template_file_length = 64;

  // 1) write glsl to tempfile
  // make temp
  char glsl_source_path[template_file_length];
  int glsl_source_temp_fd = create_tempfile(glsl_source_path, template_file_length);
  if (glsl_source_temp_fd < 0) {
    perror("mkstemp input");
    return bytes_array;
  }

  // write temp
  int write_glsl_source_rc = write_all(glsl_source_temp_fd, glsl_source.string, glsl_source.length);
  // Write source to input file
  if (write_glsl_source_rc != 0) {
    perror("write GLSL");
    close(glsl_source_temp_fd);
    unlink(glsl_source_path);
    return bytes_array;
  }
  // Ensure data reaches disk before compiler reads it (paranoid but safe).
  fsync(glsl_source_temp_fd);
  close(glsl_source_temp_fd);

  // 2) compile with validator
  // make a tempfile where the compiled bytecode goes
  // make temp
  char spirv_bytecode_path[template_file_length];
  int spirv_bytecode_temp_fd = create_tempfile(spirv_bytecode_path, template_file_length);
  if (spirv_bytecode_temp_fd < 0) {
    perror("mkstemp output");
    return bytes_array;
  }

  // invoke compiler
  int glslangValidator_rc = invoke_glslValidator(glsl_source_path, spirv_bytecode_path, stage);
  if (glslangValidator_rc != 0) {
    printf("Shader compilation failed, source dump:\n%s\n", glsl_source.string);
    return bytes_array;
  }

  // TODO hereafter copied and needs reviewed
  // Read SPIR-V
  u8 *bytes = NULL;
  size_t len = 0;
  if (read_file_to_heap(spirv_bytecode_path, &bytes, &len) != 0) {
    perror("read SPIR-V");
    unlink(glsl_source_path);
    unlink(spirv_bytecode_path);
    return bytes_array;
  }

  // Validate 4-byte word alignment
  if ((len & 3u) != 0u) {
    fprintf(stderr, "SPIR-V length not multiple of 4: %zu\n", len);
    free(bytes);
    unlink(glsl_source_path);
    unlink(spirv_bytecode_path);
    return bytes_array;
  }

  // Cleanup temp files
  unlink(glsl_source_path);
  unlink(spirv_bytecode_path);

  // Success — hand ownership to caller
  bytes_array.bytes = bytes;
  bytes_array.length = (u32)len;

  return bytes_array;
}
