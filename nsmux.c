/*---------------------------------------------------------------------------*/
/*nsmux.c*/
/*---------------------------------------------------------------------------*/
/*The filesystem proxy for namespace-based translator selection.*/
/*---------------------------------------------------------------------------*/
/*Based on the code of unionfs translator.*/
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
#define _GNU_SOURCE 1
/*---------------------------------------------------------------------------*/
#include "nsmux.h"
/*---------------------------------------------------------------------------*/
#include <error.h>
#include <argp.h>
#include <hurd/netfs.h>
#include <fcntl.h>
#include <hurd/paths.h>
/*---------------------------------------------------------------------------*/
#include "debug.h"
#include "options.h"
#include "ncache.h"
#include "magic.h"
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*--------Global Variables---------------------------------------------------*/
/*The name of the server*/
char *netfs_server_name = "nsmux";
/*---------------------------------------------------------------------------*/
/*The version of the server*/
char *netfs_server_version = "0.0";
/*---------------------------------------------------------------------------*/
/*The maximal length of a chain of symbolic links*/
int netfs_maxsymlinks = 12;
/*---------------------------------------------------------------------------*/
/*A port to the underlying node*/
mach_port_t underlying_node;
/*---------------------------------------------------------------------------*/
/*Status information for the underlying node*/
io_statbuf_t underlying_node_stat;
/*---------------------------------------------------------------------------*/
/*Mapped time used for updating node information*/
volatile struct mapped_time_value *maptime;
/*---------------------------------------------------------------------------*/
/*The filesystem ID*/
pid_t fsid;
/*---------------------------------------------------------------------------*/
/*The file to print debug messages to*/
FILE *nsmux_dbg;
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*--------Functions----------------------------------------------------------*/
/*Attempts to create a file named `name` in `dir` for `user` with mode `mode`*/
error_t
  netfs_attempt_create_file
  (struct iouser *user,
   struct node *dir, char *name, mode_t mode, struct node **node)
{
  LOG_MSG ("netfs_attempt_create_file");

  /*Unlock `dir` and say that we can do nothing else here */
  mutex_unlock (&dir->lock);
  return EOPNOTSUPP;
}				/*netfs_attempt_create_file */

/*---------------------------------------------------------------------------*/
/*Return an error if the process of opening a file should not be
  allowed to complete because of insufficient permissions*/
error_t
  netfs_check_open_permissions
  (struct iouser * user, struct node * np, int flags, int newnode)
{
  LOG_MSG ("netfs_check_open_permissions: '%s'", np->nn->lnode->name);

  /*Cheks user's permissions and return the result */
  return check_open_permissions (user, &np->nn_stat, flags);
}				/*netfs_check_open_permissions */

/*---------------------------------------------------------------------------*/
/*Attempts an utimes call for the user `cred` on node `node`*/
error_t
  netfs_attempt_utimes
  (struct iouser * cred,
   struct node * node, struct timespec * atime, struct timespec * mtime)
{
  LOG_MSG ("netfs_attempt_utimes");

  error_t err = 0;

  /*See what information is to be updated */
  int flags = TOUCH_CTIME;

  /*Check if the user is indeed the owner of the node */
  err = fshelp_isowner (&node->nn_stat, cred);

  /*If the user is allowed to do utimes */
  if (!err)
    {
      /*If atime is to be updated */
      if (atime)
	/*update the atime */
	node->nn_stat.st_atim = *atime;
      else
	/*the current time will be set as the atime */
	flags |= TOUCH_ATIME;

      /*If mtime is to be updated */
      if (mtime)
	/*update the mtime */
	node->nn_stat.st_mtim = *mtime;
      else
	/*the current time will be set as mtime */
	flags |= TOUCH_MTIME;

      /*touch the file */
      fshelp_touch (&node->nn_stat, flags, maptime);
    }

  /*Return the result of operations */
  return err;
}				/*netfs_attempt_utimes */

/*---------------------------------------------------------------------------*/
/*Returns the valid access types for file `node` and user `cred`*/
error_t
  netfs_report_access (struct iouser * cred, struct node * np, int *types)
{
  LOG_MSG ("netfs_report_access");

  /*No access at first */
  *types = 0;

  /*Check the access and set the required bits */
  if (fshelp_access (&np->nn_stat, S_IREAD, cred) == 0)
    *types |= O_READ;
  if (fshelp_access (&np->nn_stat, S_IWRITE, cred) == 0)
    *types |= O_WRITE;
  if (fshelp_access (&np->nn_stat, S_IEXEC, cred) == 0)
    *types |= O_EXEC;

  /*Everything OK */
  return 0;
}				/*netfs_report_access */

/*---------------------------------------------------------------------------*/
/*Validates the stat data for the node*/
error_t netfs_validate_stat (struct node * np, struct iouser * cred)
{
  LOG_MSG ("netfs_validate_stat: '%s'", (np ? np->nn->lnode->name : ""));

  error_t err = 0;

  /*If we are not at the root */
  if (np != netfs_root_node)
    {
      /*If the node is not surely up-to-date */
      if (!(np->nn->flags & FLAG_NODE_ULFS_UPTODATE))
	{
	  /*update it */
	  err = node_update (np);
	}

      /*If no errors have yet occurred */
      if (!err)
	{
	  /*If the port to the file corresponding to `np` is valid */
	  if (np->nn->port != MACH_PORT_NULL)
	    {
	      /*We have a directory here (normally, only they maintain an open port).
	         Generally, our only concern is to maintain an open port in this case */

	      /*attempt to stat this file */
	      err = io_stat (np->nn->port, &np->nn_stat);

	      if (S_ISDIR (np->nn_stat.st_mode))
		LOG_MSG ("\tIs a directory");

	      /*If stat information has been successfully obtained for the file */
	      if (!err)
		/*duplicate the st_mode field of stat structure */
		np->nn_translated = np->nn_stat.st_mode;
	    }
	  else
	    {
	      /*We, most probably, have something which is not a
	         directory. Therefore we will open the port and close
	         it after the stat, so that additional resources are
	         not consumed. */

	      /*the parent node of the current node */
	      node_t *dnp;

	      /*obtain the parent node of the the current node */
	      err = ncache_node_lookup (np->nn->lnode->dir, &dnp);

	      /*the lookup should never fail here */
	      assert (!err);

	      /*open a port to the file we are interested in */
	      mach_port_t p = file_name_lookup_under
		(dnp->nn->port, np->nn->lnode->name, 0, 0);

	      /*put `dnp` back, since we don't need it any more */
	      netfs_nput (dnp);

	      if (!p)
		return EBADF;

	      /*try to stat the node */
	      err = io_stat (p, &np->nn_stat);

	      /*deallocate the port */
	      PORT_DEALLOC (p);
	    }
	}
    }
  /*If we are at the root */
  else
    /*put the size of the node into the stat structure belonging to `np` */
    node_get_size (np, (OFFSET_T *) & np->nn_stat.st_size);

  /*Return the result of operations */
  return err;
}				/*netfs_validate_stat */

