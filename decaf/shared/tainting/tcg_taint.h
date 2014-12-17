#ifndef __DECAF_TCG_TAINT_H__
#define __DECAF_TCG_TAINT_H__

#include <inttypes.h>
#include "tcg-op.h"

extern TCGv shadow_arg[TCG_MAX_TEMPS];
extern TCGv tempidx, tempidx2;
extern uint16_t *gen_old_opc_ptr;
extern TCGArg *gen_old_opparam_ptr;

extern int nb_tcg_sweeps;

extern void clean_shadow_arg(void);
extern int optimize_taint(int search_pc);
extern TCGv find_shadow_arg(TCGv arg);

#endif /* __DECAF_TCG_TAINT_H__ */

