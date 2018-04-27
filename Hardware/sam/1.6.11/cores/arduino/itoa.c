/*
  Copyright (c) 2011 Arduino.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "itoa.h"
#include <string.h>

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus

#if 0
/* reverse:  reverse string s in place */
static void reverse( char s[] )
{
  int i, j ;
  char c ;

  for ( i = 0, j = strlen(s)-1 ; i < j ; i++, j-- )
  {
    c = s[i] ;
    s[i] = s[j] ;
    s[j] = c ;
  }
}

/* itoa:  convert n to characters in s */
extern void itoa( int n, char s[] )
{
  int i, sign ;

  if ( (sign = n) < 0 )  /* record sign */
  {
    n = -n;          /* make n positive */
  }

  i = 0;
  do
  {       /* generate digits in reverse order */
    s[i++] = n % 10 + '0';   /* get next digit */
  } while ((n /= 10) > 0) ;     /* delete it */

  if (sign < 0 )
  {
    s[i++] = '-';
  }

  s[i] = '\0';

  reverse( s ) ;
}

#else

extern char* itoa( int value, char *string, int radix )
{
  return ltoa( value, string, radix ) ;
}

// optimized for low stack usage
extern char* ltoa( long value, char *string, int radix )
{
	char *pbuffer = string;
	unsigned char	negative = 0;
	unsigned long	i, len;

	/* No support for unusual radixes. */
	if (radix > 36 || radix <= 1) return 0 ;

	if (value < 0) {
		negative = 1;
		value = -value;
	}

	/* This builds the string back to front ... */
	do {
		unsigned char digit = value % radix;
		*(pbuffer++) = (digit < 10 ? '0' + digit : 'a' + digit - 10);
		value /= radix;
	} while (value > 0);

//	for (i = (pbuffer - string); i < zero_pad; i++)	*(pbuffer++) = '0';

	if (negative)
		*(pbuffer++) = '-';

	*(pbuffer) = '\0';

	/* ... now we reverse it (could do it recursively but will
	 * conserve the stack space) */
	len = (pbuffer - string);
	for (i = 0; i < len / 2; i++) {
		char j = string[i];
		string[i] = string[len-i-1];
		string[len-i-1] = j;
	}

	return string;

/*
  char tmp[33];
  char *tp = tmp;
  long i;
  unsigned long v;
  int sign;
  char *sp;

  if ( string == NULL )
  {
    return 0 ;
  }

  if (radix > 36 || radix <= 1)
  {
    return 0 ;
  }

  sign = (radix == 10 && value < 0);
  if (sign)
  {
    v = -value;
  }
  else
  {
    v = (unsigned long)value;
  }

  while (v || tp == tmp)
  {
    i = v % radix;
    v = v / radix;
    if (i < 10)
      *tp++ = i+'0';
    else
      *tp++ = i + 'a' - 10;
  }

  sp = string;

  if (sign)
    *sp++ = '-';
  while (tp > tmp)
    *sp++ = *--tp;
  *sp = 0;

  return string;
*/
}

extern char* utoa( unsigned int value, char *string, int radix )
{
  return ultoa( value, string, radix ) ;
}

extern char* ultoa( unsigned long value, char *string, int radix )
{
	char *pbuffer = string;
	unsigned long	i, len;

	/* No support for unusual radixes. */
	if (radix > 36 || radix <= 1) return 0 ;

	/* This builds the string back to front ... */
	do {
		unsigned char digit = value % radix;
		*(pbuffer++) = (digit < 10 ? '0' + digit : 'a' + digit - 10);
		value /= radix;
	} while (value > 0);

//	for (i = (pbuffer - string); i < zero_pad; i++)	*(pbuffer++) = '0';

	*(pbuffer) = '\0';

	/* ... now we reverse it (could do it recursively but will
	 * conserve the stack space) */
	len = (pbuffer - string);
	for (i = 0; i < len / 2; i++) {
		char j = string[i];
		string[i] = string[len-i-1];
		string[len-i-1] = j;
	}

	return string;

/*
  char tmp[33];
  char *tp = tmp;
  long i;
  unsigned long v = value;
  char *sp;

  if ( string == NULL )
  {
    return 0;
  }

  if (radix > 36 || radix <= 1)
  {
    return 0;
  }
 
  while (v || tp == tmp)
  {
    i = v % radix;
    v = v / radix;
    if (i < 10)
      *tp++ = i+'0';
    else
      *tp++ = i + 'a' - 10;
  }

  sp = string;

 
  while (tp > tmp)
    *sp++ = *--tp;
  *sp = 0;
  return string;
*/
}
#endif /* 0 */

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
