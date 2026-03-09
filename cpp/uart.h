#pragma once

#include <stdint.h>

__declspec(dllexport) bool uart_connect(const char *port, uint32_t baud_rate);
__declspec(dllexport) bool uart_changeBaudRate(uint32_t baud_rate);
__declspec(dllexport) bool uart_disconnect(void);
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
__declspec(dllexport) int uart_updateAllStationCmd(void);
__declspec(dllexport) int uart_updateCurrentStationCmd(uint8_t addr);
__declspec(dllexport) int uart_getUpdateStatusCmd(uint8_t addr, uint8_t *resp);
