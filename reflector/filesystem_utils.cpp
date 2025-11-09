#include "filesystem_utils.h"
#include "reflector.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void free_shader_to_compile_list(ShaderToCompileList *shader_to_compile_list) {
  for (u32 i = 0; i < shader_to_compile_list->num_shaders; i++) {
    if (shader_to_compile_list != NULL) {
      free((void *)shader_to_compile_list->shaders[i].source);
      shader_to_compile_list->shaders[i].source = NULL;
    } else {
      fprintf(stderr, "Attemped to free memory from ShaderToCompile when null\n");
    }
  }
}

void push_subdirectory(SubdirectoryList *subdirectory_list, const char *s) {
  if (subdirectory_list->num_subdirectories >= MAX_NUM_SUBDIRECTORIES) {
    fprintf(stderr, "push_subdirectory: subdirectory_list exceed maximum capacity\n");
    exit(1);
  }

  snprintf(subdirectory_list->subdirectories[subdirectory_list->num_subdirectories], SUBDIRECTORY_PATH_BUFFER_LENGTH,
           "%s", s);

  subdirectory_list->num_subdirectories++;
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

ShaderToCompileList collect_shaders_to_compile(const SubdirectoryList *subdirectory_list) {
  ShaderToCompileList shader_to_compile_list;
  memset(&shader_to_compile_list, 0, sizeof(shader_to_compile_list));
  bool found_newer_shader = false;

  struct stat out_header_stat;
  if (stat(REFLECTOR_OUTPUT_FILE_PATH, &out_header_stat) != 0) {
    if (errno == ENOENT) {
      printf("%s does not exist, will recompile shaders.\n", REFLECTOR_OUTPUT_FILE_PATH);
      shader_to_compile_list.needs_recompiled = true;
    }
  }

  for (u32 subdirectory_index = 0; subdirectory_index < subdirectory_list->num_subdirectories; subdirectory_index++) {

    const char *subdirectory_path = subdirectory_list->subdirectories[subdirectory_index];

    // will probably remove gen subdirectories, but enforce skipping for now
    // this loop moves cursor one past the end to the null terminator
    const char *cursor = subdirectory_path;
    while (*cursor != '\0' && (cursor - subdirectory_path) < SUBDIRECTORY_PATH_BUFFER_LENGTH) {
      cursor++;
    }

    if (cursor - subdirectory_path >= 3 && strncmp(cursor - 3, "gen", 3) == 0) {
      // printf("collect_shaders_to_compile: skipping gen directory %s\n", subdirectory_path);
      continue;
    }

    // skipped gen, iterate over regular files
    DIR *subdirectory = opendir(subdirectory_path);
    if (!subdirectory) {
      fprintf(stderr, "collect_shaders_to_compile: Failed to open %s: %s", subdirectory_path, strerror(errno));
      continue;
    }

    struct dirent *directory_entry;
    while ((directory_entry = readdir(subdirectory))) {

      if (strcmp(directory_entry->d_name, ".") == 0 || strcmp(directory_entry->d_name, "..") == 0) {
        continue;
      }

      if (shader_to_compile_list.num_shaders >= MAX_NUM_SHADERS) {
        printf("collect_shaders_to_compile: Exceeded MAX_NUM_SHADERS\n");
        break;
      }
      ShaderToCompile *shader_to_compile = &shader_to_compile_list.shaders[shader_to_compile_list.num_shaders];

      // full path is e.g. shaders/common/quad.vert.in
      // sticks together subdirectory path (shaders/common/) and name
      // (name.stage.in)
      char full_path[FULL_PATH_BUFFER_LENGTH];
      int characters_to_copy =
          snprintf(full_path, sizeof(full_path), "%s/%s", subdirectory_path, directory_entry->d_name);
      if (characters_to_copy < 0) {
        fprintf(stderr, "collect_shaders_to_compile: encoding error in snprintf'\n");
        continue;
      }
      if (characters_to_copy >= FULL_PATH_BUFFER_LENGTH) {
        fprintf(stderr, "collect_shaders_to_compile: snprintf truncated path %s'\n", full_path);
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
        shader_to_compile_list.needs_recompiled = true;
      }

      // points into my stack buffer here, can be safe
      const char *shaders_prefix_trimmed_path = full_path;
      if (strncmp(full_path, "shaders/", strlen("shaders/")) != 0) {
        fprintf(stderr, "collect_shaders_to_compile: Not parsing a "
                        "project_root/shaders directory, exiting\n");
        closedir(subdirectory);
        exit(1);
      }

      // skip shaders prefix
      shaders_prefix_trimmed_path += strlen("shaders/");

      // get filename - check for windows backslash vs forward slash
      cursor = shaders_prefix_trimmed_path;
      // last slash and cursor now point into full_path stack buffer
      const char *last_slash_location = shaders_prefix_trimmed_path;
      while ((cursor - full_path) < FULL_PATH_BUFFER_LENGTH - 1 && *cursor != '\0') {
        if (*cursor == '/') {
          last_slash_location = cursor;
        }
        cursor++;
      }

      const u32 shader_name_buffer_size = 128;
      const u32 shader_stage_buffer_size = 8;
      const u32 shader_extension_buffer_size = 8;
      char shader_name_buffer[shader_name_buffer_size];
      char shader_stage_buffer[shader_stage_buffer_size];
      char shader_extension_buffer[shader_extension_buffer_size];
      memset(shader_name_buffer, 0, sizeof(shader_name_buffer));
      memset(shader_stage_buffer, 0, sizeof(shader_stage_buffer));
      memset(shader_extension_buffer, 0, sizeof(shader_extension_buffer));
      cursor = last_slash_location + 1;

      for (u32 i = 0; i < shader_name_buffer_size; i++) {
        if (*cursor == '.' || *cursor == '\0' || cursor - full_path >= FULL_PATH_BUFFER_LENGTH - 1) {
          break;
        }

        shader_name_buffer[i] = *cursor;
        cursor++;
      }
      if (*cursor == '\0') {
        fprintf(stderr,
                "collect_shaders_to_compile: shader filename %s ended after name, stil needed stage and extension\n",
                full_path);
        continue;
      } else {
        cursor++;
      }
      // printf("shadername : %s\n", shader_name_buffer);

      for (u32 i = 0; i < shader_stage_buffer_size; i++) {
        if (*cursor == '.' || *cursor == '\0' || cursor - full_path >= FULL_PATH_BUFFER_LENGTH - 1) {
          break;
        }

        shader_stage_buffer[i] = *cursor;
        cursor++;
      }
      if (*cursor == '\0') {
        fprintf(stderr, "collect_shaders_to_compile: shader filename %s ended after stage, still needed extension\n",
                full_path);
        continue;
      } else {
        cursor++;
      }
      // printf("shader stage : %s\n", shader_stage_buffer);

      for (u32 i = 0; i < shader_extension_buffer_size; i++) {
        if (*cursor == '.' || *cursor == '\0' || cursor - full_path >= FULL_PATH_BUFFER_LENGTH - 1) {
          break;
        }

        shader_extension_buffer[i] = *cursor;
        cursor++;
      }
      if (*cursor != '\0') {
        fprintf(stderr, "collect_shaders_to_compile: shader filename %s not finished after extension\n", full_path);
        continue;
      }
      // printf("shader stage : %s\n", shader_extension_buffer);
      if (strcmp(shader_extension_buffer, "in") != 0) {
        fprintf(stderr, "collect_shaders_to_compile: shader filename %s has invalid extension\n", full_path);
        continue;
      }

      ShaderStage shader_stage;
      if (strcmp(shader_stage_buffer, "vert") == 0) {
        shader_stage = SHADER_STAGE_VERTEX;
      } else if (strcmp(shader_stage_buffer, "frag") == 0) {
        shader_stage = SHADER_STAGE_FRAGMENT;
      } else if (strcmp(shader_stage_buffer, "comp") == 0) {
        shader_stage = SHADER_STAGE_COMPUTE;
      } else {
        fprintf(stderr, "collect_shaders_to_compile: got an invalid shader stage %s\n", shader_stage_buffer);
        continue;
      }

      const char *shader_source = read_file(full_path, &shader_to_compile->source_length);

      if (shader_source == NULL) {
        fprintf(stderr, "collect_shaders_to_compile: Failed to get source for %s\n", full_path);
        continue;
      }

      // shader name is the sanitized part after the shaders prefix
      // if we got here, then the we have a valid file path of the form
      // path/to/something.stage.in
      // discard the .in and turn / and . to _
      u32 shader_name_length = strlen(shaders_prefix_trimmed_path) - 3;
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

      shader_to_compile->source = shader_source;
      shader_to_compile->stage = shader_stage;
      shader_to_compile->name = shader_name;
      shader_to_compile->name_length = shader_name_length;
      shader_to_compile_list.num_shaders++;
    } // nested loop over files in subdir
    closedir(subdirectory);
  } // main loop over subdirectories

  if (!found_newer_shader) {
    printf("Found no shaders newer than %s.\n", REFLECTOR_OUTPUT_FILE_PATH);
  }

  return shader_to_compile_list;
}
