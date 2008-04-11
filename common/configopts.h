/*
 * configopts.h - configuration options header
 *
 * Copyright © 2007-2008 Julien Danjou <julien@danjou.info>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef AWESOME_COMMON_CONFIGOPTS_H
#define AWESOME_COMMON_CONFIGOPTS_H

#include <confuse.h>

cfg_t * cfg_new(void);
char * config_file(void);
int config_check(const char *);

alignment_t cfg_opt_getnalignment(cfg_opt_t *, unsigned int);
alignment_t cfg_getnalignment(cfg_t *, const char *, unsigned int);
alignment_t cfg_getalignment(cfg_t *, const char *);

position_t cfg_opt_getnposition(cfg_opt_t *, unsigned int);
position_t cfg_getnposition(cfg_t *, const char *, unsigned int);
position_t cfg_getposition(cfg_t *, const char *);

#endif

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