/*---------------------------------------------------------------------------*/
/*Syncs `node` completely to disk*/
error_t
  netfs_attempt_sync (struct iouser * cred, struct node * node, int wait)
{
  LOG_MSG ("netfs_attempt_sync");

  /*Operation is not supported */
  return EOPNOTSUPP;
}				/*netfs_attempt_sync */

/*---------------------------------------------------------------------------*/
/*Fetches a directory*/
error_t
  netfs_get_dirents
  (struct iouser * cred,
   struct node * dir,
   int first_entry,
   int num_entries,
   char **data,
   mach_msg_type_number_t * data_len,
   vm_size_t max_data_len, int *data_entries)
{
  LOG_MSG ("netfs_get_dirents: '%s'", dir->nn->lnode->name);

  error_t err;

  /*Two pointers required for processing the list of dirents */
  node_dirent_t *dirent_start, *dirent_current;

  /*The pointer to the beginning of the list of dirents */
  node_dirent_t *dirent_list = NULL;

  /*The size of the current dirent */
  size_t size = 0;

  /*The number of dirents added */
  int count = 0;

  /*The dereferenced value of parameter `data` */
  char *data_p;

  /*Takes into account the size of the given dirent */
  int bump_size (const char *name)
  {
    /*If the required number of entries has not been listed yet */
    if ((num_entries == -1) || (count < num_entries))
      {
	/*take the current size and take into account the length of the name */
	size_t new_size = size + DIRENT_LEN (strlen (name));

	/*If there is a limit for the received size and it has been exceeded */
	if ((max_data_len > 0) && (new_size > max_data_len))
	  /*a new dirent cannot be added */
	  return 0;

	/*memorize the new size */
	size = new_size;

	/*increase the number of dirents added */
	++count;

	/*everything is OK */
	return 1;
      }
    else
      {
	/*dirents cannot be added */
	return 0;
      }
  }				/*bump_size */

  /*Adds a dirent to the list of dirents */
  int add_dirent (const char *name, ino_t ino, int type)
  {
    /*If the required number of dirents has not been listed yet */
    if ((num_entries == -1) || (count < num_entries))
      {
	/*create a new dirent */
	struct dirent hdr;

	/*obtain the length of the name */
	size_t name_len = strlen (name);

	/*compute the full size of the dirent */
	size_t sz = DIRENT_LEN (name_len);

	/*If there is no room for this dirent */
	if (sz > size)
	  /*stop */
	  return 0;
	else
	  /*take into account the fact that a new dirent has just been added */
	  size -= sz;

	/*setup the dirent */
	hdr.d_ino = ino;
	hdr.d_reclen = sz;
	hdr.d_type = type;
	hdr.d_namlen = name_len;

	/*The following two lines of code reflect the old layout of
	   dirents in the memory. Now libnetfs expects the layout
	   identical to the layout provided by dir_readdir
	   see dir_entries_get) */

	/*copy the header of the dirent into the final block of dirents */
	memcpy (data_p, &hdr, DIRENT_NAME_OFFS);

	/*copy the name of the dirent into the block of dirents */
	strcpy (data_p + DIRENT_NAME_OFFS, name);

	/*This line is commented for the same reason as the two specifically
	   commented lines above. */
	/*move the current pointer in the block of dirents */
	data_p += sz;

	/*count the new dirent */
	++count;

	/*everything was OK, so say it */
	return 1;
      }
    else
      /*no [new] dirents allowed */
      return 0;
  }				/*add_dirent */

  /*List the dirents for node `dir` */
  err = node_entries_get (dir, &dirent_list);

  /*If listing was successful */
  if (!err)
    {
      /*find the entry whose number is `first_entry` */
      for
	(dirent_start = dirent_list, count = 2;
	 dirent_start && (count < first_entry);
	 dirent_start = dirent_start->next, ++count);

      /*reset number of dirents added so far */
      count = 0;

      /*make space for entries '.' and '..', if required */
      if (first_entry == 0)
	bump_size (".");
      if (first_entry <= 1)
	bump_size ("..");

      /*Go through all dirents */
      for
	(dirent_current = dirent_start;
	 dirent_current; dirent_current = dirent_current->next)
	/*If another dirent cannot be added succesfully */
	if (bump_size (dirent_current->dirent->d_name) == 0)
	  /*stop here */
	  break;

      /*allocate the required space for dirents */
      *data = mmap (0, size, PROT_READ | PROT_WRITE, MAP_ANON, 0, 0);

      /*check if any error occurred */
      err = ((void *) *data == MAP_FAILED) ? (errno) : (0);
    }

  /*If no errors have occurred so far */
  if (!err)
    {
      /*obtain the pointer to the beginning of the block of dirents */
      data_p = *data;

      /*fill the parameters with useful values */
      *data_len = size;
      *data_entries = count;

      /*reset the number of dirents added */
      count = 0;

      /*add entries '.' and '..', if required */
      if (first_entry == 0)
	add_dirent (".", 2, DT_DIR);
      if (first_entry <= 1)
	add_dirent ("..", 2, DT_DIR);

      /*Follow the list of dirents beginning with dirents_start */
      for
	(dirent_current = dirent_start; dirent_current;
	 dirent_current = dirent_current->next)
	/*If the addition of the current dirent fails */
	if (add_dirent
	    (dirent_current->dirent->d_name, dirent_current->dirent->d_fileno,
	     dirent_current->dirent->d_type) == 0)
	  /*stop adding dirents */
	  break;
    }

  /*If the list of dirents has been allocated, free it */
  if (dirent_list)
    node_entries_free (dirent_list);

  /*The directory has been read right now, modify the access time */
  fshelp_touch (&dir->nn_stat, TOUCH_ATIME, maptime);

  /*Return the result of listing the dirents */
  return err;
}				/*netfs_get_dirents */

