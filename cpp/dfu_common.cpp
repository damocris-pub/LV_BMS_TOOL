//only Support CAN2.0B and RS-485
//Author : richard xu (junzexu@outlook.com)
//Date : Dec 02, 2026

#include "dfu_common.h"
#include "printf.h"

//global variable
uint8_t prepareCmd[] = {
    DFU_CMD_SOP,
    0x04,
    0x00,
    DFU_PREPARE,
    0x8C,
    0xBE,
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t getBootloaderVerCmd[] = {
    DFU_CMD_SOP,
    0x04,
    0x00,
    DFU_GET_BOOTVER,
    0x8C,
    0xBE,
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t getBatterySNCmd[] = {
    DFU_CMD_SOP,
    0x04,
    0x00,
    DFU_GET_HWINFO,
    0x8D,
    0xBE,
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t getHardwareInfoCmd[] = {
    DFU_CMD_SOP,
    0x04,
    0x00,
    DFU_GET_HWINFO,
    0x8D,
    0xBA,
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t getHardwareTypeCmd[] = {
    DFU_CMD_SOP,
    0x04,
    0x00,
    DFU_GET_HWTYPE,
    0x7D,
    0xBE,
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t getApplicationVerCmd[] = {
    DFU_CMD_SOP,
    0x04,
    0x00,
    DFU_GET_APPVER,
    0x5E,
    0xBE,
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t getPacketLenCmd[] = {
    DFU_CMD_SOP,
    0x04,
    0x00,
    DFU_GET_PKTLEN,
    0x5E,
    0xBE,
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t setPacketLenCmd[] = {
    DFU_CMD_SOP,
    0x06,
    0x00,
    DFU_SET_PKTLEN,
    0x00,           //length lsb
    0x00,
    0x00,
    0x00,           //length msb
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t setApplicationLenCmd[] = {
    DFU_CMD_SOP,
    0x06,
    0x00,
    DFU_SET_APPLEN,
    0x00,           //length lsb
    0x00,
    0x00,
    0x00,           //length msb
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t setPacketSeqCmd[] = {
    DFU_CMD_SOP,
    0x04,
    0x00,
    DFU_SET_PKTNUM,
    0x00,           //seq - lsb
    0x00,           //seq - msb
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t setPacketAddrCmd[] = {
    DFU_CMD_SOP,
    0x06,
    0x00,
    DFU_SET_PKTNUM,
    0x00,           //addr - lsb
    0x00,
    0x00,
    0x00,           //addr - msb
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t verifyPacketDataCmd[] = {
    DFU_CMD_SOP,
    0x04,
    0x00,
    DFU_VERIFY_PKTDAT,
    0x00,           //packet crc - lsb
    0x00,           //packet crc - msb
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t verifyAllDataCrc16Cmd[] = {
    DFU_CMD_SOP,
    0x05,
    0x00,
    DFU_VERIFY_ALLDAT,
    0x00,           //crc16 
    0x00,           //file crc - lsb
    0x00,           //file crc - msb
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t verifyAllDataCrc32Cmd[] = {
    DFU_CMD_SOP,
    0x07,
    0x00,
    DFU_VERIFY_ALLDAT,
    0x01,           //crc32
    0x00,           //file crc - lsb
    0x00,
    0x00,
    0x00,           //file crc - msb
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t updateStationCmd[] = {
    DFU_CMD_SOP,
    0x04,
    0x00,
    DFU_UPDATE,
    0x00,           //0x51 - current station, 0x52 - all stations
    0x52,
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint8_t getUpdateStatusCmd[] = {
    DFU_CMD_SOP,
    0x02,
    0x00,
    DFU_GET_STATUS,
    0x00,           //CRC-LSB, only for RS485
    0x00,           //CRC-MSB, only for RS485
    DFU_CMD_EOP,
};

uint16_t crc16(uint8_t *buffer, uint32_t len, uint16_t start)
{
    uint16_t crc = start;
    while (len--) {
        crc ^= *buffer++;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ 0xA001; //crc polynomial: x^16 + x^15 + x^2 + 1
            } else {
                crc = (crc >> 1);
            }
        }
    }
    return crc;
}

uint32_t crc32(uint8_t *buffer, uint32_t len, uint32_t start)
{
    uint32_t crc = ~start;
    for (uint32_t i=0; i<len; i++) {
        crc = crc ^ buffer[i];
        for (uint32_t j = 8; j > 0; j--) {
            crc = (crc >> 1) ^ (0xEDB88320U & ((crc & 1) ? 0xFFFFFFFF : 0));
        }
    }
    return ~crc;
}

static bool verifyCrcEop(uint8_t *dat, int len) 
{
    uint16_t crc = dat[RSP_CRC_OFFSET(len)] | (dat[RSP_CRC_OFFSET(len) + 1] << 8);
    uint16_t expected_crc = crc16(&dat[RSP_ADR_OFFSET], len, 0xffff);
    if (expected_crc != crc) {
        printf_("response error: crc received 0x%04X, expected 0x%04X\n", crc, expected_crc);
        return false;
    }
    if (dat[RSP_EOP_OFFSET(len)] != DFU_CMD_SOP) {
        printf_("response error: SOP received %d, expected 0x5B\n", dat[RSP_EOP_OFFSET(len)]);
        return false;
    }
    return true;
}

bool verifyPrepare(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("prepare response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x05) {
        printf_("prepare response error: length received %d, expected 0x05\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_PREPARE + 0x40) {
        printf_("prepare response error: command received %d, expected 0x50\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (dat[RSP_DAT_OFFSET - offset] != 0xCC || dat[RSP_DAT_OFFSET + 1 - offset] != 0xFE) {
        printf_("prepare response error: data received 0x%02X 0x%02X, expected 0xCC 0xFE\n", dat[RSP_DAT_OFFSET-offset],  dat[RSP_DAT_OFFSET + 1 - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifyGetBootloaderVer(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("getBootloaderVer response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x07) {
        printf_("getBootloaderVer response error: length received %d, expected 0x07\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_GET_BOOTVER + 0x40) {
        printf_("getBootloaderVer response error: command received %d, expected 0x50\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifyGetHardwareInfo(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("getHardwareInfo response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x07) {
        printf_("getHardwareInfo response error: length received %d, expected 0x07\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_GET_HWINFO + 0x40) {
        printf_("getHardwareInfo response error: command received %d, expected 0x61\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifyGetHardwareType(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("getHardwareType response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x07) {
        printf_("getHardwareType response error: length received %d, expected 0x07\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_GET_HWTYPE + 0x40) {
        printf_("getHardwareType response error: command received %d, expected 0x62\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifyGetApplicationVer(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("getApplicationVer response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x07) {
        printf_("getApplicationVer response error: length received %d, expected 0x07\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_GET_APPVER + 0x40) {
        printf_("getApplicationVer response error: command received %d, expected 0x63\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifyGetPacketLen(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("getPacketLen response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x06) {
        printf_("getPacketLen response error: length received %d, expected 0x06\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_GET_PKTLEN + 0x40) {
        printf_("getPacketLen response error: command received %d, expected 0x68\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifySetPacketLen(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("setPacketLen response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x03) {
        printf_("setPacketLen response error: length received %d, expected 0x03\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_SET_PKTLEN + 0x40) {
        printf_("setPacketLen response error: command received %d, expected 0x69\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (dat[RSP_DAT_OFFSET - offset] != SET_PKTLEN_OK) {
        printf_("setPacketSeq response error: ack received %d, expected 0xA1\n", dat[RSP_DAT_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifySetApplicationLen(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("setApplicationLen response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x07) {
        printf_("setApplicationLen response error: length received %d, expected 0x07\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_SET_APPLEN + 0x40) {
        printf_("setApplicationLen response error: command received %d, expected 0x70\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (dat[RSP_DAT_OFFSET - offset] != APP_LENGTH_OK) {
        printf_("setApplicationLen response error: ack received %d, expected 0xA1\n", dat[RSP_DAT_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifySetPacketSeq(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("setPacketSeq response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x05) {
        printf_("setPacketSeq response error: length received %d, expected 0x05\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_SET_PKTNUM + 0x40) {
        printf_("setPacketSeq response error: command received %d, expected 0x80\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (dat[RSP_DAT_OFFSET - offset] != SET_PKTNUM_OK) {
        printf_("setPacketSeq response error: ack received %d, expected 0xA2\n", dat[RSP_DAT_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifySetPacketAddr(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("setPacketAddr response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x07) {
        printf_("setPacketAddr response error: length received %d, expected 0x07\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_SET_PKTNUM + 0x40) {
        printf_("setPacketAddr response error: command received %d, expected 0x80\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (dat[RSP_DAT_OFFSET - offset] != SET_PKTNUM_OK) {
        printf_("setPacketAddr response error: ack received %d, expected 0xA2\n", dat[RSP_DAT_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifySendPacketData(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_DAT_SOP)) {
        printf_("setPacketAddr response error: SOP received %d, expected 0x5C\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x03) {
        printf_("setPacketAddr response error: length received %d, expected 0x03\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != 0x8C) {
        printf_("setPacketAddr response error: command received %d, expected 0x8C\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (dat[RSP_DAT_OFFSET - offset] != XFER_DATA_OK) {
        printf_("setPacketAddr response error: ack received %d, expected 0xA2\n", dat[RSP_DAT_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifyPacketData(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("verifyPacketData response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x03) {
        printf_("verifyPacketData response error: length received %d, expected 0x03\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_VERIFY_PKTDAT + 0x40) {
        printf_("verifyPacketData response error: command received %d, expected 0x85\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (dat[RSP_DAT_OFFSET - offset] != VERIFY_DATA_OK) {
        printf_("verifyPacketData response error: ack received %d, expected 0xA3\n", dat[RSP_DAT_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifyAllData(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("verifyAllData response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x03) {
        printf_("verifyAllData response error: length received %d, expected 0x03\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_VERIFY_ALLDAT + 0x40) {
        printf_("verifyAllData response error: command received %d, expected 0x85\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (dat[RSP_DAT_OFFSET - offset] != VERIFY_ALL_OK) {
        printf_("verifyAllData response error: ack received %d, expected 0xA4\n", dat[RSP_DAT_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}

bool verifyGetUpdateStatus(uint8_t *dat, bool useSop)
{
    uint8_t offset;
    if (useSop) {  //RS485
        offset = 0;
    } else {
        offset = 1;
    }
    if (useSop && (dat[RSP_SOP_OFFSET] != DFU_CMD_SOP)) {
        printf_("getUpdateStatus response error: SOP received %d, expected 0x5B\n", dat[CMD_SOP_OFFSET]);
        return false;
    }
    int len = dat[RSP_LEN_OFFSET - offset];
    if (len != 0x05) {
        printf_("getUpdateStatus response error: length received %d, expected 0x05\n", len);
        return false;
    }
    if (dat[RSP_STA_OFFSET - offset] != DFU_GET_STATUS + 0x40) {
        printf_("getUpdateStatus response error: command received %d, expected 0x85\n", dat[RSP_STA_OFFSET - offset]);
        return false;
    }
    if (useSop && !verifyCrcEop(dat, len)) {
        return false;
    }
    return true;
}
