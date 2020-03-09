/*
 * compat defines
 *
 * Copyright 1995 by Gray Watson
 *
 * This file is part of the argv library.
 *
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies, and that the name of Gray Watson not be used in advertising
 * or publicity pertaining to distribution of the document or software
 * without specific, written prior permission.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#ifndef __COMPAT_H__
#define __COMPAT_H__

#include "conf.h"			/* for all of the HAVE_* */

/*<<<<<<<<<<  The below prototypes are auto-generated by fillproto */

#if HAVE_STRCHR == 0
/*
 * find CH in STR by searching backwards through the string
 */
extern
char	*strchr(const char *str, int ch);
#endif

#if HAVE_STRCMP == 0
/*
 * returns -1,0,1 on whether STR1 is <,==,> STR2
 */
extern
int	strcmp(const char *str1, const char *str2);
#endif

#if HAVE_STRCPY == 0
/*
 * copies STR2 to STR1.  returns STR1
 */
extern
char	*strcpy(char *str1, const char *str2);
#endif

#if HAVE_STRLEN == 0
/*
 * return the length in characters of STR
 */
extern
int	strlen(const char *str);
#endif

#if HAVE_STRNCMP == 0
/*
 * compare at most LEN chars in STR1 and STR2 and return -1,0,1 or STR1 - STR2
 */
extern
int	strncmp(const char *str1, const char *str2, const int len);
#endif

#if HAVE_STRNCPY == 0
/*
 * copy STR2 to STR1 until LEN or '\0'
 */
extern
char	*strncpy(char *str1, const char *str2, const int len);
#endif

#if HAVE_STRSEP == 0
/*
 * char *strsep
 *
 * DESCRIPTION:
 *
 * This is a function which should be in libc in every Unix.  Grumble.
 * It basically replaces the strtok function because it is reentrant.
 * This tokenizes a string by returning the next token in a string and
 * punching a \0 on the first delimiter character past the token.  The
 * difference from strtok is that you pass in the address of a string
 * pointer which will be shifted allong the buffer being processed.
 * With strtok you passed in a 0L for subsequant calls.  Yeach.
 *
 * This will count the true number of delimiter characters in the string
 * and will return an empty token (one with \0 in the zeroth position)
 * if there are two delimiter characters in a row.
 *
 * Consider the following example:
 *
 * char *tok, *str_p = "1,2,3, hello there ";
 *
 * while (1) { tok = strsep(&str_p, " ,"); if (tok == 0L) { break; } }
 *
 * strsep will return as tokens: "1", "2", "3", "", "hello", "there", "".
 * Notice the two empty "" tokens where there were two delimiter
 * characters in a row ", " and at the end of the string where there
 * was an extra delimiter character.  If you want to ignore these
 * tokens then add a test to see if the first character of the token
 * is \0.
 *
 * RETURNS:
 *
 * Success - Pointer to the next delimited token in the string.
 *
 * Failure - 0L if there are no more tokens.
 *
 * ARGUMENTS:
 *
 * string_p - Pointer to a string pointer which will be searched for
 * delimiters.  \0's will be added to this buffer.
 *
 * delim - List of delimiter characters which separate our tokens.  It
 * does not have to remain constant through all calls across the same
 * string.
 */
extern
char	*strsep(char **string_p, const char *delim);
#endif

/*<<<<<<<<<<   This is end of the auto-generated output from fillproto. */

#endif /* ! __COMPAT_H__ */
