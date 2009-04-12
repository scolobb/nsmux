/*---------------------------------------------------------------------------*/
/*trans.h*/
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
#ifndef __DEBUG_H__
#define __DEBUG_H__

/*---------------------------------------------------------------------------*/
#include <hurd.h>
#include <error.h>
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*---------Types-------------------------------------------------------------*/
/*An element in the list of dynamic translators */
struct trans_el
{
  /*the control port to the translator */
  mach_port_t cntl;

  /*the next and the previous elements in the list */
  struct trans_el * next, * prev;
};				/*struct trans_el */
/*---------------------------------------------------------------------------*/
typedef struct trans_el trans_el_t;
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*---------Variables---------------------------------------------------------*/
/*The list of dynamic translators */
extern trans_el_t * dyntrans;
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*---------Functions---------------------------------------------------------*/
/*Adds a translator control port to the list of ports. One should use
  only this function to add a new element to the list of ports. */
error_t
trans_register (mach_port_t cntl);
/*---------------------------------------------------------------------------*/
/*Removes a translator control port from the list of ports. One should
  use only this function to remove an element from the list of
  ports. This function does not shut down the translator. */
void
trans_unregister (trans_el_t * trans);
/*---------------------------------------------------------------------------*/
/*Gracefully shuts down all the translators registered in the
  list with `flags`. */
error_t
trans_shutdown_all (int flags);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
#endif /*__TRANS_H__*/
