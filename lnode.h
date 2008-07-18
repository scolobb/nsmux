/*----------------------------------------------------------------------------*/
/*lnode.h*/
/*----------------------------------------------------------------------------*/
/*Management of cheap 'light nodes'*/
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
#ifndef __LNODE_H__
#define __LNODE_H__

/*----------------------------------------------------------------------------*/
#include <error.h>
#include <hurd/netfs.h>
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*A candy synonym for the fundamental libnetfs node*/
typedef struct node node_t;
/*----------------------------------------------------------------------------*/
/*The light node*/
struct lnode
	{
	/*the name of the lnode*/
	char * name;
	
	/*the length of the name; `name` does not change, and this value is used
	quite often, therefore calculate it just once*/
	size_t name_len;
	
	/*the full path to the lnode*/
	char * path;
	
	/*the malloced set of translators which have to be stacked upon this node
	  and upon its children; the corresponding translators will have to decide
	  on their own whether to accept directories or not*/
	char * trans;
	
	/*the number of translators listed in `translators`*/
	size_t ntrans;
	
	/*the length of the list of translators (in bytes)*/
	size_t translen;

	/*the associated flags*/
	int flags;
	
	/*the number of references to this lnode*/
	int references;
	
	/*the reference to the real node*/
	node_t * node;
	
	/*the next lnode and the pointer to this lnode from the previous one*/
	struct lnode * next, **prevp;
	
	/*the lnode (directory) in which this node is contained*/
	struct lnode * dir;
	
	/*the beginning of the list of entries contained in this lnode (directory)*/
	struct lnode * entries;
	
	/*a lock*/
	struct mutex lock;
	};/*struct lnode*/
/*----------------------------------------------------------------------------*/
typedef struct lnode lnode_t;
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Adds a reference to the `lnode` (which must be locked)*/
void
lnode_ref_add
	(
	lnode_t * node
	);
/*----------------------------------------------------------------------------*/
/*Removes a reference from `node` (which must be locked). If that was the last
	reference, destroy the node*/
void
lnode_ref_remove
	(
	lnode_t * node
	);
/*----------------------------------------------------------------------------*/
/*Creates a new lnode with `name`; the new node is locked and contains
	a single reference*/
error_t
lnode_create
	(
	char * name,
	lnode_t ** node	/*put the result here*/
	);
/*----------------------------------------------------------------------------*/
/*Destroys the given lnode*/
void
lnode_destroy
	(
	lnode_t * node	/*destroy this*/
	);
/*----------------------------------------------------------------------------*/
/*Constructs the full path for the given lnode and stores the result both in
	the parameter and inside the lnode (the same string, actually)*/
error_t
lnode_path_construct
	(
	lnode_t * node,
	char ** path	/*store the path here*/
	);
/*----------------------------------------------------------------------------*/
/*Gets a light node by its name, locks it and increments its refcount*/
error_t
lnode_get
	(
	lnode_t * dir,	/*search here*/
	char * name,		/*search for this name*/
	lnode_t ** node	/*put the result here*/
	);
/*----------------------------------------------------------------------------*/
/*Install the lnode into the lnode tree: add a reference to `dir` (which must
	be locked)*/
void
lnode_install
	(
	lnode_t * dir,	/*install here*/
	lnode_t * node	/*install this*/
	);
/*----------------------------------------------------------------------------*/
/*Unistall the node from the node tree; remove a reference from the lnode*/
void
lnode_uninstall
	(
	lnode_t * node
	);
/*----------------------------------------------------------------------------*/
/*Constructs a list of translators that were set on the ancestors of `node`*/
error_t
lnode_list_translators
	(
	lnode_t * node,
	char ** trans,	/*the malloced list of 0-separated strings*/
	size_t * ntrans	/*the number of elements in `trans`*/
	);
/*----------------------------------------------------------------------------*/
#endif /*__LNODE_H__*/
