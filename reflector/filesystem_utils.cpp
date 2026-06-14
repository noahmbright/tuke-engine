#include "filesystem_utils.h"
#include "reflector.h"

#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Sanitize input dir.
// If the path ends in shaders/, then use the path as is.
// If not, check if a subdirectory called shaders exists.
void validate_in_path(const char *raw_path, char *out_path) {
  struct stat s;
  if (stat(raw_path, &s) != 0) {
    fprintf(stderr, "%s: %s\n", raw_path, strerror(errno));
    exit(1);
  }

  if (!S_ISDIR(s.st_mode)) {
    fprintf(stderr, "%s is not a directory\n", raw_path);
    exit(1);
  }

  size_t len = strlen(raw_path);
  size_t shaders_len = strlen("shaders");
  if (len >= shaders_len && strcmp(raw_path + len - shaders_len, "shaders") == 0) {
    int n = snprintf(out_path, FULL_PATH_BUFFER_LENGTH, "%s", raw_path);
    if (n < 0 || n >= FULL_PATH_BUFFER_LENGTH) {
      fprintf(stderr, "validate_in_path: path too long: %s\n", raw_path);
      exit(1);
    }
    return;
  }

  char shaders_path[FULL_PATH_BUFFER_LENGTH];
  int shaders_path_n = snprintf(shaders_path, sizeof(shaders_path), "%s/shaders", raw_path);
  if (shaders_path_n < 0 || shaders_path_n >= FULL_PATH_BUFFER_LENGTH) {
    fprintf(stderr, "validate_in_path: path too long: %s/shaders\n", raw_path);
    exit(1);
  }

  struct stat shaders_stat;
  if (stat(shaders_path, &shaders_stat) != 0 || !S_ISDIR(shaders_stat.st_mode)) {
    fprintf(stderr, "%s: no shaders/ subdirectory\n", raw_path);
    exit(1);
  }

  int out_n = snprintf(out_path, FULL_PATH_BUFFER_LENGTH, "%s", shaders_path);
  if (out_n < 0 || out_n >= FULL_PATH_BUFFER_LENGTH) {
    fprintf(stderr, "validate_in_path: path too long: %s\n", shaders_path);
    exit(1);
  }
}

void free_shader_to_compile_list(ShaderToCompileList *shader_to_compile_list) {
  for (u32 i = 0; i < shader_to_compile_list->num_shaders; i++) {
    if (shader_to_compile_list != NULL) {
      free((void *)shader_to_compile_list->shaders[i].source);
      free((void *)shader_to_compile_list->shaders[i].name);
      free((void *)shader_to_compile_list->shaders[i].source_path);
      shader_to_compile_list->shaders[i].source = NULL;
    } else {
      fprintf(stderr, "Attemped to free memory from ShaderToCompile when null\n");
    }
  }
}

void push_subdirectory(SubdirectoryList *subdir_list, const char *s) {
  if (subdir_list->num_subdirectories >= MAX_NUM_SUBDIRECTORIES) {
    fprintf(stderr, "push_subdirectory: subdirectory_list exceed maximum capacity\n");
    exit(1);
  }

  char *subdir = subdir_list->subdirectories[subdir_list->num_subdirectories];
  snprintf(subdir, SUBDIRECTORY_PATH_BUFFER_LENGTH, "%s", s);
  subdir_list->num_subdirectories++;
}

void walk_dirs(const char *path, SubdirectoryList *subdirectory_list) {
  DIR *dir = opendir(path);
  if (!dir) {
    fprintf(stderr, "Failed to open %s: %s", path, strerror(errno));
    return;
  }

  struct dirent *directory_entry;
  while ((directory_entry = readdir(dir))) {
    if (strcmp(directory_entry->d_name, ".") == 0 || strcmp(directory_entry->d_name, "..") == 0) {
      continue;
    }

    char full_path[FULL_PATH_BUFFER_LENGTH];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, directory_entry->d_name);

    struct stat st;
    if (stat(full_path, &st) == -1) {
      fprintf(stderr, "stat failed on '%s': %s\n", full_path, strerror(errno));
      continue;
    }

    if (S_ISDIR(st.st_mode)) {
      push_subdirectory(subdirectory_list, full_path);
      walk_dirs(full_path, subdirectory_list);
    }
  }

  closedir(dir);
}

