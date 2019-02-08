/*
  Top level container for including correct target main.
  @author David Jones (dsj1n15)
*/
#if defined(DL_MASTER) && defined(DL_SLAVE)
#error Cannot compile as both DL_MASTER and DL_SLAVE
#endif

// Pass an alternative during compilation if multiple of each board type
// TODO: Could use __COUNTER__ macro to do automatically
#ifndef ARG_BOARD_ID
#define ARG_BOARD_ID (1)
#endif

// Include appropriate 
#ifdef DL_MASTER
#include "dl_master.h"
// Top bit not set for Masters
#define BOARD_ID (uint8_t) (ARG_BOARD_ID & 0x7F)
#endif
#ifdef DL_SLAVE
#include "dl_slave.h"
// Top bit set for Slaves
#define BOARD_ID (0x80 + (uint8_t) (ARG_BOARD_ID & 0x7F))
#endif