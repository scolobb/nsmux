/*----------------------------------------------------------------------------*/
/*lib.h*/
/*----------------------------------------------------------------------------*/
/*Basic routines for filesystem manipulations*/
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

/*----------------------------------------------------------------------------*/
#define _GNU_SOURCE 1
/*----------------------------------------------------------------------------*/
#include <sys/mman.h>
/*----------------------------------------------------------------------------*/
#include "lib.h"
#include "debug.h"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Fetches directory entries for `dir`*/
error_t
dir_entries_get
	(
	file_t dir,
	char ** dirent_data,					/*the list of directory entries as returned
																	by dir_readdir*/
	size_t * dirent_data_size,		/*the size of `dirent_data`*/
	struct dirent *** dirent_list /*the array of pointers to beginnings of
																	dirents in dirent_data*/
	)
	{
	error_t err = 0;
	
	/*The data (array of dirents(?)) returned by dir_readdir*/
	char * data;
	
	/*The size of `data`*/
	size_t data_size;
	
	/*The number of entries in `data`*/
	int entries_num;
	
	/*Try to read the contents of the specified directory*/
	err = dir_readdir(dir, &data, &data_size, 0, -1, 0, &entries_num);
	if(err)
		return err;
		
	/*Create a new list of dirents*/
	struct dirent ** list;
	
	/*Allocate the memory for the list of dirents and for the
		finalizing element*/
	list = malloc(sizeof(struct dirent *) * (entries_num + 1));
	
	/*If memory allocation has failed*/
	if(!list)
		{
		/*free the result of dir_readdir*/
		munmap(data, data_size);
		
		/*return the corresponding error*/
		return ENOMEM;
		}
		
	/*The current directory entry*/
	struct dirent * dp;

	int i;
	
	/*Go through every element of the list of dirents*/
	for
		(
		i = 0, dp = (struct dirent *)data;
		i < entries_num;
		++i, dp = (struct dirent *)((char *)dp + dp->d_reclen))
		/*copy the current value into the list*/
		*(list + i) = dp;
	
	/*Nullify the last element of the list*/
	*(list + i) = NULL;
	
	/*Copy the required values in the parameters*/
	*dirent_data 			= data;
	*dirent_data_size	= data_size;
	*dirent_list 			= list;
	
	/*Return success*/
	return err;
	}/*dir_entries_get*/
/*----------------------------------------------------------------------------*/
/*Lookup `name` under `dir` (or cwd, if `dir` is invalid)*/
error_t
file_lookup
	(
	file_t dir,
	char * name,
	int flags0,					/*try to open with these flags first*/
	int flags1,					/*try to open with these flags, if `flags0` fail*/
	int mode,						/*if the file is to be created, create it with this mode*/
	file_t * port,			/*store the port to the looked up file here*/
	io_statbuf_t * stat	/*store the stat information here*/
	)
	{
	error_t err = 0;
	
	/*The port to the looked up file*/
	file_t p;
	
	/*The stat information about the looked up file*/
	io_statbuf_t s;
	
	/*Performs a lookup under 'dir' or in cwd, if `dir` is invalid*/
	file_t
	do_file_lookup
		(
		file_t dir,
		char * name,	/*lookup this file*/
		int flags,		/*lookup the file with these flags*/
		int mode			/*if a new file is to be created, create it with this mode*/
		)
		{
		/*The result of lookup*/
		file_t p;
	
		/*If `dir` is a valid port*/
		if(dir != MACH_PORT_NULL)
			/*try to lookup `name` under `dir`*/
			p = file_name_lookup_under(dir, name, flags, mode);
		else
			/*lookup `name` under current cwd*/
			p = file_name_lookup(name, flags, mode);
		
		/*Return the result of the lookup*/
		return p;
		}/*do_file_lookup*/
		
	/*Lookup `name` under the suggested `dir`*/
	p = do_file_lookup(dir, name, flags0, mode);
	
	/*If the lookup failed*/
	if(p == MACH_PORT_NULL)
		/*try to lookup for `name` using alternative flags `flags1`*/
		p = do_file_lookup(dir, name, flags1, mode);
	
	/*If the port is valid*/
	if(p != MACH_PORT_NULL)
		{
		/*If stat information is required*/
		if(stat)
			{
			/*obtain stat information for `p`*/
			err = io_stat(p, &s);
			if(err)
				PORT_DEALLOC(p);
			}
		}
	else
		/*copy `errno` into `err`*/
		err = errno;

	/*If no errors have happened during lookup*/
	if(!err)
		{
		/*copy the resulting port into *`port`*/
		*port = p;
		
		/*fill in the receiver for stat information, if requried*/
		if(stat)
			*stat = s;
		}
	
	/*Return the result of performing operations*/
	return err;
	}/*file_lookup*/
/*----------------------------------------------------------------------------*/
