/*
 * imports.h -- .so import resolution for LSWTCS
 */

#ifndef __IMPORTS_H__
#define __IMPORTS_H__

#include "so_util.h"
#include <stdio.h>

extern FILE *stderr_fake;
extern DynLibFunction dynlib_functions[];
extern size_t dynlib_numfunctions;

void glUniform1fv_wrap(int location, int count, const float *value);
void glUniform2fv_wrap(int location, int count, const float *value);
void glUniform3fv_wrap(int location, int count, const float *value);
void glUniform4fv_wrap(int location, int count, const float *value);

#endif
