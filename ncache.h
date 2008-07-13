/*----------------------------------------------------------------------------*/
/*ncache.h*/
/*----------------------------------------------------------------------------*/
/*The cache of nodes*/
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
#ifndef __NCACHE_H__
#define __NCACHE_H__

/*----------------------------------------------------------------------------*/
#include <error.h>
#include <hurd/netfs.h>
/*----------------------------------------------------------------------------*/
#include "node.h"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Macros--------------------------------------------------------------*/
/*The default maximal cache size*/
#define NCACHE_SIZE 256
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*A cache chain*/
struct ncache
	{
	/*the MRU end of the cache chain*/
	node_t * mru;
	
	/*the LRU end of the cache chain*/
	node_t * lru;
	
	/*the maximal number of nodes to cache*/
	int size_max;
	
	/*the current length of the cache chain*/
	int size_current;
	
	/*a lock*/
	struct mutex lock;
	};/*struct ncache*/
/*----------------------------------------------------------------------------*/
typedef struct ncache ncache_t;
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Global Variables----------------------------------------------------*/
/*The cache size (may be overwritten by the user)*/
extern int cache_size;
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Initializes the node cache*/
void
ncache_init
	(
	int size_max
	);
/*----------------------------------------------------------------------------*/
/*Looks up the lnode and stores the result in `node`; creates a new entry
	in the cache if the lookup fails*/
error_t
ncache_node_lookup
	(
	lnode_t * lnode,	/*search for this*/
	node_t ** node		/*put the result here*/
	);
/*----------------------------------------------------------------------------*/
/*Resets the node cache*/
void
ncache_reset(void);
/*----------------------------------------------------------------------------*/
/*Adds the given node to the cache*/
void
ncache_node_add
	(
	node_t * node	/*the node to add*/
	);
/*----------------------------------------------------------------------------*/
#endif /*__NCACHE_H__*/
