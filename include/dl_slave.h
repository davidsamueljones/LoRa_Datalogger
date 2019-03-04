#ifndef DL_SLAVE_H
#define DL_SLAVE_H

#include <Arduino.h>

bool dl_slave_setup(void);
bool dl_slave_loop(void);
void dl_slave_recv_and_execute_cmd(void);
bool dl_slave_handle_testdef_cmd(uint8_t master_id);

#endif // DL_SLAVE_H
