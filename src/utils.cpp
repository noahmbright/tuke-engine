#include "utils.h"
#include "camera.h"

const char *read_file(const char *path, unsigned long *size) {
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open %s\n", path);
    exit(1);
  }

  fseek(fp, 0L, SEEK_END);
  long file_size = ftell(fp);
  rewind(fp);
  char *buffer = (char *)malloc(file_size + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Failed to allocte buffer for %s\n", path);
    exit(1);
  }

  long bytes_read = fread(buffer, sizeof(char), file_size, fp);
  if (bytes_read < file_size) {
    fprintf(stderr, "Failed to read correct number of bytes from %s\n", path);
    exit(1);
  }

  buffer[bytes_read] = '\0';
  fclose(fp);

  if (size)
    *size = bytes_read;
  return buffer;
}