/*---------------------------------------------------------------------------*/
/*Looks up `name` under `dir` for `user`*/
error_t
  netfs_attempt_lookup
  (struct iouser * user, struct node * dir, char *name, struct node ** node)
{
  LOG_MSG ("netfs_attempt_lookup");

  /*We should never get here. In any case, we do not want to do this type
     of lookup, netfs_attempt_lookup_improved is what we want */
  return EOPNOTSUPP;
}				/*netfs_attempt_lookup */

/*---------------------------------------------------------------------------*/
/*Performs an advanced lookup of file `name` under `dir`. If the
  lookup of the last component of the path is requested (`lastcomp` is
  1), it is not a directory and the creation of proxy (shadow) node is
  not required (`proxy` is 1), the function will simply open the
  required file and return the port in `file`. In other cases it will
  create a proxy node and return it in `node`.*/
error_t
  netfs_attempt_lookup_improved
  (struct iouser * user, struct node * dir, char *name, int flags,
   int lastcomp, node_t ** node, file_t * file, int proxy)
{
  LOG_MSG ("netfs_attempt_lookup_improved: '%s'", name);

  error_t err = 0;

  /*If we are asked to fetch the current directory */
  if (strcmp (name, ".") == 0)
    {
      /*validate the stat information for `dir` */
      err = netfs_validate_stat (dir, user);
      if (err)
	{
	  mutex_unlock (&dir->lock);
	  return err;
	}

      /*If `dir` is not a directory, actually */
      if (!S_ISDIR (dir->nn_stat.st_mode))
	{
	  /*unlock the directory and stop right here */
	  mutex_unlock (&dir->lock);
	  return ENOTDIR;
	}

      /*add a reference to `dir` and put it into `node` */
      netfs_nref (dir);
      *node = dir;

      /*everything is OK */
      return 0;
    }
  /*If we are asked to fetch the parent directory */
  else if (strcmp (name, "..") == 0)
    {
      /*validate the stat information for `dir` */
      err = netfs_validate_stat (dir, user);
      if (err)
	{
	  mutex_unlock (&dir->lock);
	  return err;
	}

      /*If `dir` is not a directory, actually */
      if (!S_ISDIR (dir->nn_stat.st_mode))
	{
	  /*unlock the directory and stop right here */
	  mutex_unlock (&dir->lock);
	  return ENOTDIR;
	}

      /*If the supplied node is not root */
      if (dir->nn->lnode->dir)
	{
	  /*The node corresponding to the parent directory must exist here */
	  assert (dir->nn->lnode->dir->node);

	  /*put the parent node of `dir` into the result */
	  err = ncache_node_lookup (dir->nn->lnode->dir, node);
	}
      /*The supplied node is root */
      else
	{
	  /*this node is not included into our filesystem */
	  err = ENOENT;
	  *node = NULL;
	}

      /*unlock the directory */
      mutex_unlock (&dir->lock);

      /*stop here */
      return err;
    }

  /*The port to the requested file */
  mach_port_t p;

  /*The lnode corresponding to the entry we are supposed to fetch */
  lnode_t *lnode;

  /*Is the looked up file a directory */
  int isdir;

  /*Finalizes the execution of this function */
  void finalize (void)
  {
    /*If some errors have occurred */
    if (err)
      {
	/*the user should receive nothing */
	*node = NULL;

	/*If there is some port, free it */
	if (p != MACH_PORT_NULL)
	  PORT_DEALLOC (p);
      }
    /*If there is a node to return */
    if (*node)
      {
	/*unlock the node */
	mutex_unlock (&(*node)->lock);

	/*add the node to the cache */
	ncache_node_add (*node);
      }

    /*Unlock the mutexes in `dir` */
    mutex_unlock (&dir->nn->lnode->lock);
    mutex_unlock (&dir->lock);
  }				/*finalize */

  /*Performs a usual lookup */
  error_t lookup (char *name,	/*lookup this */
		  int flags,	/*lookup `name` in the this way */
		  int proxy	/*should a proxy node be created */
    )
  {
    /*Try to lookup the given file in the underlying directory */
    p = file_name_lookup_under (dir->nn->port, name, 0, 0);

    /*If the lookup failed */
    if (p == MACH_PORT_NULL)
      /*no such entry */
      return ENOENT;

    /*Obtain the stat information about the file */
    io_statbuf_t stat;
    err = io_stat (p, &stat);

    /*Deallocate the obtained port */
    PORT_DEALLOC (p);

    /*If this file is not a directory */
    if (err || !S_ISDIR (stat.st_mode))
      {
	/*remember we do not have a directory */
	isdir = 0;

	if (!proxy)
	  {
	    /*We don't need to do lookups here if a proxy shadow node
	      is required. The lookup will be done by the translator
	      starting procedure. Just check whether the file
	      exists.*/

	    p = file_name_lookup_under (dir->nn->port, name, flags, 0);
	    if (p == MACH_PORT_NULL)
	      return EBADF;

	    /*If a proxy node is not required */
	    if (!proxy)
	      /*stop here, we want only the port to the file */
	      return 0;
	  }
	else
	    p = MACH_PORT_NULL;
      }
    else
      {
	if (!lastcomp || !proxy)
	  {
	    p = file_name_lookup_under
	      (dir->nn->port, name, flags | O_READ | O_DIRECTORY, 0);
	    if (p == MACH_PORT_NULL)
	      return EBADF;		/*not enough rights? */
	  }
	else
	  /*If we are at the last component of the path and need to
	    open a directory, do not do the lookup; the translator
	    starting procedure will do that.*/
	  p = MACH_PORT_NULL;

	/*we have a directory here */
	isdir = 1;
      }

    /*Try to find an lnode called `name` under the lnode corresponding
      to `dir` */
    err = lnode_get (dir->nn->lnode, name, &lnode);

    /*If such an entry does not exist */
    if (err == ENOENT)
      {
	/*create a new lnode with the supplied name */
	err = lnode_create (name, &lnode);
	if (err)
	  {
	    finalize ();
	    return err;
	  }

	/*install the new lnode into the directory */
	lnode_install (dir->nn->lnode, lnode);
      }

    /*If we are to create a proxy node */
    if (proxy)
      /*create a proxy node from the given lnode */
      err = node_create_proxy (lnode, node);
    /*If we don't need proxy nodes in this lookup */
    else
      {
	/*obtain the node corresponding to this lnode */
	err = ncache_node_lookup (lnode, node);

	/*remove an extra reference from the lnode */
	lnode_ref_remove (lnode);
      }

    /*If either the lookup in the cache or the creation of a proxy failed */
    if (err)
      {
	/*stop */
	mutex_unlock (&lnode->lock);
	finalize ();
	return err;
      }

    /*Store the port in the node */
    (*node)->nn->port = p;

    /*Fill in the flag about the node being a directory */
    if (isdir)
      lnode->flags |= FLAG_LNODE_DIR;
    else
      lnode->flags &= ~FLAG_LNODE_DIR;

    /*Construct the full path to the node */
    err = lnode_path_construct (lnode, NULL);
    if (err)
      {
	mutex_unlock (&lnode->lock);
	finalize ();
	return err;
      }

    /*Unlock the lnode */
    mutex_unlock (&lnode->lock);

    /*Now the node is up-to-date */
    (*node)->nn->flags = FLAG_NODE_ULFS_UPTODATE;

    /*Everything OK here */
    return 0;
  }				/*lookup */

  /*Simply lookup the provided name, without creating the proxy, if not
    necessary (i.e. when the file is not a directory) */
  err = lookup (name, flags, proxy);
  if (err)
    {
      finalize ();
      return err;
    }

  /*if we have looked up a regular file, store the port to it in *`file` */
  if (!isdir)
    *file = p;

  /*Everything OK here */
  finalize ();
  return err;
}				/*netfs_attempt_lookup_improved */

