#pragma once

#include <stdint.h>

#define WIFI_SOP 0x5F
#define WIFI_EOP 0xF5

enum Wifi_Command {
    WIFI_UPGRADE_REQUEST  = 0xC0,
    WIFI_UPGRADE_LENGTH   = 0xC1,
    WIFI_UPGRADE_DATA     = 0xC2,
    WIFI_UPGRADE_COMPLETE = 0xC3,
    WIFI_UPGRADE_EXECUTE  = 0xC4,
    WIFI_GET_SN   = 0x42,
    WIFI_GET_VER  = 0x33,
    WIFI_SET_TIME = 0xCF,
    WIFI_GET_PCS  = 0x40,
    WIFI_GET_DOD  = 0x05,
    WIFI_GET_DSGC = 0x12,
    WIFI_GET_CHGT = 0x16,
    WIFI_GET_DSGT = 0x1C,
    WIFI_GET_DSGV = 0x2E,
    WIFI_GET_SOCM = 0x47,
};

__declspec(dllexport) bool uart_connect(const char *port, uint32_t baud_rate);
__declspec(dllexport) bool uart_disconnect(void);
__declspec(dllexport) bool uart_changeHostBaud(uint32_t baud_rate);
__declspec(dllexport) bool uart_requestSlaveBaud(bool to_high);
__declspec(dllexport) bool uart_requestUpgrade(void);
__declspec(dllexport) bool uart_sendFrameCount(uint16_t cnt);
__declspec(dllexport) int uart_sendFrameData(uint16_t seq, uint16_t len, uint8_t *dat);
__declspec(dllexport) bool uart_requestComplete(void);

__declspec(dllexport) int uart_prepareCmd(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int uart_getBootloaderVerCmd(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int uart_getBatterySN(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int uart_getHardwareInfoCmd(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int uart_getHardwareTypeCmd(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int uart_getApplicationVerCmd(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int uart_getPacketLenCmd(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int uart_setPacketLenCmd(uint8_t addr, uint16_t packetLen);
__declspec(dllexport) int uart_setApplicationLenCmd(uint8_t addr, uint32_t applicationLen, uint8_t *resp);
__declspec(dllexport) int uart_setPacketSeqCmd(uint8_t addr, uint16_t packetSeq, uint8_t *resp);
__declspec(dllexport) int uart_setPacketAddrCmd(uint8_t addr, uint32_t packetAddr, uint8_t *resp);
__declspec(dllexport) int uart_sendPacketData(uint16_t packetLen, uint8_t *data);
__declspec(dllexport) int uart_verifyPacketDataCmd(uint8_t addr, uint16_t packetCrc);
__declspec(dllexport) int uart_verifyAllDataCmd(uint8_t addr, uint8_t crcType, uint32_t fileCrc);
__declspec(dllexport) int uart_updateStationCmd(bool all);
__declspec(dllexport) int uart_getUpdateStatusCmd(uint8_t addr, uint8_t *resp);
