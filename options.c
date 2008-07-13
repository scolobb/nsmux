/*----------------------------------------------------------------------------*/
/*options.h*/
/*----------------------------------------------------------------------------*/
/*Definitions for parsing the command line switches*/
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
#include <argp.h>
#include <error.h>
/*----------------------------------------------------------------------------*/
#include "debug.h"
#include "options.h"
#include "ncache.h"
#include "node.h"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Macros--------------------------------------------------------------*/
/*Short documentation for argp*/
#define ARGS_DOC	"DIR"
#define DOC 			"Provides namespace-based translator selection."\
	" You can dynamically obtain the file 'file' translated by translator"\
	" 'x' using the syntax: 'file,,x'."
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Forward Declarations------------------------------------------------*/
/*Argp parser function for the common options*/
static
error_t
argp_parse_common_options
	(
	int key,
	char * arg,
	struct argp_state * state
	);
/*----------------------------------------------------------------------------*/
/*Argp parser function for the startup options*/
static
error_t
argp_parse_startup_options
	(
	int key,
	char * arg,
	struct argp_state * state
	);
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Global Variables----------------------------------------------------*/
/*This variable is set to a non-zero value after the parsing of starup options
	is finished*/
/*Whenever the argument parser is later called to modify the
	options of the root node will be initialized accordingly directly
	by the parser*/
static int parsing_startup_options_finished;
/*----------------------------------------------------------------------------*/
/*Argp options common to both the runtime and the startup parser*/
static const struct argp_option argp_common_options[] =
	{
	{OPT_LONG_CACHE_SIZE, OPT_CACHE_SIZE, "SIZE", 0,
		"The maximal number of nodes in the node cache"}
	};
/*----------------------------------------------------------------------------*/
/*Argp options only meaningful for startup parsing*/
static const struct argp_option argp_startup_options[] =
	{
	{0}
	};
/*----------------------------------------------------------------------------*/
/*Argp parser for only the common options*/
static const struct argp argp_parser_common_options =
	{argp_common_options, argp_parse_common_options, 0, 0, 0};
/*----------------------------------------------------------------------------*/
/*Argp parser for only the startup options*/
static const struct argp argp_parser_startup_options =
	{argp_startup_options, argp_parse_startup_options, 0, 0, 0};
/*----------------------------------------------------------------------------*/
/*The list of children parsers for runtime arguments*/
static const struct argp_child argp_children_runtime[] =
	{
	{&argp_parser_common_options},
	{&netfs_std_runtime_argp},
	{0}
	};
/*----------------------------------------------------------------------------*/
/*The list of children parsers for startup arguments*/
static const struct argp_child argp_children_startup[] =
	{
	{&argp_parser_startup_options},
	{&argp_parser_common_options},
	{&netfs_std_startup_argp},
	{0}
	};
/*----------------------------------------------------------------------------*/
/*The version of the server for argp*/
const char * argp_program_version = "0.0";
/*----------------------------------------------------------------------------*/
/*The arpg parser for runtime arguments*/
struct argp argp_runtime =
	{0, 0, 0, 0, argp_children_runtime};
/*----------------------------------------------------------------------------*/
/*The argp parser for startup arguments*/
struct argp argp_startup =
	{0, 0, ARGS_DOC, DOC, argp_children_startup};
/*----------------------------------------------------------------------------*/
/*The directory to mirror*/
char * dir = NULL;
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Argp parser function for the common options*/
static
error_t
argp_parse_common_options
	(
	int key,
	char * arg,
	struct argp_state * state
	)
	{
	error_t err = 0;
	
	/*Go through the possible options*/
	switch(key)
		{
		case OPT_CACHE_SIZE:
			{
			/*store the new cache-size*/
			ncache_size = strtol(arg, NULL, 10);
			
			break;
			}
		case ARGP_KEY_ARG: /*the directory to mirror*/
			{
			/*try to duplicate the directory name*/
			dir = strdup(arg);
			if(!dir)
				error(EXIT_FAILURE, ENOMEM, "argp_parse_common_options: "
					"Could not strdup the directory");
				
			/*fill all trailing spaces with 0*/
			int i = strlen(dir) - 1;
			/*for(i = strlen(dir) - 1; (i >= 0) && (dir[i] == ' '); dir[i--] = 0);*/
				/*the original filename may contain spaces*/
			
			/*If the last non blank symbol is a '/' and it's not the only one*/
			if((dir[i] == '/') && (i != 0))
				/*put 0 instead*/
				dir[i] = 0;
				
			LOG_MSG("argp_parse_common_options: Mirroring the directory %s.", dir);

			break;
			}
		case ARGP_KEY_END:
			{
			/*If parsing of startup options has not finished*/
			if(!parsing_startup_options_finished)
				{
				/*reset the cache*/
				ncache_reset();
				
				/*If the directory has not been specified*/
				if(!dir)
					{
					/*assume the directory to be the home directory*/
					;

					/*FIXME: The default directory is /var/tmp*/
					dir = "/var/tmp";
					}
				
				/*set the flag that the startup options have already been parsed*/
				parsing_startup_options_finished = 1;
				}
			else
				{
/*TODO: Take care of runtime calls modifying the property*/
				}
			}
		/*If the option could not be recognized*/
		default:
			{
			/*set the error code*/
			err = ARGP_ERR_UNKNOWN;
			}
		}
		
	/*Return the result*/
	return err;
	}/*argp_parse_common_options*/
/*----------------------------------------------------------------------------*/
/*Argp parser function for the startup options*/
static
error_t
argp_parse_startup_options
	(
	int key,
	char * arg,
	struct argp_state * state
	)
	{
	/*Do nothing in a beautiful way*/
	error_t err = 0;
	
	switch(key)
		{
		default:
			{
			err = ARGP_ERR_UNKNOWN;
			
			break;
			}
		}
	
	return err;
	}/*argp_parse_startup_options*/
/*----------------------------------------------------------------------------*/