/*---------------------------------------------------------------------------*/
/*Responds to the RPC dir_lookup*/
error_t
  netfs_S_dir_lookup
  (struct protid * diruser,
   char *filename,
   int flags,
   mode_t mode,
   retry_type * do_retry,
   char *retry_name,
   mach_port_t * retry_port, mach_msg_type_number_t * retry_port_type)
{
  LOG_MSG ("netfs_S_dir_lookup: '%s'", filename);

  int create;			/* true if O_CREAT flag set */
  int excl;			/* true if O_EXCL flag set */
  int mustbedir = 0;		/* true if the result must be S_IFDIR */
  int lastcomp = 0;		/* true if we are at the last component */
  int newnode = 0;		/* true if this node is newly created */
  int nsymlinks = 0;
  struct node *dnp, *np;
  char *nextname;
  error_t error;
  struct protid *newpi;
  struct iouser *user;

  /*The port to the file for the case when we don't need proxy nodes */
  file_t file = MACH_PORT_NULL;

  /*The port to the same file with restricted rights */
  file_t file_restricted = MACH_PORT_NULL;

  /*The stat information about the node pointed at by `file` (for the case
     when not proxy nodes are to be created) */
  io_statbuf_t stat;

  /*The position of the magic separator in the filename */
  char * sep;

  /*The position of the next magic separator in the filename */
  char * next_sep;

  if (!diruser)
    return EOPNOTSUPP;

  create = (flags & O_CREAT);
  excl = (flags & O_EXCL);

  /* Skip leading slashes */
  while (*filename == '/')
    filename++;

  *retry_port_type = MACH_MSG_TYPE_MAKE_SEND;
  *do_retry = FS_RETRY_NORMAL;
  *retry_name = '\0';

  if (*filename == '\0')
    {
      /* Set things up in the state expected by the code from gotit: on. */
      dnp = 0;
      np = diruser->po->np;
      mutex_lock (&np->lock);
      netfs_nref (np);
      goto gotit;
    }

  dnp = diruser->po->np;
  mutex_lock (&dnp->lock);

  netfs_nref (dnp);		/* acquire a reference for later netfs_nput */

  do
    {
      assert (!lastcomp);

      /* Find the name of the next pathname component */
      nextname = index (filename, '/');

      if (nextname)
	{
	  *nextname++ = '\0';
	  while (*nextname == '/')
	    nextname++;
	  if (*nextname == '\0')
	    {
	      /* These are the rules for filenames ending in /. */
	      nextname = 0;
	      lastcomp = 1;
	      mustbedir = 1;
	      create = 0;
	    }
	  else
	    lastcomp = 0;
	}
      else
	lastcomp = 1;

      np = 0;

    retry_lookup:

      if ((dnp == netfs_root_node || dnp == diruser->po->shadow_root)
	  && filename[0] == '.' && filename[1] == '.' && filename[2] == '\0')
	if (dnp == diruser->po->shadow_root)
	  /* We're at the root of a shadow tree.  */
	  {
	    *do_retry = FS_RETRY_REAUTH;
	    *retry_port = diruser->po->shadow_root_parent;
	    *retry_port_type = MACH_MSG_TYPE_COPY_SEND;
	    if (!lastcomp)
	      strcpy (retry_name, nextname);
	    error = 0;
	    mutex_unlock (&dnp->lock);
	    goto out;
	  }
	else if (diruser->po->root_parent != MACH_PORT_NULL)
	  /* We're at a real translator root; even if DIRUSER->po has a
	     shadow root, we can get here if its in a directory that was
	     renamed out from under it...  */
	  {
	    *do_retry = FS_RETRY_REAUTH;
	    *retry_port = diruser->po->root_parent;
	    *retry_port_type = MACH_MSG_TYPE_COPY_SEND;
	    if (!lastcomp)
	      strcpy (retry_name, nextname);
	    error = 0;
	    mutex_unlock (&dnp->lock);
	    goto out;
	  }
	else
	  /* We are global root */
	  {
	    error = 0;
	    np = dnp;
	    netfs_nref (np);
	  }
      else
	{
	  /* Attempt a lookup on the next pathname component. */
	  /*error = netfs_attempt_lookup (diruser->user, dnp, filename, &np);*/

	  /*try to find the magic separator in the filename */
	  sep = magic_find_sep (filename);

	  if (sep)
	    {
	      /*We have to create a shadow node and set a translator
		on it. */

	      sep[0] = sep[1] = 0;
	      sep += 2;

	      error = netfs_attempt_lookup_improved
		(diruser->user, dnp, filename, flags, lastcomp, &np, &file, 1);
	      if (!error && !excl)
		{
		  /*set the required translator on the node */
		  error = node_set_translator
		    (diruser, np, sep, flags, filename, &file);
		  if (error)
		    goto out;

		  /*if there is at least one more separator in the
		    filename, we will have to do a retry */
		  next_sep = magic_find_sep(sep);
		  if (next_sep)
		    {
		    }
		  else
		    /*No (more) retries are necessary */
		    goto justport;
		}
	    }
	  else
	    {
	      /*We have to do an ordinary lookup. */

	      error = netfs_attempt_lookup_improved
		(diruser->user, dnp, filename, flags, lastcomp, &np, &file, 0);
	      if (!error && !excl && (file != MACH_PORT_NULL))
		/*We have looked up an ordinary file, no proxy nodes
		  have been created; finalize and stop */
		goto justport;
	    }
	}

      /* At this point, DNP is unlocked */

      /* Implement O_EXCL flag here */
      if (lastcomp && create && excl && !error)
	error = EEXIST;

      /* Create the new node if necessary */
      if (lastcomp && create && error == ENOENT)
	{
	  mode &= ~(S_IFMT | S_ISPARE | S_ISVTX);
	  mode |= S_IFREG;
	  mutex_lock (&dnp->lock);
	  error = netfs_attempt_create_file (diruser->user, dnp,
					     filename, mode, &np);

	  /* If someone has already created the file (between our lookup
	     and this create) then we just got EEXIST.  If we are
	     EXCL, that's fine; otherwise, we have to retry the lookup. */
	  if (error == EEXIST && !excl)
	    {
	      mutex_lock (&dnp->lock);
	      goto retry_lookup;
	    }

	  newnode = 1;
	}

      /* All remaining errors get returned to the user */
      if (error)
	goto out;

      error = netfs_validate_stat (np, diruser->user);
      if (error)
	goto out;

      if ((((flags & O_NOTRANS) == 0) || !lastcomp)
	  && ((np->nn_translated & S_IPTRANS)
	      || S_ISFIFO (np->nn_translated)
	      || S_ISCHR (np->nn_translated)
	      || S_ISBLK (np->nn_translated)
	      || fshelp_translated (&np->transbox)))
	{
	  mach_port_t dirport;

	  /* A callback function for short-circuited translators.
	     S_ISLNK and S_IFSOCK are handled elsewhere. */
	  error_t short_circuited_callback1 (void *cookie1, void *cookie2,
					     uid_t * uid, gid_t * gid,
					     char **argz, size_t * argz_len)
	  {
	    struct node *np = cookie1;
	    error_t err;

	    err = netfs_validate_stat (np, diruser->user);
	    if (err)
	      return err;

	    switch (np->nn_translated & S_IFMT)
	      {
	      case S_IFCHR:
	      case S_IFBLK:
		if (asprintf (argz, "%s%c%d%c%d",
			      (S_ISCHR (np->nn_translated)
			       ? _HURD_CHRDEV : _HURD_BLKDEV),
			      0, major (np->nn_stat.st_rdev),
			      0, minor (np->nn_stat.st_rdev)) < 0)
		  return ENOMEM;
		*argz_len = strlen (*argz) + 1;
		*argz_len += strlen (*argz + *argz_len) + 1;
		*argz_len += strlen (*argz + *argz_len) + 1;
		break;
	      case S_IFIFO:
		if (asprintf (argz, "%s", _HURD_FIFO) < 0)
		  return ENOMEM;
		*argz_len = strlen (*argz) + 1;
		break;
	      default:
		return ENOENT;
	      }

	    *uid = np->nn_stat.st_uid;
	    *gid = np->nn_stat.st_gid;

	    return 0;
	  }

	  /* Create an unauthenticated port for DNP, and then
	     unlock it. */
	  error = iohelp_create_empty_iouser (&user);
	  if (!error)
	    {
	      newpi = netfs_make_protid (netfs_make_peropen (dnp, 0,
							     diruser->po),
					 user);
	      if (!newpi)
		{
		  error = errno;
		  iohelp_free_iouser (user);
		}
	    }

	  if (!error)
	    {
	      dirport = ports_get_send_right (newpi);
	      ports_port_deref (newpi);

	      error = fshelp_fetch_root (&np->transbox, diruser->po,
					 dirport,
					 diruser->user,
					 lastcomp ? flags : 0,
					 ((np->nn_translated & S_IPTRANS)
					  ? _netfs_translator_callback1
					  : short_circuited_callback1),
					 _netfs_translator_callback2,
					 do_retry, retry_name, retry_port);
	      /* fetch_root copies DIRPORT for success, so we always should
	         deallocate our send right.  */
	      mach_port_deallocate (mach_task_self (), dirport);
	    }

	  if (error != ENOENT)
	    {
	      netfs_nrele (dnp);
	      netfs_nput (np);
	      *retry_port_type = MACH_MSG_TYPE_MOVE_SEND;
	      if (!lastcomp && !error)
		{
		  strcat (retry_name, "/");
		  strcat (retry_name, nextname);
		}
	      return error;
	    }

	  /* ENOENT means there was a hiccup, and the translator
	     vanished while NP was unlocked inside fshelp_fetch_root;
	     continue as normal. */
	  error = 0;
	}

      /* "foo/" must see that foo points to a dir */
      if (S_ISLNK (np->nn_translated) && (!lastcomp || mustbedir
					  || !(flags &
					       (O_NOLINK | O_NOTRANS))))
	{
	  size_t nextnamelen, newnamelen, linklen;
	  char *linkbuf;

	  /* Handle symlink interpretation */
	  if (nsymlinks++ > netfs_maxsymlinks)
	    {
	      error = ELOOP;
	      goto out;
	    }

	  linklen = np->nn_stat.st_size;

	  nextnamelen = nextname ? strlen (nextname) + 1 : 0;
	  newnamelen = nextnamelen + linklen + 1;
	  linkbuf = alloca (newnamelen);

	  error = netfs_attempt_readlink (diruser->user, np, linkbuf);
	  if (error)
	    goto out;

	  if (nextname)
	    {
	      linkbuf[linklen] = '/';
	      memcpy (linkbuf + linklen + 1, nextname, nextnamelen - 1);
	    }
	  linkbuf[nextnamelen + linklen] = '\0';

	  if (linkbuf[0] == '/')
	    {
	      /* Punt to the caller */
	      *do_retry = FS_RETRY_MAGICAL;
	      *retry_port = MACH_PORT_NULL;
	      strcpy (retry_name, linkbuf);
	      goto out;
	    }

	  filename = linkbuf;
	  if (lastcomp)
	    {
	      lastcomp = 0;

	      /* Symlinks to nonexistent files aren't allowed to cause
	         creation, so clear the flag here. */
	      create = 0;
	    }
	  netfs_nput (np);
	  mutex_lock (&dnp->lock);
	  np = 0;
	}
      else
	{
	  /* Normal nodes here for next filename component */
	  filename = nextname;
	  netfs_nrele (dnp);

	  if (lastcomp)
	    dnp = 0;
	  else
	    {
	      dnp = np;
	      np = 0;
	    }
	}
    }
  while (filename && *filename);

  /* At this point, NP is the node to return.  */
gotit:

  if (mustbedir)
    {
      netfs_validate_stat (np, diruser->user);
      if (!S_ISDIR (np->nn_stat.st_mode))
	{
	  error = ENOTDIR;
	  goto out;
	}
    }
  error = netfs_check_open_permissions (diruser->user, np, flags, newnode);
  if (error)
    goto out;

  flags &= ~OPENONLY_STATE_MODES;

  error = iohelp_dup_iouser (&user, diruser->user);
  if (error)
    goto out;

  newpi = netfs_make_protid (netfs_make_peropen (np, flags, diruser->po),
			     user);
  if (!newpi)
    {
      iohelp_free_iouser (user);
      error = errno;
      goto out;
    }

  *retry_port = ports_get_right (newpi);
  ports_port_deref (newpi);

  goto out;

  /*At this point, we have to return `file` as the resulting port */
justport:
  /*If a directory is definitely wanted */
  if (mustbedir)
    {
      /*stat the looked up file */
      error = io_stat (file, &stat);
      if (error)
	goto out;

      /*If the file is not a directory */
      if (!S_ISDIR (stat.st_mode))
	{
	  /*this is an error; stop here */
	  error = ENOTDIR;
	  goto out;
	}

    }

  /*Check the user's open permissions for the specified port */
  error = check_open_permissions (diruser->user, &stat, file);
  if (error)
    goto out;

  /*Restrict the port to the rights of the user */
  error = io_restrict_auth
    (file, &file_restricted,
     diruser->user->uids->ids, diruser->user->uids->num,
     diruser->user->gids->ids, diruser->user->gids->num);
  if (error)
    goto out;

  /*Put the resulting port in the corresponding receiver parameter */
  *retry_port = file_restricted;
  *retry_port_type = MACH_MSG_TYPE_MOVE_SEND;

out:
  if (error && (file != MACH_PORT_NULL))
    PORT_DEALLOC (file);
  if (np)
    netfs_nput (np);
  if (dnp)
    netfs_nrele (dnp);
  return error;
}				/*netfs_S_dir_lookup */

