#ifndef DL_SLAVE_H
#define DL_SLAVE_H

#ifdef DL_SLAVE

// Pass an alternative during compilation if multiple of each board type
#ifndef ARG_BOARD_ID
#define ARG_BOARD_ID (__COUNTER__)
#endif
// Top bit set for Slaves
#define BOARD_ID (0x80 + (uint8_t) (ARG_BOARD_ID & 0x7F))

void setup();

void loop();
#endif

#endif // DL_SLAVE_H
