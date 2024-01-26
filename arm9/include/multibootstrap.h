#ifndef MULTIBOOTSTRAP_H
#define MULTIBOOTSTRAP_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool multibootstrap_patchFile(const char *path);

#ifdef __cplusplus
}
#endif

#endif // MULTIBOOTSTRAP_H
