#ifndef DL_MASTER_H
#define DL_MASTER_H

#include <Arduino.h>
#include "radio.h"

bool dl_master_setup(void);
bool dl_master_loop(void);
void dl_master_run_testdefs(void);
bool dl_master_run_testdef(lora_testdef_t *testdef, uint16_t* recv_packets, File* log_file);
void dl_master_send_heartbeats(void);

#endif // DL_MASTER_H