/*---------------------------------------------------------------------------*/
/*Deletes `name` in `dir` for `user`*/
error_t
  netfs_attempt_unlink (struct iouser * user, struct node * dir, char *name)
{
  LOG_MSG ("netfs_attempt_unlink");

  return 0;
}				/*netfs_attempt_unlink */

/*---------------------------------------------------------------------------*/
/*Attempts to rename `fromdir`/`fromname` to `todir`/`toname`*/
error_t
  netfs_attempt_rename
  (struct iouser * user,
   struct node * fromdir,
   char *fromname, struct node * todir, char *toname, int excl)
{
  LOG_MSG ("netfs_attempt_rename");

  /*Operation not supported */
  return EOPNOTSUPP;
}				/*netfs_attempt_rename */

/*---------------------------------------------------------------------------*/
/*Attempts to create a new directory*/
error_t
  netfs_attempt_mkdir
  (struct iouser * user, struct node * dir, char *name, mode_t mode)
{
  LOG_MSG ("netfs_attempt_mkdir");

  return 0;
}				/*netfs_attempt_mkdir */

/*---------------------------------------------------------------------------*/
/*Attempts to remove directory `name` in `dir` for `user`*/
error_t
  netfs_attempt_rmdir (struct iouser * user, struct node * dir, char *name)
{
  LOG_MSG ("netfs_attempt_rmdir");

  return 0;
}				/*netfs_attempt_rmdir */

