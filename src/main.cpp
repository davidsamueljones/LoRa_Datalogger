/*
  Top level container for including correct target main.
  @author David Jones (dsj1n15)
*/
#if defined(DL_MASTER) && defined(DL_SLAVE)
#error Cannot compile as both DL_MASTER and DL_SLAVE
#endif

// Include appropriate 
#ifdef DL_MASTER
#include "dl_master.h"
#endif
#ifdef DL_SLAVE
#include "dl_slave.h"
#endif