/*----------------------------------------------------------------------------*/
/*lnode.c*/
/*----------------------------------------------------------------------------*/
/*Implementation of policies of management of 'light nodes'*/
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
#define _GNU_SOURCE
/*----------------------------------------------------------------------------*/
#include "lnode.h"
#include "debug.h"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Adds a reference to the `lnode` (which must be locked)*/
void
lnode_ref_add
	(
	lnode_t * node
	)
	{
	/*Increment the number of references*/
	++node->references;
	}/*lnode_ref_add*/
/*----------------------------------------------------------------------------*/
/*Removes a reference from `node` (which must be locked). If that was the last
	reference, destroy the node*/
void
lnode_ref_remove
	(
	lnode_t * node
	)
	{
	/*Fail if the node is not referenced by anybody*/
	assert(node->references);
	
	/*Decrement the number of references to `node`*/
	--node->references;
	
	/*If there are no references remaining*/
	if(node->references == 0)
		{
		/*uninstall the node from the directory it is in and destroy it*/
		lnode_uninstall(node);
		lnode_destroy(node);
		}
	else
		/*simply unlock the node*/
		mutex_unlock(&node->lock);
	}/*lnode_ref_remove*/
/*----------------------------------------------------------------------------*/
/*Creates a new lnode with `name`; the new node is locked and contains
	a single reference*/
error_t
lnode_create
	(
	char * name,
	lnode_t ** node	/*put the result here*/
	)
	{
	/*Allocate the memory for the node*/
	lnode_t * node_new = malloc(sizeof(lnode_t));
	
	/*If the memory has not been allocated*/
	if(!node_new)
		{
		/*stop*/
		return ENOMEM;
		}
		
	/*The copy of the name*/
	char * name_cp = NULL;
	
	/*If the name exists*/
	if(name)
		{
		/*duplicate it*/
		name_cp = strdup(name);
		
		/*If the name has not been duplicated*/
		if(!name_cp)
			{
			/*free the node*/
			free(node_new);
			
			/*stop*/
			return ENOMEM;
			}
		}
		
	/*Setup the new node*/
	memset(node_new, 0, sizeof(lnode_t));
	node_new->name 				= name_cp;
	node_new->name_len		= (name_cp) ? (strlen(name_cp)) : (0);
	
	/*Setup one reference to this lnode*/
	node_new->references = 1;
	
	/*Initialize the mutex and acquire a lock on this lnode*/
	mutex_init(&node_new->lock);
	mutex_lock(&node_new->lock);
	
	/*Store the result in the second parameter*/
	*node = node_new;

	/*Return success*/
	return 0;
	}/*lnode_create*/
/*----------------------------------------------------------------------------*/
/*Destroys the given lnode*/
void
lnode_destroy
	(
	lnode_t * node	/*destroy this*/
	)
	{
	/*Destroy the name of the node*/
	free(node->name);
	
	/*Destroy the node itself*/
	free(node);
	}/*lnode_destroy*/
/*----------------------------------------------------------------------------*/
/*Constructs the full path for the given lnode and stores the result both in
	the parameter and inside the lnode (the same string, actually)*/
error_t
lnode_path_construct
	(
	lnode_t * node,
	char ** path	/*store the path here*/
	)
	{
	error_t err = 0;

	/*The final path*/
	char * p;
	
	/*The final length of the path*/
	int p_len = 0;
	
	/*A temporary pointer to an lnode*/
	lnode_t * n;
	
	/*While the root node of the proxy filesystem has not been reached*/
	for(n = node; n && n->dir; n = n->dir)
		/*add the length of the name of `n` to `p_len` make some space for
			the delimiter '/', if we are not approaching the root node*/
		/*p_len += n->name_len + ((n->dir->dir) ? (1) : (0));*/
		/*There is some path to our root node, so we will anyway have to
			add a '/'*/
		p_len += n->name_len + 1;
		
	/*Include the space for the path to the root node of the proxy
		(n is now the root of the filesystem)*/
	p_len += strlen(n->path) + 1;
		
	/*Try to allocate the space for the string*/
	p = malloc(p_len * sizeof(char));
	if(!p)
		err = ENOMEM;
	/*If memory allocation has been successful*/
	else
		{
		/*put a terminal 0 at the end of the path*/
		p[--p_len] = 0;
		
		/*While the root node of the proxy filesystem has not been reached*/
		for(n = node; n && n->dir; n = n->dir)
			{
			/*compute the position where the name of `n` is to be inserted*/
			p_len -= n->name_len;
			
			/*copy the name of the node into the path (omit the terminal 0)*/
			strncpy(p + p_len, n->name, n->name_len);
			
			/*If we are not at the root node of the proxy filesystem, add the
				separator*/
			/*if(n->dir->dir)
				p[--p_len] = '/';*/
			/*we anyway have to add the separator slash*/
			p[--p_len] = '/';
			}
		
		/*put the path to the root node at the beginning of the first path
			(n is at the root now)*/
		strncpy(p, n->path, strlen(n->path));
		
		/*destroy the former path in lnode, if it exists*/
		if(node->path)
			free(node->path);
		
		/*store the new path inside the lnode*/
		node->path = p;

		/*store the path in the parameter*/
		if(path)
			*path = p;
		}
		
	/*Return the result of operations*/
	return err;
	}/*lnode_path_construct*/
