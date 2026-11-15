#ifndef __ELEGANT_DEBUG_8051_H
#define __ELEGANT_DEBUG_8051_H

#include <stdio.h>

#define COLOR_RED      "\033[31m"
#define COLOR_GREEN    "\033[32m"
#define COLOR_YELLOW   "\033[33m"
#define COLOR_BLUE     "\033[34m"
#define CLR            "\033[0m" 

#define INFO_PREFIX    "\033[92m[INFO]\033[0m "
#define ERROR_PREFIX   "\033[91m[ERROR]\033[0m "

#define ELOG_INFO(s)   printf(INFO_PREFIX s CLR "\r\n")
#define ELOG_ERROR(s)  printf(ERROR_PREFIX s CLR "\r\n")

#endif