/*---------------------------------------------------------------------------*/
/*Attempts to change the mode of `node` for user `cred` to `uid`:`gid`*/
error_t
  netfs_attempt_chown
  (struct iouser * cred, struct node * node, uid_t uid, uid_t gid)
{
  LOG_MSG ("netfs_attempt_chown");

  /*Operation is not supported */
  return EOPNOTSUPP;
}				/*netfs_attempt_chown */

/*---------------------------------------------------------------------------*/
/*Attempts to change the author of `node` to `author`*/
error_t
  netfs_attempt_chauthor
  (struct iouser * cred, struct node * node, uid_t author)
{
  LOG_MSG ("netfs_attempt_chauthor");

  /*Operation is not supported */
  return EOPNOTSUPP;
}				/*netfs_attempt_chauthor */

/*---------------------------------------------------------------------------*/
/*Attempts to change the mode of `node` to `mode` for `cred`*/
error_t
  netfs_attempt_chmod (struct iouser * user, struct node * node, mode_t mode)
{
  LOG_MSG ("netfs_attempt_chmod");

  /*Operation is not supported */
  return EOPNOTSUPP;
}				/*netfs_attempt_chmod */

/*---------------------------------------------------------------------------*/
/*Attempts to turn `node` into a symlink targetting `name`*/
error_t
  netfs_attempt_mksymlink
  (struct iouser * cred, struct node * node, char *name)
{
  LOG_MSG ("netfs_attempt_mksymlink");

  /*Operation is not supported */
  return EOPNOTSUPP;
}				/*netfs_attempt_mksymlink */

