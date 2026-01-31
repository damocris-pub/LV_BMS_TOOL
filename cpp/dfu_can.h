#pragma once

//only Support ZLG USBCAN
//Author : richard xu (junzexu@outlook.com)
//Date : Dec 02, 2026

#include <stdint.h>

#define CAN_CMD_ID              0x300
#define CAN_DAT_ID              0x4C0
#define CAN_RSP_ID              0x370

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllexport) bool can_connect(int can_chan, int can_speed);
__declspec(dllexport) bool can_disconnect(void);
__declspec(dllexport) bool can_getDeviceInfo(char *sn);
__declspec(dllexport) int can_prepareCmd(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int can_getBootloaderVerCmd(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int can_getBatterySN(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int can_getHardwareInfoCmd(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int can_getHardwareTypeCmd(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int can_getApplicationVerCmd(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int can_getPacketLenCmd(uint8_t addr, uint8_t *resp);
__declspec(dllexport) int can_setPacketLenCmd(uint8_t addr, uint16_t packetLen);
__declspec(dllexport) int can_setApplicationLenCmd(uint8_t addr, uint32_t applicationLen, uint8_t *resp);
__declspec(dllexport) int can_setPacketSeqCmd(uint8_t addr, uint16_t packetSeq, uint8_t *resp);
__declspec(dllexport) int can_setPacketAddrCmd(uint8_t addr, uint32_t packetAddr, uint8_t *resp);
__declspec(dllexport) int can_sendPacketData(uint16_t packetLen, uint8_t *data);
__declspec(dllexport) int can_verifyPacketDataCmd(uint8_t addr, uint16_t packetCrc);
__declspec(dllexport) int can_verifyAllDataCmd(uint8_t addr, uint8_t crcType, uint32_t fileCrc);
__declspec(dllexport) int can_updateAllStationCmd(void);
__declspec(dllexport) int can_updateCurrentStationCmd(uint8_t addr);
__declspec(dllexport) int can_getUpdateStatusCmd(uint8_t addr, uint8_t *resp);

#ifdef __cplusplus
}
#endif
