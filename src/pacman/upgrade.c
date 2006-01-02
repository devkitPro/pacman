/*
 *  upgrade.c
 * 
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alpm.h>
/* pacman */
#include "list.h"
#include "add.h"
#include "upgrade.h"
#include "conf.h"

extern config_t *config;

int pacman_upgrade(list_t *targets)
{
	/* this is basically just a remove-then-add process. pacman_add() will */
	/* handle it */
	config->upgrade = 1;
	return(pacman_add(targets));
}


/* vim: set ts=2 sw=2 noet: */
