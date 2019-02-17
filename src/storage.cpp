#include <TimeLib.h>

#include "storage.h"

static void extract_testdef_name(File* file, char *testdef_id);
static void get_fat_date_time(uint16_t *date, uint16_t* time);

#define MAX_TESTDEF_FILELEN (48)
static const char *TESTDEF_FIELDS[] = {"exp_range", "packet_cnt", "packet_len", "freq", "sf", "tx_dbm",
                                       "bw", "cr4_denom", "preamble_syms", "crc"};
#define TESTDEF_FIELD_COUNT (uint8_t) (sizeof(TESTDEF_FIELDS) / sizeof(TESTDEF_FIELDS[1]))

static const char *RECV_PACKETS_FIELDS[] = {"id", "rssi", "snr", "failed_recv", "time_left"};            
#define RECV_PACKETS_FIELD_COUNT (uint8_t) (sizeof(RECV_PACKETS_FIELDS) / sizeof(RECV_PACKETS_FIELDS[1]))   

SdFatSdio SD;
static bool _initialised;

typedef enum testdef_field_t {
  FIELD_EXP_RANGE = 0,
  FIELD_PACKET_CNT,
  FIELD_PACKET_LEN, 
  FIELD_FREQ, 
  FIELD_SF, 
  FIELD_TX_DBM, 
  FIELD_BW,
  FIELD_CR4_DENOM,
  FIELD_PREAMBLE_SYMS,
  FIELD_CRC,
} testdef_field_t;

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

    for (uint8_t field=0; field < TESTDEF_FIELD_COUNT; field++) {
      file.write(TESTDEF_FIELDS[field]);
      // TODO: Currently actually need a comma on the end, aim to remove this at some point
      file.write(",");
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

bool storage_load_testdef(File* file, lora_testdef_t *testdef) {
  // Get filename as test id
  extract_testdef_name(file, testdef->id); 
  // Extract each field as if csv format with an extra ',' on the end
  for (uint8_t field=0; field < TESTDEF_FIELD_COUNT; field++) {
    String str = file->readStringUntil(',');
    switch (field) {
      case FIELD_EXP_RANGE:
        testdef->exp_range = str.toInt();
        break;
      case FIELD_PACKET_CNT:
        testdef->packet_cnt = str.toInt();
        break;
      case FIELD_PACKET_LEN:
        testdef->packet_len = str.toInt();
        break;
      case FIELD_FREQ: 
        testdef->cfg.freq = str.toFloat();
        break;
      case FIELD_SF:
        testdef->cfg.sf = str.toInt();
        break;
      case FIELD_TX_DBM:
        testdef->cfg.tx_dbm = str.toInt();
        break;
      case FIELD_BW:
        testdef->cfg.bw = str.toInt();
        break;
      case FIELD_CR4_DENOM:
        testdef->cfg.cr4_denom = str.toInt();
        break;
      case FIELD_PREAMBLE_SYMS:
        testdef->cfg.preamble_syms = str.toInt();
        break;
      case FIELD_CRC:
        testdef->cfg.crc = str.toInt() ? true : false;
        break;
      default:
        return false;
    }
  }
  // TODO: Should probably do some form of validation
  return true;
}

bool storage_load_testdef(char* path, lora_testdef_t *testdef) {
  File file = SD.open(path, O_READ);
  storage_load_testdef(&file, testdef);
  file.close();
  return true;
}

uint8_t storage_load_testdefs(lora_testdef_t testdefs[], uint8_t arr_len) {
  File dir = SD.open(TESTDEF_DIR, O_RDONLY);
  File file;
  uint8_t n = 0;
  while (n < arr_len && file.openNext(&dir, O_RDONLY)) {
    char buf[MAX_TESTDEF_FILELEN];
    file.getName(buf, MAX_TESTDEF_FILELEN);
    if (!file.isSubDir() && !file.isHidden()) {
      Serial.printf("* Found: %s", buf);
      if (buf[0] == '_') {
        Serial.printf(" [Ignored]\n");
      } else {   
        bool loaded = storage_load_testdef(&file, &testdefs[n]);
        if (loaded) {
          n++;
        }
        Serial.printf("%s\n", loaded ? "" : "[FAILED]");
      }
    }
    file.close();
  }
  Serial.printf("%d testdefs loaded!\n", n);
  return n;
}

File storage_init_result_file(char* filename) {
    char buf[TESTDEF_ID_LEN + 5];
    sprintf(buf, "%s.csv", filename);
    Serial.printf("Making results file...\n");
    File file = SD.open(buf, FILE_WRITE);
    for (uint8_t field=0; field < RECV_PACKETS_FIELD_COUNT; field++) {
      file.write(RECV_PACKETS_FIELDS[field]);
      if ((field + 1) < RECV_PACKETS_FIELD_COUNT) {
        file.write(",");
      }
    }
    Serial.printf("Initialised test results file!\n");
    return file;
}

bool storage_write_result(File *file, uint8_t id, int16_t rssi, int16_t snr, uint32_t failed_recv, int32_t time_left) {
  char wr_buf[30];
  sprintf(wr_buf, "\n%d,%d,%d,%ld,%ld", id, rssi, snr, failed_recv, time_left);
  return file->write(wr_buf) ? true : false;
}

File storage_init_test_log(void) {
    Serial.printf("Making test log file...\n");
    File file = SD.open(LOG_FILE, FILE_WRITE);
      file.write("Test log created!\n");
    Serial.printf("Initialised test log file!\n");
    return file;
}

bool is_storage_initialised(void) {
  return _initialised;
}

static void extract_testdef_name(File* file, char *testdef_id) {
  char rd_buf[MAX_TESTDEF_FILELEN];
  file->getName(rd_buf, MAX_TESTDEF_FILELEN);
  // Get substring of filename to not include extension and not exceed max id length
  for (uint8_t i=0; i < MAX_TESTDEF_FILELEN; i++) {
    if (rd_buf[i] == '.' || rd_buf[i] == '\0' || i == TESTDEF_ID_LEN) {
      testdef_id[i] = '\0';
      break;
    }
    testdef_id[i] = rd_buf[i];
  }
}

static void get_fat_date_time(uint16_t* date, uint16_t* time) {
 // Convert and return date as FAT_DATE
 *date = FAT_DATE(year(), month(), day());
 // Convert and return time as FAT_TIME
 *time = FAT_TIME(hour(), minute(), second());
}
