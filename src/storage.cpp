#include <TimeLib.h>

#include "storage.h"

static void get_fat_date_time(uint16_t *date, uint16_t* time);

SdFatSdio SD;

static const char *TESTDEF_FIELDS[] = {"exp_range", "freq", "sf", "tx_dbm", "bw", "cr4_denom",
                                       "preamble_syms", "crc", "packet_cnt", "packet_len"};
#define FIELD_COUNT (uint8_t) (sizeof(TESTDEF_FIELDS) / sizeof(TESTDEF_FIELDS[1]))

static bool _initialised;

bool storage_init(void) {
  if (_initialised) {
    return false;
  }
  bool sd_init_success =  SD.begin();
  if (sd_init_success) {
    SdFile::dateTimeCallback(get_fat_date_time);
    _initialised = true;
  }
  return sd_init_success;
}

bool storage_master_defaults(void) {
  if (!_initialised) {
    return false;
  }
  // Create any directories that are expected to exist
  if (!SD.exists(TESTDEF_DIR)) {
    Serial.printf("Making testdef directory...\n");
    bool made_dir = SD.mkdir(TESTDEF_DIR);
    Serial.printf("Creation of directory '%s' %s!\n", TESTDEF_DIR, made_dir ? "successful" : "failed");
  }
  if (!SD.exists(RESULTS_DIR)) {
    Serial.printf("Making results directory...\n");
    bool made_dir = SD.mkdir(RESULTS_DIR);
    Serial.printf("Creation of directory '%s' %s!\n", RESULTS_DIR, made_dir ? "successful" : "failed");
  }
  // Create helper files for format explanations
  if (!SD.exists(TESTDEF_FORMAT_FILE)) {
    Serial.printf("Making example format file...\n");
    File file = SD.open(TESTDEF_FORMAT_FILE, FILE_WRITE);

    for (uint8_t field=0; field < FIELD_COUNT; field++) {
      file.write(TESTDEF_FIELDS[field]);
      if ((field + 1) < FIELD_COUNT) {
      file.write(",");
      }
    }
    file.close();
    Serial.printf("Creation of example testdef format successful!\n");
  }
  return true;
}

bool storage_slave_defaults(void) {
  if (!_initialised) {
    return false;
  }
  return true;
}

bool is_storage_initialised(void) {
  return _initialised;
}

static void get_fat_date_time(uint16_t* date, uint16_t* time) {
 // Convert and return date as FAT_DATE
 *date = FAT_DATE(year(), month(), day());
 // Convert and return time as FAT_TIME
 *time = FAT_TIME(hour(), minute(), second());
}
