/*----------------------------------------------------------------------------*/
/*debug.h*/
/*----------------------------------------------------------------------------*/
/*Simple facilities for debugging messages*/
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
#ifndef __DEBUG_H__
#define __DEBUG_H__

/*----------------------------------------------------------------------------*/
#include <stdio.h>
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Macros--------------------------------------------------------------*/
/*Print debug messages here*/
#define DEBUG_OUTPUT "/var/log/nsmux.dbg"
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
	/*Initializes the log*/
#	define INIT_LOG() nsmux_dbg = fopen(DEBUG_OUTPUT, "wt")
	/*Closes the log*/
# define CLOSE_LOG() fclose(nsmux_dbg)
	/*Prints a debug message and flushes the debug output*/
#	define LOG_MSG(fmt, args...) {fprintf(nsmux_dbg, fmt"\n", ##args);\
		fflush(nsmux_dbg);}
#else
	/*Remove requests for debugging output*/
#	define INIT_LOG()
#	define CLOSE_LOG()
#	define LOG_MSG(fmt, args...)
#endif /*DEBUG*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Global Variables----------------------------------------------------*/
/*The file to write debugging info to*/
extern FILE * nsmux_dbg;
/*----------------------------------------------------------------------------*/

#endif /*__DEBUG_H__*/
