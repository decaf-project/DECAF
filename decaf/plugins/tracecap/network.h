/* 
   Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
   All rights reserved.
   Do not copy, disclose, or distribute without explicit written
   permission. 

   Author: Juan Caballero <jcaballero@cmu.edu>
*/
#ifndef _NETWORK_H_
#define _NETWORK_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/* Functions */
void internal_do_taint_nic(Monitor *mon, int state);
void do_taint_nic(Monitor *mon, const QDict *qdict);
void print_nic_filter (void);
int update_nic_filter (const char *filter_str, const char *value_str);
void tracing_nic_recv(DECAF_Callback_Params* params);
void tracing_nic_send(DECAF_Callback_Params* params);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _NETWORK_H_

