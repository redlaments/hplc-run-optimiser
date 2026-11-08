#ifndef UTIL_H
#define UTIL_H

#include "logic.h"
int tokenize_str1(char*str,char*argv[],int argv_size,char delimiter);
char*action_str(enum selectivity_action aaction,void*adata);
char*action_arg_str(struct action_arg arg);

#endif
