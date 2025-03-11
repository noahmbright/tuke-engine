#pragma once
#include <stdio.h>
#include <stdlib.h>

const char *read_file(const char *path, unsigned long *size = nullptr);
unsigned load_texture(const char *path);
