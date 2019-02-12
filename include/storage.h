#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <SdFat.h>

#include "radio.h"
#define TESTDEF_DIR  "/testdefs/"
#define RESULTS_DIR  "/results/"

#define TESTDEF_FORMAT_FILE TESTDEF_DIR "_format.txt"
#define LOG_FILE "log.txt"

bool storage_init(void);

bool storage_master_defaults(void);
bool storage_slave_defaults(void);

bool storage_load_testdef(char* path, lora_testdef_t *testdef);

bool is_storage_initialised(void);

extern SdFatSdio SD;

#endif // STORAGE_H