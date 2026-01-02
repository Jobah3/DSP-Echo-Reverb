/*
 * HAOS_emulation.h
 *
 *  Created on: May 24, 2024
 *      Author: tmilica
 */

#ifndef _HAOS_EMULATION_H_
#define _HAOS_EMULATION_H_

/// initialize emulation environment with command line parameters
///
/// Call this once from the beginning of your emulation main() function.
extern void HAOS_init( int argc, const char * argv[]);

/// add a set of algorithms (modules) to be run
///
/// Call this as many times as you have modules to run. The actual type of
/// module_list is platform dependent.
//extern void cl_os_add_modules( void * module_list);
extern void HAOS_add_modules( void* moduleList);

/// emulate the OS
///
/// call this when you want the OS to emulate, which should schedule
/// the algorithms added via calls to cl_os_add_modules().
extern void HAOS_run();

#endif /* _HAOS_EMULATION_H_ */
