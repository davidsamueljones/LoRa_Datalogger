#ifndef DL_COMMON_H
#define DL_COMMON_H

#include <Arduino.h>

// Reserved bit for identifying a master
#define MASTER_ID_FLAG         (0x80)
#define MASTER_ID_BIT          (7)
// Reserved bit for identifying a slave
#define SLAVE_ID_FLAG          (0x40)
#define SLAVE_ID_BIT           (6)
// Mask to capture actual id part
#define BOARD_ID_MASK          (0x6F)
// Macro to check if board is master
#define IS_MASTER_BOARD(ID)    (ID & MASTER_ID_FLAG)
// Macro to check if board is master
#define IS_SLAVE_BOARD(ID)     (ID & SLAVE_ID_FLAG)
// Macro to check if one of master or slave bit is set but not both
#define IS_VALID_BOARD_ID(ID)  ((IS_MASTER_BOARD(ID) >> MASTER_ID_BIT) ^ \
                                (IS_SLAVE_BOARD(ID) >> SLAVE_ID_BIT))

// String identifier for a master
#define MASTER_BOARD_STR_ID "Master"
// String identifier for a slave
#define SLAVE_BOARD_STR_ID   "Slave"
// String identifier for an invalid board
#define INVALID_BOARD_STR_ID "INVALID BOARD"
// Macro to select the correct string identifier for the given board
#define GET_BOARD_STR_ID(ID) (!IS_VALID_BOARD_ID(ID) ? INVALID_BOARD_STR_ID : \
                              (IS_MASTER_BOARD(ID) ? MASTER_BOARD_STR_ID : SLAVE_BOARD_STR_ID))

// LED to use as master identifier
#define MASTER_LED  BO_LED_3
// LED to use as slave identifier
#define SLAVE_LED   BO_LED_1
// LED for invalid boards
#define INVALID_LED BO_LED_2
// Macro to select the correct LED identifier for the given board
#define GET_BOARD_LED(ID) (!IS_VALID_BOARD_ID(ID) ? INVALID_LED : \
                           (IS_MASTER_BOARD(ID) ? MASTER_LED : SLAVE_LED))

// EEPROM locations of relevant bytes for UID checks
#define IDX_IDENTIFIER_BYTE_1 (0)
#define IDX_IDENTIFIER_BYTE_2 (1)
#define IDX_BOARD_ID          (2)

// Unique pattern (byte 1) for checking existence of Board ID
#define IDENTIFIER_BYTE_1 0x5E
// Unique pattern (byte 2) for checking existence of Board ID
#define IDENTIFIER_BYTE_2 0x1F

// A board identifier that is known to be invalid
#define INVALID_BOARD_ID (0xFF)
#if IS_VALID_BOARD_ID(INVALID_BOARD_ID)
#error "Invalid Board ID is defined as valid!"
#endif

bool dl_common_boot(void switch_isr(void));

void dl_common_finish_boot(bool boot_success);

uint8_t dl_common_get_board_id(void);

// Call dl_common_set_interrupts with default true argument, can be used as an
// ISR to trigger all known common interrupts
void dl_common_set_interrupts(void);

void dl_common_set_interrupts(bool value);

bool dl_common_check_interrupts(bool clear = false);


#endif // DL_COMMON_H