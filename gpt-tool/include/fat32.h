#ifndef FAT32_H
#define FAT32_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

bool write_esp(FILE* image);
bool add_path_to_esp(char *path, FILE *image);

#endif