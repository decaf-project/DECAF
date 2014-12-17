/*
 * parser.c - ARM instruction parse functions
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "libdisarm_endian.h"
#include "libdisarm_macros.h"
#include "libdisarm_types.h"


/* Figure 3-2 in ARM Architecture Reference */
static da_group_t
da_parse_group_mul_ls(da_word_t data)
{
	if ((data >> 6) & 1) {
		if ((data >> 20) & 1) {
			if ((data >> 22) & 1) {
				return DA_GROUP_L_SIGN_IMM;
			} else return DA_GROUP_L_SIGN_REG;
		} else if ((data >> 22) & 1) {
			return DA_GROUP_LS_TWO_IMM;
		} else return DA_GROUP_LS_TWO_REG;
	} else {
		if ((data >> 5) & 1) {
			if ((data >> 22) & 1) {
				return DA_GROUP_LS_HW_IMM;
			} else return DA_GROUP_LS_HW_REG;
		} else {
			if ((data >> 24) & 1) return DA_GROUP_SWP;
			else if ((data >> 23) & 1) {
				return DA_GROUP_MULL;
			} else return DA_GROUP_MUL;
		}
	}
}

static da_group_t
da_parse_group_type_0(da_word_t data)
{
	if ((data >> 4) & 1) {
		if ((data >> 7) & 1) return da_parse_group_mul_ls(data);
		else if ((((data >> 23) & 0x3) == 0x2) &&
			 (((data >> 20) & 1) == 0)) {
			if ((data >> 6) & 1) {
				if ((data >> 5) & 1) return DA_GROUP_BKPT;
				else return DA_GROUP_DSP_ADD_SUB;
			} else {
				if ((data >> 22) & 1) return DA_GROUP_CLZ;
				else return DA_GROUP_BLX_REG;
			}
		} else return DA_GROUP_DATA_REG_SH;
	} else {
		if ((((data >> 23) & 0x3) == 0x2) &&
		    (((data >> 20) & 1) == 0)) {
			if ((data >> 7) & 1) return DA_GROUP_DSP_MUL;
			else if ((data >> 21) & 1) return DA_GROUP_MSR;
			else return DA_GROUP_MRS;
		} else return DA_GROUP_DATA_IMM_SH;
	}
}

static da_group_t
da_parse_group_cond(da_word_t data)
{
	switch ((data >> 25) & 0x7) {
	case 0: return da_parse_group_type_0(data);
	case 1:
		if ((((data >> 23) & 0x3) == 0x2) &&
		    (((data >> 20) & 1) == 0)) {
			if ((data >> 21) & 1) {
				return DA_GROUP_MSR_IMM;
			} else return DA_GROUP_UNDEF_1;
		} else return DA_GROUP_DATA_IMM;
	case 2: return DA_GROUP_LS_IMM;
	case 3:
		if ((data >> 4) & 1) return DA_GROUP_UNDEF_2;
		else return DA_GROUP_LS_REG;
	case 4: return DA_GROUP_LS_MULTI;
	case 5: return DA_GROUP_BL;
	case 6: return DA_GROUP_CP_LS;
	case 7:
		if ((data >> 24) & 1) return DA_GROUP_SWI;
		else if ((data >> 4) & 1) return DA_GROUP_CP_REG;
		else return DA_GROUP_CP_DATA;
	default:
		assert(0); /* Control shouldn't reach here. */
	}
}

static da_group_t
da_parse_group(da_word_t data)
{
	switch ((data >> 28) & 0xf) {
	case 0xf:
		switch ((data >> 25) & 0x7) {
		case 0: 
		case 1:
		case 2:
		case 3: return DA_GROUP_UNDEF_3;
		case 4:	return DA_GROUP_UNDEF_4;
		case 5:	return DA_GROUP_BLX_IMM;
		case 6:
		case 7:	return DA_GROUP_UNDEF_5;
	}
	default: return da_parse_group_cond(data);
	}
}

DA_API void
da_instr_parse(da_instr_t *instr, da_word_t data, int big_endian)
{
	instr->data = (big_endian ? be32toh(data) : le32toh(data));
	instr->group = da_parse_group(instr->data);
}
