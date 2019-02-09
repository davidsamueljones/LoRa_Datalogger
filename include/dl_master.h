#ifndef DL_MASTER_H
#define DL_MASTER_H

#ifdef DL_MASTER

// Pass an alternative during compilation if multiple of each board type
#ifndef ARG_BOARD_ID
#define ARG_BOARD_ID (__COUNTER__)
#endif
// Top bit not set for Masters
#define BOARD_ID (uint8_t) (ARG_BOARD_ID & 0x7F)

void setup();

void loop();
#endif

#endif // DL_MASTER_H
