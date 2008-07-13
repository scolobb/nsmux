/*----------------------------------------------------------------------------*/
/*options.h*/
/*----------------------------------------------------------------------------*/
/*Declarations for parsing the command line switches*/
/*----------------------------------------------------------------------------*/
/*Based on the code of unionfs translator.*/
/*----------------------------------------------------------------------------*/
/*Copyright (C) 2001, 2002, 2005 Free Software Foundation, Inc.
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
/*----------------------------------------------------------------------------*/
#ifndef __OPTIONS_H__
#define __OPTIONS_H__

/*----------------------------------------------------------------------------*/
/*--------Macros--------------------------------------------------------------*/
/*The possible short options*/
#define OPT_CACHE_SIZE 'c'
/*----------------------------------------------------------------------------*/
/*The corresponding long options*/
#define OPT_LONG_CACHE_SIZE "cache-size"
/*----------------------------------------------------------------------------*/
/*Makes a long option out of option name*/
#define OPT_LONG(o) "--"o
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Global Variables----------------------------------------------------*/
/*The argp parser for startup arguments*/
extern struct argp argp_startup;
/*----------------------------------------------------------------------------*/
/*The argp parser for rutime arguments*/
extern struct argp argp_runtime;
/*----------------------------------------------------------------------------*/
/*The number of nodes in cache (see ncache.{c,h})*/
extern int ncache_size;
/*----------------------------------------------------------------------------*/
/*The directory to mirror*/
extern char * dir;
/*----------------------------------------------------------------------------*/
#endif /*__OPTIONS_H__*/
