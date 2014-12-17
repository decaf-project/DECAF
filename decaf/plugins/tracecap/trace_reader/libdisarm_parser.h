/*
 * parser.c - ARM instruction parser header
 *
 * Copyright (C) 2007  Jon Lund Steffensen <jonlst@gmail.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LIBDISARM_PARSER_H
#define _LIBDISARM_PARSER_H

#include "libdisarm_macros.h"
#include "libdisarm_types.h"

DA_BEGIN_DECLS

void da_instr_parse(da_instr_t *instr, da_word_t data, int big_endian);

DA_END_DECLS

#endif /* ! _LIBDISARM_PARSER_H */
