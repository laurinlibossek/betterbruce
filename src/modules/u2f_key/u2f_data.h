#pragma once

#include <stdint.h>
#include <stdbool.h>

#define U2F_DATA_FOLDER   "/u2f"
#define U2F_CERT_FILE     U2F_DATA_FOLDER "/cert.der"
#define U2F_CERT_KEY_FILE U2F_DATA_FOLDER "/cert_key.bin"
#define U2F_KEY_FILE      U2F_DATA_FOLDER "/key.bin"
#define U2F_CNT_FILE      U2F_DATA_FOLDER "/cnt.bin"

bool u2f_data_check(bool cert_only);
bool u2f_data_cert_check(void);
uint32_t u2f_data_cert_load(uint8_t* cert);
bool u2f_data_cert_key_load(uint8_t* cert_key);
bool u2f_data_key_load(uint8_t* device_key);
bool u2f_data_key_generate(uint8_t* device_key);
bool u2f_data_cnt_read(uint32_t* cnt);
bool u2f_data_cnt_write(uint32_t cnt);
