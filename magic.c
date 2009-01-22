/*---------------------------------------------------------------------------*/
/*magic.h*/
/*---------------------------------------------------------------------------*/
/*Declaration of functions for handling the magic syntax.*/
/*---------------------------------------------------------------------------*/
/*Copyright (C) 2001, 2002, 2005, 2008 Free Software Foundation, Inc.
  Written by Sergiu Ivanov <unlimitedscolobb@gmail.com>.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or * (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
#include <string.h>
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
#include "magic.h"
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*---------Functions---------------------------------------------------------*/
/*Locates the first unescaped magic separator in the supplied file
  name. Returns NULL in case it finds nothing.*/
char * magic_find_sep (const char * name)
{
  /*The position of the separator */
  char * sep = NULL;

  /*Go through all occurrences of the separator sequence */
  for (sep = strstr (name, ",,"); sep; sep = strstr (sep, ",,"))
    {
      /*if the current separator is not escaped, stop */
      if (sep[2] != ',')
	break;

      sep += 2;
    }

  return sep;
}				/*magic_process_find_sep */

/*---------------------------------------------------------------------------*/
/*Unescapes escaped separators in the substring of the file name
  starting at `name` of length `sz`.*/
void magic_unescape (char * name, int sz)
{
  /*A pointer for string operations */
  char * str = NULL;

  /*The position of the escaped separator */
  char * esc = NULL;

  /*Go through all occurrences of the separator sequence. Note that we
    would like the whole separator to be enclosed in the substring of
    length `sz`, hence the +2. */
  for 
    (
     esc = strstr (name, ",,,"); esc && (esc - name + 2 < sz);
     esc = strstr (esc, ",,,")
     )
    {
      /*we've found and escaped separator; remove the escaping comma
	from the string */
      for (str = ++esc; *str; str[-1] = *str, ++str);
      str[-1] = 0;
      --sz;
    }
}				/*magic_unescape */
/*---------------------------------------------------------------------------*/
