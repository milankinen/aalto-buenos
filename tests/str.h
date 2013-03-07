/*
 * str.h
 *
 *  Created on: 7 Mar 2013
 *      Author: matti
 */

#ifndef STR_H_
#define STR_H_

#include <stdarg.h>
#include <stddef.h>
#include "lib/types.h"


/* userland sprintf */
int snprintf(char *str, int size, const  char  *fmt, ...);

/* Prototypes for string manipulation functions */
int stringcmp(const char *str1, const char *str2);
char *stringcopy(char *target, const char *source, int buflen);
int strlen(const char *str);

/* memory copy */
void memcopy(int buflen, void *target, const void *source);

/* memory set */
void memoryset(void *target, char value, int size);

/* convert string to integer */
int atoi(const char *s);


#endif /* STR_H_ */
