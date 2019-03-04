#include <Arduino.h>

#include "dl_common.h"
#include "dl_master.h"
#include "dl_slave.h"

void setup() {
  // Do general boot procedure for all boards (includes finding out board type)
  bool boot_success = dl_common_boot(dl_common_set_interrupts);
  // Do board specific boot sequence
  if (boot_success) {
    uint8_t board_id = dl_common_get_board_id();
    if (IS_MASTER_BOARD(board_id)) {
      boot_success &= dl_master_setup();
    }
    if (IS_SLAVE_BOARD(board_id)) {
      boot_success &= dl_slave_setup();
    }
  }
  // Report whether boot has successed, will block if boot_success is false
  dl_common_finish_boot(boot_success);                           
}

void loop() {
  uint8_t board_id = dl_common_get_board_id();
  if (IS_MASTER_BOARD(board_id)) {
    dl_master_loop();
  }
  if (IS_SLAVE_BOARD(board_id)) {
    dl_slave_loop();
  }
}