/*---------------------------------------------------------------------------*/
/*Attempts to turn `node` into a device; type can be either S_IFBLK or
  S_IFCHR*/
error_t
  netfs_attempt_mkdev
  (struct iouser * cred, struct node * node, mode_t type, dev_t indexes)
{
  LOG_MSG ("netfs_attempt_mkdev");

  /*Operation is not supported */
  return EOPNOTSUPP;
}				/*netfs_attempt_mkdev */

/*---------------------------------------------------------------------------*/
/*Attempts to set the passive translator record for `file` passing `argz`*/
error_t
  netfs_set_translator
  (struct iouser * cred, struct node * node, char *argz, size_t arglen)
{
  LOG_MSG ("netfs_set_translator");

  /*Operation is not supported */
  return EOPNOTSUPP;
}				/*netfs_set_translator */

/*---------------------------------------------------------------------------*/
/*Attempts to call chflags for `node`*/
error_t
  netfs_attempt_chflags (struct iouser * cred, struct node * node, int flags)
{
  LOG_MSG ("netfs_attempt_chflags");

  /*Operation is not supported */
  return EOPNOTSUPP;
}				/*netfs_attempt_chflags */

/*---------------------------------------------------------------------------*/
/*Attempts to set the size of file `node`*/
error_t
  netfs_attempt_set_size
  (struct iouser * cred, struct node * node, loff_t size)
{
  LOG_MSG ("netfs_attempt_set_size");

  /*Operation is not supported */
  return EOPNOTSUPP;
}				/*netfs_attempt_set_size */

/*---------------------------------------------------------------------------*/
/*Fetches the filesystem status information*/
error_t
  netfs_attempt_statfs
  (struct iouser * cred, struct node * node, fsys_statfsbuf_t * st)
{
  LOG_MSG ("netfs_attempt_statfs");

  /*Operation is not supported */
  return EOPNOTSUPP;
}				/*netfs_attempt_statfs */

/*---------------------------------------------------------------------------*/
/*Syncs the filesystem*/
error_t netfs_attempt_syncfs (struct iouser * cred, int wait)
{
  LOG_MSG ("netfs_attempt_syncfs");

  /*Everythin OK */
  return 0;
}				/*netfs_attempt_syncfs */

/*---------------------------------------------------------------------------*/
/*Creates a link in `dir` with `name` to `file`*/
error_t
  netfs_attempt_link
  (struct iouser * user,
   struct node * dir, struct node * file, char *name, int excl)
{
  LOG_MSG ("netfs_attempt_link");

  /*Operation not supported */
  return EOPNOTSUPP;
}				/*netfs_attempt_link */

/*---------------------------------------------------------------------------*/
/*Attempts to create an anonymous file related to `dir` with `mode`*/
error_t
  netfs_attempt_mkfile
  (struct iouser * user, struct node * dir, mode_t mode, struct node ** node)
{
  LOG_MSG ("netfs_attempt_mkfile");

  /*Unlock the directory */
  mutex_unlock (&dir->lock);

  /*Operation not supported */
  return EOPNOTSUPP;
}				/*netfs_attempt_mkfile */

/*---------------------------------------------------------------------------*/
/*Reads the contents of symlink `node` into `buf`*/
error_t
  netfs_attempt_readlink (struct iouser * user, struct node * node, char *buf)
{
  LOG_MSG ("netfs_attempt_readlink");

  /*Operation not supported (why?..) */
  return EOPNOTSUPP;
}				/*netfs_attempt_readlink */

/*---------------------------------------------------------------------------*/
/*Reads from file `node` up to `len` bytes from `offset` into `data`*/
error_t
  netfs_attempt_read
  (struct iouser * cred,
   struct node * np, loff_t offset, size_t * len, void *data)
{
  LOG_MSG ("netfs_attempt_read");

  error_t err = 0;

  /*Obtain a pointer to the first byte of the supplied buffer */
  char *buf = data;

  /*Try to read the requested information from the file */
  err = io_read (np->nn->port, &buf, len, offset, *len);

  /*If some data has been read successfully */
  if (!err && (buf != data))
    {
      /*copy the data from the buffer into which is has been read into the
         supplied receiver */
      memcpy (data, buf, *len);

      /*unmap the new buffer */
      munmap (buf, *len);
    }

