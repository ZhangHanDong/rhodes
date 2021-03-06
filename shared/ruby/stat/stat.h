#ifndef DEBUG_STAT_H
#define DEBUG_STAT_H 1

//ruby vm statistics
//NOTE: define ENABLE_RUBY_VM_STAT to enable calculation of the statistics 

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RUBY_VM_STAT

extern int g_collect_stat;

extern unsigned long g_iseq_binread_sec; //time lapsed in binread (in seconds)
extern unsigned long g_iseq_binread_usec; //time lapsed in binread (in microseconds) 

extern unsigned long g_iseq_marshal_load_sec; //time lapsed in marshal load (in seconds)
extern unsigned long g_iseq_marshal_load_usec; //time lapsed in marshal load (in microseconds)

extern unsigned long g_require_compiled_sec;
extern unsigned long g_require_compiled_usec;

#endif

#ifdef __cplusplus
}
#endif

#endif