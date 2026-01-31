#pragma once

//only Support CAN and RS-485
//Author : richard xu (junzexu@outlook.com)
//Date : Dec 02, 2026

#include <stdint.h>

//defines, should be aligned with dfu fw
#define SIGNATURE_MAX_SIZE          512
#define DEFAULT_PKT_LEN             128
#define MAXIMUM_PKT_LEN             512

#define APP_LENGTH_OK               0xA1
#define APP_LENGTH_NG               0x01

#define SET_PKTLEN_OK               0xA1
#define SET_PKTLEN_NG               0x01

#define SET_PKTNUM_OK               0xA2
#define SET_PKTNUM_NG               0x01

#define XFER_DATA_OK                0xA2
#define XFER_DATA_NG                0x01

#define VERIFY_DATA_OK              0xA3
#define VERIFY_CRC_NG               0x02
#define VERIFY_WRITE_NG             0x03
#define VERIFY_SIZE_NG              0x04
#define VERIFY_SIGNATURE_NG         0x05

#define VERIFY_ALL_OK               0xA4
#define VERIFY_ALL_NG               0x08
#define SETTINGS_SAVE_NG            0x06

#define CMD_SOP_OFFSET              0
#define CMD_LEN_OFFSET              1
#define CMD_ADR_OFFSET              2
#define CMD_CMD_OFFSET              3
#define CMD_DAT_OFFSET              4
#define CMD_CRC_OFFSET(len)         (len+2)
#define CMD_EOP_OFFSET(len)         (len+4)

#define RSP_SOP_OFFSET              0
#define RSP_LEN_OFFSET              1
#define RSP_ADR_OFFSET              2
#define RSP_STA_OFFSET              3
#define RSP_DAT_OFFSET              4
#define RSP_CRC_OFFSET(len)         (len+2)
#define RSP_EOP_OFFSET(len)         (len+4)

#define DFU_CMD_SOP                 0x5B    //only used for RS-485
#define DFU_DAT_SOP                 0x5C    //only used for RS-485
#define DFU_CMD_EOP                 0x18    //only used for RS-485

//all the commands for upgrading
#define DFU_ECHO                    0x00
#define DFU_PREPARE                 0x10
#define DFU_GET_BOOTVER             0x20
#define DFU_GET_HWINFO              0x21
#define DFU_GET_HWTYPE              0x22
#define DFU_GET_APPVER              0x23
#define DFU_GET_PKTLEN              0x28
#define DFU_SET_PKTLEN              0x29
#define DFU_GET_PKTLEN_MAX          0x2A
#define DFU_SET_APPLEN              0x30
#define DFU_SET_PKTNUM              0x40   //set packet length or set packet start address   
#define DFU_VERIFY_PKTDAT           0x45
#define DFU_VERIFY_ALLDAT           0x50
#define DFU_UPDATE                  0x60 
#define DFU_GET_STATUS              0x61

//all the commands for application
#define DFU_GET_BATINFO             0x01
#define DFU_GET_CURINFO             0x12
#define DFU_GET_TEMPINFO            0x16
#define DFU_ENABLE_NVM              0x24
#define DFU_READ_NVM                0x25
#define DFU_ERASE_NVM               0x26
#define DFU_WRITE_NVM               0x27
#define DFU_GET_VOLTINFO            0x2E
#define DFU_GET_SOCINFO             0x47
#define DFU_GET_WIFIINFO            0x4E
#define DFU_GET_SERIAL              0x42
#define DFU_GET_PCSINFO             0x44
#define DFU_GET_FWVER               0x33
#define DFU_REQ_UPGRADE             0x80
#define DFU_SET_TOTALPKT            0x88
#define DFU_XFER_PKTDAT             0x89
#define DFU_XFER_ALLDAT             0x8A
#define DFU_REQ_RESTART             0x8B

//variables
extern uint8_t prepareCmd[];
extern uint8_t getBootloaderVerCmd[];
extern uint8_t getBatterySNCmd[];
extern uint8_t getHardwareInfoCmd[];
extern uint8_t getHardwareTypeCmd[];
extern uint8_t getApplicationVerCmd[];
extern uint8_t getPacketLenCmd[];
extern uint8_t setPacketLenCmd[];
extern uint8_t setApplicationLenCmd[];
extern uint8_t setPacketSeqCmd[];
extern uint8_t setPacketAddrCmd[];
extern uint8_t verifyPacketDataCmd[];
extern uint8_t verifyAllDataCrc16Cmd[];
extern uint8_t verifyAllDataCrc32Cmd[];
extern uint8_t updateStationCmd[];
extern uint8_t getUpdateStatusCmd[];

//functions
bool verifyPrepare(uint8_t *dat, bool useSop);
bool verifyGetBootloaderVer(uint8_t *dat, bool useSop);
bool verifyGetHardwareInfo(uint8_t *dat, bool useSop);
bool verifyGetHardwareType(uint8_t *dat, bool useSop);
bool verifyGetApplicationVer(uint8_t *dat, bool useSop);
bool verifyGetPacketLen(uint8_t *dat, bool useSop);
bool verifySetPacketLen(uint8_t *dat, bool useSop);
bool verifySetApplicationLen(uint8_t *dat, bool useSop);
bool verifySetPacketSeq(uint8_t *dat, bool useSop);
bool verifySetPacketAddr(uint8_t *dat, bool useSop);
bool verifySendPacketData(uint8_t *dat, bool useSop);
bool verifyPacketData(uint8_t *dat, bool useSop);
bool verifyAllData(uint8_t *dat, bool useSop);
bool verifyGetUpdateStatus(uint8_t *dat, bool useSop);

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllexport) uint16_t crc16(uint8_t *buffer, uint32_t len, uint16_t start);
__declspec(dllexport) uint32_t crc32(uint8_t *buffer, uint32_t len, uint32_t start);

#ifdef __cplusplus
}
#endif