const char *read_file(const char *path, u64 *out_size) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open %s\n", path);
    exit(1);
  }

  if (fseek(fp, 0L, SEEK_END) != 0) {
    perror("fseek");
    exit(1);
  }

  long file_size = ftell(fp);
  if (file_size < 0) {
    perror("ftell");
    exit(1);
  }
  rewind(fp);

  char *buffer = (char *)malloc(file_size + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Failed to allocte buffer for %s\n", path);
    exit(1);
  }

  size_t bytes_read = fread(buffer, sizeof(char), file_size, fp);
  if (bytes_read < (size_t)file_size) {
    fprintf(stderr, "Failed to read correct number of bytes from %s\n", path);
    free(buffer);
    exit(1);
  }

  buffer[bytes_read] = '\0';
  fclose(fp);

  if (out_size) {
    *out_size = (u64)bytes_read;
  }

  return buffer;
}

static const char *scan_token(const char *src, char *out, u32 out_size, char delim) {
  u32 i = 0;
  while (*src != '\0' && *src != delim && i < out_size - 1)
    out[i++] = *src++;
  out[i] = '\0';
  return src;
}

ShaderToCompileList collect_shaders_to_compile(
    const SubdirectoryList *subdirectory_list, const char *shaders_root, const char *output_path
) {
  ShaderToCompileList shader_list;
  memset(&shader_list, 0, sizeof(shader_list));
  bool found_newer_shader = false;

  struct stat out_header_stat;
  memset(&out_header_stat, 0, sizeof(out_header_stat));
  if (stat(output_path, &out_header_stat) != 0) {
    if (errno == ENOENT) {
      printf("%s does not exist: Compiling shaders.\n", output_path);
      shader_list.needs_recompiled = true;
    }
  }

  for (u32 subdir_index = 0; subdir_index < subdirectory_list->num_subdirectories; subdir_index++) {
    const char *subdir_path = subdirectory_list->subdirectories[subdir_index];

    // Skip gen output directories
    const char *last_slash = strrchr(subdir_path, '/');
    const char *last_component = last_slash ? last_slash + 1 : subdir_path;
    if (strcmp(last_component, "gen") == 0) {
      continue;
    }

    // Iterate over regular files
    DIR *subdirectory = opendir(subdir_path);
    if (!subdirectory) {
      fprintf(stderr, "%s(): Failed to open %s: %s", __func__, subdir_path, strerror(errno));
      continue;
    }

    struct dirent *directory_entry;
    while ((directory_entry = readdir(subdirectory))) {
      // Skip . and ..
      if (strcmp(directory_entry->d_name, ".") == 0 || strcmp(directory_entry->d_name, "..") == 0) {
        continue;
      }

      if (shader_list.num_shaders >= MAX_NUM_SHADERS) {
        printf("%s(): Exceeded MAX_NUM_SHADERS\n", __func__);
        break;
      }

      // Full path is e.g. shaders/common/quad.vert.in
      // Sticks together subdirectory path (shaders/common/) and name (name.stage.in)
      char full_path[FULL_PATH_BUFFER_LENGTH];
      int characters_to_copy = snprintf(full_path, sizeof(full_path), "%s/%s", subdir_path, directory_entry->d_name);
      if (characters_to_copy < 0) {
        fprintf(stderr, "%s(): encoding error in snprintf'\n", __func__);
        continue;
      }
      if (characters_to_copy >= FULL_PATH_BUFFER_LENGTH) {
        fprintf(stderr, "%s(): snprintf truncated path %s'\n", __func__, full_path);
        continue;
      }

      struct stat shader_stat;
      if (stat(full_path, &shader_stat) == -1) {
        fprintf(stderr, "stat failed on '%s': %s\n", full_path, strerror(errno));
        continue;
      }

      if (!S_ISREG(shader_stat.st_mode)) {
        continue;
      }

      if (shader_stat.st_mtime > out_header_stat.st_mtime) {
        found_newer_shader = true;
        shader_list.needs_recompiled = true;
      }

      const char *shaders_prefix_trimmed_path = full_path + strlen(shaders_root) + 1;
      const char *cursor = shaders_prefix_trimmed_path;
      const char *last_slash_location = shaders_prefix_trimmed_path;

      while ((cursor - full_path) < FULL_PATH_BUFFER_LENGTH - 1 && *cursor != '\0') {
        if (*cursor == '/') {
          last_slash_location = cursor;
        }
        cursor++;
      }

      const u32 BUFFER_SIZE = 128;
      char token[BUFFER_SIZE];
      cursor = last_slash_location + 1;

      cursor = scan_token(cursor, token, BUFFER_SIZE, '.');
      if (*cursor == '\0') {
        fprintf(stderr, "%s: shader filename %s ended after name: need stage and extension\n", __func__, full_path);
        continue;
      }
      u32 shader_name_length = cursor - shaders_prefix_trimmed_path;
      cursor++;

      cursor = scan_token(cursor, token, BUFFER_SIZE, '.');
      ShaderStage shader_stage;
      if (strcmp(token, "vert") == 0) {
        shader_stage = SHADER_STAGE_VERTEX;
      } else if (strcmp(token, "frag") == 0) {
        shader_stage = SHADER_STAGE_FRAGMENT;
      } else if (strcmp(token, "comp") == 0) {
        shader_stage = SHADER_STAGE_COMPUTE;
      } else if (strcmp(token, "shader") == 0) {
        shader_stage = SHADER_STAGE_COMBINED;
      } else {
        fprintf(stderr, "%s(): got an invalid shader stage %s\n", __func__, token);
        continue;
      }
      if (*cursor == '\0') {
        fprintf(stderr, "%s(): shader filename %s ended after stage: still need extension\n", __func__, full_path);
        continue;
      }
      cursor++;

      cursor = scan_token(cursor, token, BUFFER_SIZE, '.');
      if (*cursor != '\0') {
        fprintf(stderr, "%s(): shader filename %s not finished after extension\n", __func__, full_path);
        continue;
      }
      if (strcmp(token, "in") != 0) {
        fprintf(stderr, "%s(): shader filename %s has invalid extension\n", __func__, full_path);
        continue;
      }

      u64 source_length;
      const char *shader_source = read_file(full_path, &source_length);

      if (shader_source == NULL) {
        fprintf(stderr, "%s(): Failed to get source for %s\n", __func__, full_path);
        continue;
      }

      // Shader name is the sanitized part after the shaders prefix
      // For path/to/something.stage.in, it is path_to_something
      char *shader_name = (char *)malloc((shader_name_length + 1) * sizeof(char));
      if (shader_name == NULL) {
        fprintf(stderr, "Failed to allocate buffer shader name %s\n", shaders_prefix_trimmed_path);
        exit(1);
      }

      for (u32 i = 0; i < shader_name_length; i++) {
        char c = shaders_prefix_trimmed_path[i];
        shader_name[i] = (c == '.' || c == '/') ? '_' : c;
      }
      shader_name[shader_name_length] = '\0';

      shader_list.shaders[shader_list.num_shaders++] = {
          .stage = shader_stage,
          .source = shader_source,
          .source_length = source_length,
          .name = shader_name,
          .name_len = shader_name_length,
          .source_path = strdup(full_path),
      };
    } // nested loop over files in subdir
    closedir(subdirectory);
  } // main loop over subdirectories

  if (!found_newer_shader) {
    printf("Found no shaders newer than %s.\n", output_path);
  }

  return shader_list;
}
