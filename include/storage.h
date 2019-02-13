#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <SdFat.h>

#include "radio.h"
#define TESTDEF_DIR  "/testdefs/"
#define RESULTS_DIR  "/results/"

#define TESTDEF_FORMAT_FILE TESTDEF_DIR "_format.txt"
#define LOG_FILE "log.txt"

extern SdFatSdio SD;

bool storage_init(void);

bool storage_master_defaults(void);
bool storage_slave_defaults(void);

bool storage_load_testdef(File* file, lora_testdef_t *testdef);
bool storage_load_testdef(char* path, lora_testdef_t *testdef);

uint8_t storage_load_testdefs(lora_testdef_t testdefs[], uint8_t arr_len);

File storage_init_result_file(char* filename);
bool storage_write_result(File *file, uint8_t id, int16_t rssi, int16_t snr);
bool is_storage_initialised(void);

#endif // STORAGE_H