#ifndef DL_COMMON_H
#define DL_COMMON_H

#include <Arduino.h>

// Reserved bit for identifying a master
#define MASTER_ID_FLAG (0x80)
// Reserved bit for identifying a slave
#define SLAVE_ID_FLAG  (0x40)
// Mask to capture actual id part
#define BOARD_ID_MASK (0x6F)

// Choose the correct flag for this compilation
#ifdef DL_MASTER
#define BOARD_ID_FLAG  MASTER_ID_FLAG
#define BOARD_TYPE "Master"
#define BOARD_TYPE_LED BO_LED_3
#endif
#ifdef DL_SLAVE
#define BOARD_ID_FLAG  SLAVE_ID_FLAG
#define BOARD_TYPE "Slave"
#define BOARD_TYPE_LED  BO_LED_1
#endif

// Pass an alternative during compilation if multiple of each board type
#ifndef ARG_BOARD_ID
#define ARG_BOARD_ID (__COUNTER__)
#endif

// Define the unique board id
#define BOARD_ID (BOARD_ID_FLAG + (uint8_t) (ARG_BOARD_ID & BOARD_ID_MASK))

bool dl_common_boot(void switch_isr(void));

void dl_common_finish_boot(bool boot_success);

// Call dl_common_set_interrupts with default true argument, can be used as an
// ISR to trigger all known common interrupts
void dl_common_set_interrupts(void);

void dl_common_set_interrupts(bool value);

bool dl_common_check_interrupts(bool clear = false);


#endif // DL_COMMON_H