  /*Return the result of reading */
  return err;
}				/*netfs_attempt_read */

/*---------------------------------------------------------------------------*/
/*Writes to file `node` up to `len` bytes from offset from `data`*/
error_t
  netfs_attempt_write
  (struct iouser * cred,
   struct node * node, loff_t offset, size_t * len, void *data)
{
  /*Write the supplied data into the file and return the result */
  return io_write (node->nn->port, data, *len, offset, len);
}				/*netfs_attempt_write */

/*---------------------------------------------------------------------------*/
/*Frees all storage associated with the node*/
void netfs_node_norefs (struct node *np)
{
  /*Destroy the node */
  node_destroy (np);
}				/*netfs_node_norefs */

/*---------------------------------------------------------------------------*/
/*Implements file_get_translator_cntl as described in <hurd/fs.defs>
  (according to diskfs_S_file_get_translator_cntl)*/
kern_return_t
  netfs_S_file_get_translator_cntl
  (struct protid *user, mach_port_t * cntl, mach_msg_type_name_t * cntltype)
{
  /*If the information about the user is missing */
  if (!user)
    return EOPNOTSUPP;

  error_t err = 0;

  /*Obtain the node for which we are called */
  node_t *np = user->po->np;

  /*If the node is not the root node of nsmux */
  if (np != netfs_root_node)
    {
      /*TODO: The functionality here will be provided later */
    }

  /*Lock the node */
  mutex_lock (&np->lock);

  /*Check if the user is the owner of this node */
  err = fshelp_isowner (&np->nn_stat, user->user);

  /*If no errors have happened */
  if (!err)
    /*try to fetch the control port */
    err = fshelp_fetch_control (&np->transbox, cntl);

  /*If no errors have occurred, but no port has been returned */
  if (!err && (cntl == MACH_PORT_NULL))
    /*set the error accordingly */
    err = ENXIO;

  /*If no errors have occurred so far */
  if (!err)
    /*set the control port type */
    *cntltype = MACH_MSG_TYPE_MOVE_SEND;

  /*Unlock the node */
  mutex_unlock (&np->lock);

  /*Return the result of operations */
  return err;
}				/*netfs_S_file_get_translator_cntl */

/*---------------------------------------------------------------------------*/
/*Entry point*/
int main (int argc, char **argv)
{
  /*Start logging */
  INIT_LOG ();
  LOG_MSG (">> Starting initialization...");

  /*The port on which this translator will be set upon */
  mach_port_t bootstrap_port;

  error_t err = 0;

  /*Parse the command line arguments */
  argp_parse (&argp_startup, argc, argv, ARGP_IN_ORDER, 0, 0);
  LOG_MSG ("Command line arguments parsed.");

  /*Try to create the root node */
  err = node_create_root (&netfs_root_node);
  if (err)
    error (EXIT_FAILURE, err, "Failed to create the root node");
  LOG_MSG ("Root node created.");

  /*Obtain the bootstrap port */
  task_get_bootstrap_port (mach_task_self (), &bootstrap_port);

  /*Initialize the translator */
  netfs_init ();

  /*Obtain a port to the underlying node */
  underlying_node = netfs_startup (bootstrap_port, O_READ);
  LOG_MSG ("netfs initialization complete.");

  /*Initialize the root node */
  err = node_init_root (netfs_root_node);
  if (err)
    error (EXIT_FAILURE, err, "Failed to initialize the root node");
  LOG_MSG ("Root node initialized.");
  LOG_MSG ("\tRoot node address: 0x%lX", (unsigned long) netfs_root_node);

  /*Map the time for updating node information */
  err = maptime_map (0, 0, &maptime);
  if (err)
    error (EXIT_FAILURE, err, "Failed to map the time");
  LOG_MSG ("Time mapped.");

  /*Initialize the cache with the required number of nodes */
  ncache_init ( /*ncache_size */ );
  LOG_MSG ("Cache initialized.");

  /*Obtain stat information about the underlying node */
  err = io_stat (underlying_node, &underlying_node_stat);
  if (err)
    error (EXIT_FAILURE, err,
	   "Cannot obtain stat information about the underlying node");
  LOG_MSG ("Stat information for undelying node obtained.");

  /*Obtain the ID of the current process */
  fsid = getpid ();

  /*Setup the stat information for the root node */
  netfs_root_node->nn_stat = underlying_node_stat;

  netfs_root_node->nn_stat.st_ino = NSMUX_ROOT_INODE;
  netfs_root_node->nn_stat.st_fsid = fsid;
  /*we are providing a translated directory */
  netfs_root_node->nn_stat.st_mode =
    S_IFDIR | (underlying_node_stat.st_mode & ~S_IFMT & ~S_ITRANS);

  netfs_root_node->nn_translated = netfs_root_node->nn_stat.st_mode;

  /*If the underlying node is not a directory, enhance the permissions
     of the root node of the proxy filesystem */
  if (!S_ISDIR (underlying_node_stat.st_mode))
    {
      /*can be read by owner */
      if (underlying_node_stat.st_mode & S_IRUSR)
	/*allow execution by the owner */
	netfs_root_node->nn_stat.st_mode |= S_IXUSR;
      /*can be read by group */
      if (underlying_node_stat.st_mode & S_IRGRP)
	/*allow execution by the group */
	netfs_root_node->nn_stat.st_mode |= S_IXGRP;
      /*can be read by others */
      if (underlying_node_stat.st_mode & S_IROTH)
	/*allow execution by the others */
	netfs_root_node->nn_stat.st_mode |= S_IXOTH;
    }

  /*Update the timestamps of the root node */
  fshelp_touch
    (&netfs_root_node->nn_stat, TOUCH_ATIME | TOUCH_MTIME | TOUCH_CTIME,
     maptime);

  LOG_MSG (">> Initialization complete. Entering netfs server loop...");

  /*Start serving clients */
  for (;;)
    netfs_server_loop ();
}				/*main */

/*---------------------------------------------------------------------------*/
