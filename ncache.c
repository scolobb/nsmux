/*----------------------------------------------------------------------------*/
/*ncache.h*/
/*----------------------------------------------------------------------------*/
/*The implementation of the cache of nodes*/
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
#include "ncache.h"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Global Variables----------------------------------------------------*/
/*The global cache chain*/
ncache_t ncache;
/*----------------------------------------------------------------------------*/
/*Cache size (may be overwritten by the user)*/
int ncache_size = NCACHE_SIZE;
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Initializes the node cache*/
void
ncache_init
	(
	int size_max
	)
	{
	/*Reset the LRU and MRU ends of the list*/
	ncache.mru = ncache.lru = NULL;
	
	/*Set the maximal size according to the parameter*/
	ncache.size_max = size_max;
	
	/*The cache is empty so far; remark that*/
	ncache.size_current = 0;
	
	/*Init the lock*/
	mutex_init(&ncache.lock);
	}/*ncache_init*/
/*----------------------------------------------------------------------------*/
/*Looks up the lnode and stores the result in `node`; creates a new entry
	in the cache if the lookup fails*/
error_t
ncache_node_lookup
	(
	lnode_t * lnode,	/*search for this*/
	node_t ** node		/*put the result here*/
	)
	{
	error_t err = 0;

	/*Obtain the pointer to the node corresponding to `lnode`*/
	node_t * n = lnode->node;

	/*If a node has already been created for the given lnode*/
	if(n != NULL)
		{
		/*count a new reference to 'n'*/
		netfs_nref(n);
		}
	else
		{
		/*create a new node for the given lnode and store the result in `n`*/
		err = node_create(lnode, &n);
		}
		
	/*If no errors have occurred during the previous operations*/
	if(!err)
		{
		/*lock the mutex in the looked up node*/
		mutex_lock(&n->lock);
		
		/*store the lookup result in `node`*/
		*node = n;
		}
		
	/*Return the result of operations*/
	return err;
	}/*ncache_node_lookup*/
/*----------------------------------------------------------------------------*/
/*Removes the given node from the cache*/
static
void
ncache_node_remove
	(
	node_t * node
	)
	{
	/*Obtain the pointer to the netnode (this contains the information
		specific of us)*/
	struct netnode * nn = node->nn;
	
	/*If there exists a successor of this node in the cache chain*/
	if(nn->ncache_next)
		/*remove the reference in the successor*/
		nn->ncache_next->nn->ncache_prev = nn->ncache_prev;
	/*If there exists a predecessor of this node in the cache chain*/
	if(nn->ncache_prev)
		/*remove the reference in the predecessor*/
		nn->ncache_prev->nn->ncache_next = nn->ncache_next;
	
	/*If the node was located at the MRU end of the list*/
	if(ncache.mru == node)
		/*shift the MRU end to the next node*/
		ncache.mru = nn->ncache_next;
	/*If the node was located at the LRU end of the list*/
	if(ncache.lru == node)
		/*shift the LRU end to the previous node*/
		ncache.lru = nn->ncache_prev;
		
	/*Invalidate the references inside the node*/
	nn->ncache_next = nn->ncache_prev = NULL;
	
	/*Count the removal of a node*/
	--ncache.size_current;
	}/*ncache_node_remove*/
/*----------------------------------------------------------------------------*/
/*Resets the node cache*/
void
ncache_reset(void)
	{
	/*The node being currently deleted*/
	node_t * node;
	
	/*Acquire a lock on the cache*/
	mutex_lock(&ncache.lock);
	
	/*Remove the whole cache chain*/
	for(node = ncache.mru; node != NULL; ncache_node_remove(node), node = ncache.mru);

	/*Release the lock*/
	mutex_unlock(&ncache.lock);
	}/*ncache_reset*/
/*----------------------------------------------------------------------------*/
/*Adds the given node to the cache*/
void
ncache_node_add
	(
	node_t * node	/*the node to add*/
	)
	{
	/*Acquire a lock on the cache*/
	mutex_lock(&ncache.lock);
	
	/*If there already are some nodes in the cache and it is enabled*/
	if((ncache.size_max > 0) || (ncache.size_current > 0))
		{
		/*If the node to be added is not at the MRU end already*/
		if(ncache.mru != node)
			{
			/*If the node is correctly integrated in the cache*/
			if((node->nn->ncache_next != NULL) && (node->nn->ncache_prev != NULL))
				/*remove the old entry*/
				ncache_node_remove(node);
			else
				/*add a new reference to the node*/
				netfs_nref(node);
			
			/*put the node at the MRU end of the cache chain*/
			node->nn->ncache_next = ncache.mru;
			node->nn->ncache_prev = NULL;
		
			/*setup the pointer in the old MRU end, if it exists*/
			if(ncache.mru != NULL)
				ncache.mru->nn->ncache_prev = node;
			
			/*setup the LRU end of the cache chain, if it did not exist previously*/
			if(ncache.lru == NULL)
				ncache.lru = node;
			
			/*shift the MRU end to the new node*/
			ncache.mru = node;
		
			/*count the addition*/
			++ncache.size_current;
			}
		}
		
	/*While the size of the cache is exceeding the maximal size*/
	node_t * old_lru;
	for
		(
		old_lru = ncache.lru;
		ncache.size_current > ncache.size_max;
		old_lru = ncache.lru
		)
		{
		/*remove the current LRU end of the list from the cache*/
		ncache_node_remove(old_lru);
		
		/*release the reference to the node owned by this thread*/
		netfs_nrele(old_lru);
		}
		
	/*Release the lock on the cache*/
	mutex_unlock(&ncache.lock);
	}/*ncache_node_add*/
/*----------------------------------------------------------------------------*/
