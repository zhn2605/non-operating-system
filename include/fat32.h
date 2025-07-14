#ifndef FAT32_H
#define FAT32_H

#include <stdio.h>
#include <stdbool.h>

bool write_esp(FILE* image);
bool add_path_to_esp(void);

#endif