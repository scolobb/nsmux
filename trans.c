/*---------------------------------------------------------------------------*/
/*trans.c*/
/*---------------------------------------------------------------------------*/
/*Facilities to keep track of dynamic translators.*/
/*---------------------------------------------------------------------------*/
/*Copyright (C) 2008, 2009 Free Software Foundation, Inc.  Written by
  Sergiu Ivanov <unlimitedscolobb@gmail.com>.

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
#include <stdlib.h>
#include <hurd/fsys.h>
#include <sys/wait.h>
/*---------------------------------------------------------------------------*/
#include "trans.h"
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*---------Variables---------------------------------------------------------*/
/*The list of dynamic translators */
trans_el_t * dyntrans = NULL;
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*---------Functions---------------------------------------------------------*/
/*Adds a translator to the list of translators. One should use only
  this function to add a new element to the list. */
error_t
trans_register (fsys_t cntl, pid_t pid, trans_el_t ** new_trans)
{
  /*The new entry in the list */
  trans_el_t * el;

  el = malloc (sizeof (trans_el_t));
  if (!el)
    return ENOMEM;

  el->cntl = cntl;
  el->pid = pid;

  el->prev = NULL;
  el->next = dyntrans;
  dyntrans = el;

  *new_trans = el;

  return 0;
}				/*trans_register */

/*---------------------------------------------------------------------------*/
/*Removes a translator from the list. One should use only this
  function to remove an element from the list. This function does not
  shut down the translator. */
void
trans_unregister (trans_el_t * trans)
{
  if (trans->prev)
    trans->prev->next = trans->next;
  if(trans->next)
    trans->next->prev = trans->prev;

  free(trans);
}				/*trans_unregister */

/*---------------------------------------------------------------------------*/
/*Gracefully shuts down all the translators registered in the list
  with `flags`. If wait is nonzero, waits for each translator to
  finish. */
error_t
trans_shutdown_all (int flags, int wait)
{
  error_t err;

  /*The element being dealt with now. */
  trans_el_t * el;

  /*The exit status of the dynamic translator, in case we are waiting
    for it to finish. */
  int exit_status;
  
  for (el = dyntrans; el; el = el->next)
    {
      err = fsys_goaway (el->cntl, flags);

      if (err)
	{
	  /*The error cannot happen because the translators are being
	    shut down in the incorrect order, so something is
	    wrong. Stop and update dyntrans. */

	  dyntrans = el;
	  return err;
	}

      if (wait)
	{
	  /*Wait for the translator process to stop, if needed. */
	  waitpid (el->pid, &exit_status, 0);
	}
    }

  dyntrans = NULL;
  return 0;
}				/*trans_shutdown_all */

/*---------------------------------------------------------------------------*/