/*----------------------------------------------------------------------------*/
/*Gets a light node by its name, locks it and increments its refcount*/
error_t
lnode_get
	(
	lnode_t * dir,	/*search here*/
	char * name,		/*search for this name*/
	lnode_t ** node	/*put the result here*/
	)
	{
	error_t err = 0;
	
	/*The pointer to the required lnode*/
	lnode_t * n;
	
	/*Find `name` among the names of entries in `dir`*/
	for(n = dir->entries; n && (strcmp(n->name, name) != 0); n = n->next);
	
	/*If the search has been successful*/
	if(n)
		{
		/*lock the node*/
		mutex_lock(&n->lock);
		
		/*increment the refcount of the found lnode*/
		lnode_ref_add(n);
		
		/*put a pointer to `n` into the parameter*/
		*node = n;
		}
	else
		err = ENOENT;
		
	/*Return the result of operations*/
	return err;
	}/*lnode_get*/
/*----------------------------------------------------------------------------*/
/*Install the lnode into the lnode tree: add a reference to `dir` (which must
	be locked)*/
void
lnode_install
	(
	lnode_t * dir,	/*install here*/
	lnode_t * node	/*install this*/
	)
	{
	/*Install `node` into the list of entries in `dir`*/
	node->next = dir->entries;
	node->prevp = &dir->entries; /*this node is the first on the list*/
	if(dir->entries)
		dir->entries->prevp = &node->next;	/*here `prevp` gets the value
																					corresponding to its meaning*/
	dir->entries = node;
	
	/*Add a new reference to dir*/
	lnode_ref_add(dir);
	
	/*Setup the `dir` link in node*/
	node->dir = dir;
	}/*lnode_install*/
/*----------------------------------------------------------------------------*/
/*Unistall the node from the node tree; remove a reference from the lnode
	containing `node`*/
void
lnode_uninstall
	(
	lnode_t * node
	)
	{
	/*Remove a reference from the parent*/
	lnode_ref_remove(node->dir);
	
	/*Make the next pointer in the previous element point to the element,
		which follows `node`*/
	*node->prevp = node->next;

	/*If the current node is not the last one, connect the list after removal
		of the current node*/
	if(node->next)
		node->next->prevp = &node->next;
	}/*lnode_uninstall*/
/*----------------------------------------------------------------------------*/
/*Constructs a list of translators that were set on the ancestors of `node`*/
error_t
lnode_list_translators
	(
	lnode_t * node,
	char ** trans,	/*the malloced list of 0-separated strings*/
	size_t * ntrans	/*the number of elements in `trans`*/
	)
	{
	/*The size of block of memory for the list of translators*/
	size_t sz = 0;
	
	/*Used for tracing the lineage of `node`*/
	lnode_t * ln = node;
	
	/*Used in computing the lengths of lists of translators in every node
		we will go through and for constructing the final list of translators*/
	char * p;
	
	/*The current position in *data (used in filling out the list of
		translators)*/
	char * transp;
	
	/*The length of the current translator name*/
	size_t complen;
	
	size_t i;
	
	/*Trace the lineage of the `node` (including itself) and compute the
		total length of the list of translators*/
	for(; ln; sz += ln->translen, ln = ln->dir);
	
	/*Try to allocate a block of memory sufficient for storing the list of
		translators*/
	*trans = malloc(sz);
	if(!*trans)
		return ENOMEM;
	
	/*No translators at first*/
	*ntrans = 0;
	
	/*Again trace the lineage of the `node` (including itself)*/
	for(transp = *trans + sz, ln = node; ln; ln = ln->dir)
		{
		/*Go through each translator name in the list of names*/
		for(i = 0, p = ln->trans + ln->translen - 2; i < ln->ntrans; ++i)
			{
			/*position p at the beginning of the current component and
				compute its length at the same time*/
			for(complen = 0; *p; --p, ++complen);
			--p;
			
			/*move the current position backwards*/
			transp -= complen + 1;

			/*copy the current translator name into the list*/
			strcpy(transp, p + 2);
			
			/*we've got another translator*/
			++*ntrans;
			}
		}
		
	/*Everything OK*/
	return 0;
	}/*lnode_list_translators*/
/*----------------------------------------------------------------------------*/
