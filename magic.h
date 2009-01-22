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
#ifndef __MAGIC_H__
#define __MAGIC_H__

/*---------------------------------------------------------------------------*/
/*---------Functions---------------------------------------------------------*/
/*Locates the first unescaped magic separator in the supplied
  filename. Returns NULL in case it finds nothing.*/
char * magic_find_sep (const char * name);
/*---------------------------------------------------------------------------*/
/*Unescapes escaped separators in the substring of the filename
  starting at `name` of length `sz`.*/
void magic_unescape (char * name, int sz);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
#endif /*__MAGIC_H__*/
