//only Support ZLG USBCAN
//Author : richard xu (junzexu@outlook.com)
//Date : Dec 02, 2026

#define _CRT_SECURE_NO_WARNINGS

#include <string.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <Windows.h>
#include "zlgcan.h"
#include "dfu_common.h"
#include "dfu_can.h"
#include "printf.h"

//global variable
static DEVICE_HANDLE dev = NULL;
static CHANNEL_HANDLE chn = NULL;
static can_openDevice ZCAN_OpenDevice = NULL;
static can_closeDevice ZCAN_CloseDevice = NULL;
static can_getDeviceInf ZCAN_GetDeviceInf = NULL;
static can_isDeviceOnLine ZCAN_IsDeviceOnLine = NULL;
static can_initCAN ZCAN_InitCAN = NULL;
static can_startCAN ZCAN_StartCAN = NULL;
static can_resetCAN ZCAN_ResetCAN = NULL;
static can_clearBuffer ZCAN_ClearBuffer = NULL;
static can_getReceiveNum ZCAN_GetReceiveNum = NULL;
static can_transmit ZCAN_Transmit = NULL;
static can_receive ZCAN_Receive = NULL;
static can_setValue ZCAN_SetValue = NULL;
static can_getValue ZCAN_GetValue = NULL;
static can_getIProperty GetIProperty = NULL;
static can_geleaseIProperty ReleaseIProperty = NULL;
static SPSCQueue que;

const static int speed_option[] = {
    10000, 20000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000,  
};

static bool SPSCQueuePush(const can_frame &item)
{
    size_t current_head = que.head.load(std::memory_order_relaxed);
    size_t next_head = (current_head + 1) % kCapacity;
    if (next_head == que.tail.load(std::memory_order_acquire)) {
        return false;   //queue is full
    }
    que.buffer[current_head] = item;
    que.head.store(next_head, std::memory_order_release);
    return true;
}

static bool SPSCQueuePop(can_frame &item)
{
    size_t current_tail = que.tail.load(std::memory_order_relaxed);
    if (current_tail == que.head.load(std::memory_order_relaxed)) {
        return false;
    }
    item = que.buffer[current_tail];
    que.tail.store((current_tail + 1) % kCapacity, std::memory_order_release);
    return true;
}

static ZCAN_Transmit_Data can_constructFrame(uint8_t len, uint8_t *data)
{
    ZCAN_Transmit_Data can_data;
	memset(&can_data, 0, sizeof(can_data));
	can_data.frame.can_id = MAKE_CAN_ID(CAN_CMD_ID, 0, 0, 0);   //standard frame, data frame
	can_data.frame.can_dlc = 8; //always 8 bytes
	can_data.transmit_type = TRANSMIT_NORMAL;
	for (int i=0; i<len; ++i) {
		can_data.frame.data[i] = data[i];
	}
    for (int i=len; i<8; ++i) {
        can_data.frame.data[i] = 0x00;  //padding with 0x00
    }
    return can_data;
}

static bool can_waitResponse(can_frame &item, uint32_t expected_id, int timeout_ms) 
{
    auto start = std::chrono::high_resolution_clock::now();
    auto deadline = start + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::high_resolution_clock::now() < deadline) {
        if (SPSCQueuePop(item)) {
            if (item.can_id == expected_id) {
                return true;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return false;
}

void can_rx_thread(volatile int *running)
{
    while (*running) {
        int num = ZCAN_GetReceiveNum(chn, 0);   //0 - CAN, 1 - CANFD
        if (num != 0) {
            ZCAN_Receive_Data response_data;
            if (ZCAN_Receive(chn, &response_data, 1, -1) != 1) {
                printf_("CAN : receive packet timeout\n");
            } else {
#if 0
                printf_("new packet with can id: %d, data ", response_data.frame.can_id);
                for (int i=0; i<response_data.frame.can_id; ++i) {
                    printf_("%02X ", response_data.frame.data[i]);
                }
                printf_("\n");
#endif
                while (!SPSCQueuePush(response_data.frame)) {
                    std::this_thread::yield();
                    printf_("CAN RX FIFO full\n");
                }
            }
            break;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

bool can_connect(int can_chan, int can_speed)
{
    char path[16];
    char speed[16];

    if (can_chan != 0 && can_chan != 1) {
        printf_("ZLG USBCAN channel should be 0 or 1\n");
        return false;
    }
    if (!std::binary_search(speed_option, speed_option + sizeof(speed_option)/sizeof(int), can_speed)) {
        printf_("ZLG USBCAN speed doesn't support %d\n", can_speed);
    }
    //load library 
    HINSTANCE handle = LoadLibraryA("zlgcan.dll");
    if (handle == NULL) {
        printf_("could not load zlgcan.dll\n");
        return false;
    }
    ZCAN_OpenDevice = (can_openDevice)GetProcAddress(handle, "ZCAN_OpenDevice");
    ZCAN_CloseDevice = (can_closeDevice)GetProcAddress(handle, "ZCAN_CloseDevice");
    ZCAN_GetDeviceInf = (can_getDeviceInf)GetProcAddress(handle, "ZCAN_GetDeviceInf");
    ZCAN_IsDeviceOnLine = (can_isDeviceOnLine)GetProcAddress(handle, "ZCAN_IsDeviceOnLine");
    ZCAN_InitCAN = (can_initCAN)GetProcAddress(handle, "ZCAN_InitCAN");
    ZCAN_StartCAN = (can_startCAN)GetProcAddress(handle, "ZCAN_StartCAN");
    ZCAN_ResetCAN = (can_resetCAN)GetProcAddress(handle, "ZCAN_ResetCAN");
    ZCAN_ClearBuffer = (can_clearBuffer)GetProcAddress(handle, "ZCAN_ClearBuffer");
    ZCAN_GetReceiveNum = (can_getReceiveNum)GetProcAddress(handle, "ZCAN_GetReceiveNum");
    ZCAN_Transmit = (can_transmit)GetProcAddress(handle, "ZCAN_Transmit");
    ZCAN_Receive = (can_receive)GetProcAddress(handle, "ZCAN_Receive");
    ZCAN_SetValue = (can_setValue)GetProcAddress(handle, "ZCAN_SetValue");
    ZCAN_GetValue = (can_getValue)GetProcAddress(handle, "ZCAN_GetValue");
    GetIProperty = (can_getIProperty)GetProcAddress(handle, "GetIProperty");
    ReleaseIProperty = (can_geleaseIProperty)GetProcAddress(handle, "ReleaseIProperty");

	ZCAN_CHANNEL_INIT_CONFIG config;
	dev = ZCAN_OpenDevice(ZCAN_USBCAN2, 0, 0);
	if (dev == INVALID_DEVICE_HANDLE) {
		printf_("could not open ZLG USBCAN\n");
		return false;
	}
	sprintf_(path, "%d/baud_rate", can_chan);
    sprintf_(speed, "%d", can_speed);
	ZCAN_SetValue(dev, path, speed);
	memset(&config, 0, sizeof(config));
	config.can_type = 0;
	config.can.mode = 0;    //0 : normal mode, 1 : listen mode
	config.can.acc_code = 0;
	config.can.acc_mask = 0xFFFFFFFF;
	chn = ZCAN_InitCAN(dev, can_chan, &config);
	if (chn == INVALID_CHANNEL_HANDLE) {
        printf_("could not init CAN channel %d\n", can_chan);
		return false;
	}
	if (ZCAN_StartCAN(chn) != STATUS_OK) {
        printf_("could not start CAN channel %d\n", can_chan);
		return false;
	}
    que.head = 0;
    que.tail = 0;
    return true;
}

bool can_disconnect(void)
{
	if (ZCAN_ResetCAN(chn) != STATUS_OK) {
        printf_("could not reset CAN channel\n");
		return false;
    }
	if (ZCAN_CloseDevice(dev) != STATUS_OK) {
        printf_("could not close CAN device\n");
		return false;
    }
    return true;
}

bool can_getDeviceInfo(char *sn)
{
    ZCAN_DEVICE_INFO info;
    if (ZCAN_GetDeviceInf(dev, &info) != STATUS_OK) {
        return false;
    }
    printf_("USBCAN HW version is %04X, FW version is %04X, Driver Version is %04X, API version is %04X\n", 
        info.hw_Version, info.fw_Version, info.dr_Version, info.in_Version);
    strcpy(sn, info.str_Serial_Num);
    return true;
}

int can_prepareCmd(uint8_t addr, uint8_t *resp)
{
    can_frame frame;
    prepareCmd[CMD_ADR_OFFSET] = addr;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(4 + 1, prepareCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send prepare command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifyPrepare(frame.data, false)) {
        return -1;
    }
    resp[0] = frame.data[5];    //get the cell num
    return 1;
}

int can_getBootloaderVerCmd(uint8_t addr, uint8_t *resp)
{
    can_frame frame;
    getBootloaderVerCmd[CMD_ADR_OFFSET] = addr;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(4 + 1, getBootloaderVerCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send getBootloaderVer command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifyGetBootloaderVer(frame.data, false)) {
        return -1;
    }
    uint8_t *p = frame.data;
    memcpy(resp, &p[RSP_DAT_OFFSET - 1], 5);
    return 5;
}

int can_getBatterySN(uint8_t addr, uint8_t *resp)
{
    can_frame frame;
    getBatterySNCmd[CMD_ADR_OFFSET] = addr;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(4 + 1, getBatterySNCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send getBatterySN command failed\n");
		return -1;
    }
    int seq = 1;
    int idx = 0;
    while (true) {
        if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
            break;  //no remainging packet
        }
        uint8_t *p = frame.data;
        if (p[RSP_STA_OFFSET - 1] != DFU_GET_HWINFO + 0x40) {
            printf_("getBatterySN response error: command received %d, expected 0x61\n", p[RSP_STA_OFFSET - 1]);
            return -1;
        }
        if (p[RSP_DAT_OFFSET - 1] != seq) {
            printf_("getBatterySN response error: seq received %d, expected %d\n", p[RSP_DAT_OFFSET - 1], seq);
            return -1;
        }
        int len = p[RSP_LEN_OFFSET - 1] - 3;
        memcpy(resp + idx, &p[RSP_DAT_OFFSET], len);
        idx += len;
        ++seq;
    }
    return idx;
}

int can_getHardwareInfoCmd(uint8_t addr, uint8_t *resp)
{
    can_frame frame;
    getHardwareInfoCmd[CMD_ADR_OFFSET] = addr;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(4 + 1, getHardwareInfoCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send getHardwareInfo command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifyGetHardwareInfo(frame.data, false)) {
        return -1;
    }
    uint8_t *p = frame.data;
    memcpy(resp, &p[RSP_DAT_OFFSET-1], 5);
    return 5;
}

int can_getHardwareTypeCmd(uint8_t addr, uint8_t *resp)
{
    can_frame frame;
    getHardwareTypeCmd[CMD_ADR_OFFSET] = addr;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(4 + 1, getHardwareTypeCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send getHardwareType command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifyGetHardwareType(frame.data, false)) {
        return -1;
    }
    uint8_t *p = frame.data;
    memcpy(resp, &p[RSP_DAT_OFFSET-1], 5);
    return 5;
}

int can_getApplicationVerCmd(uint8_t addr, uint8_t *resp)
{
    can_frame frame;
    getApplicationVerCmd[CMD_ADR_OFFSET] = addr;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(4 + 1, getApplicationVerCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send getApplicationVer command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifyGetApplicationVer(frame.data, false)) {
        return -1;
    }
    uint8_t *p = frame.data;
    memcpy(resp, &p[RSP_DAT_OFFSET-1], 5);
    return 5;
}

int can_getPacketLenCmd(uint8_t addr, uint8_t *resp)
{
    can_frame frame;
    getPacketLenCmd[CMD_ADR_OFFSET] = addr;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(4 + 1, getPacketLenCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send getPacketLen command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifyGetPacketLen(frame.data, false)) {
        return -1;
    }
    uint8_t *p = frame.data;
    memcpy(resp, &p[RSP_DAT_OFFSET-1], 4);
    return 4;
}

int can_setPacketLenCmd(uint8_t addr, uint16_t packetLen)
{
    can_frame frame;
    if (packetLen != 8 && packetLen != 16 && packetLen != 32 && packetLen != 64 &&
        packetLen != 128 && packetLen != 256 && packetLen != 512) {
        printf_("packetLen should be 8, 16, 32, 64, 128, 256, 512\n");
		return -1;
    }
    setPacketLenCmd[CMD_ADR_OFFSET] = addr;
    setPacketLenCmd[CMD_DAT_OFFSET] = packetLen & 0xFF;
    setPacketLenCmd[CMD_DAT_OFFSET + 1] = (packetLen >> 8) & 0xFF;
    setPacketLenCmd[CMD_DAT_OFFSET + 2] = 0x00;
    setPacketLenCmd[CMD_DAT_OFFSET + 3] = 0x00;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(6 + 1, setPacketLenCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send setPacketLen command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifySetPacketLen(frame.data, false)) {
        return -1;
    }
    return 0;
}

int can_setApplicationLenCmd(uint8_t addr, uint32_t applicationLen, uint8_t *resp)
{
    can_frame frame;
    setApplicationLenCmd[CMD_ADR_OFFSET] = addr;
    setApplicationLenCmd[CMD_DAT_OFFSET] = applicationLen & 0xFF;
    setApplicationLenCmd[CMD_DAT_OFFSET + 1] = (applicationLen >> 8) & 0xFF;
    setApplicationLenCmd[CMD_DAT_OFFSET + 2] = (applicationLen >> 16) & 0xFF;
    setApplicationLenCmd[CMD_DAT_OFFSET + 3] = (applicationLen >> 24) & 0xFF;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(6 + 1, setApplicationLenCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send getApplicationLen command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifySetApplicationLen(frame.data, false)) {
        return -1;
    }
    uint8_t *p = frame.data;
    memcpy(resp, &p[RSP_DAT_OFFSET], 4);
    return 4;
}

int can_setPacketSeqCmd(uint8_t addr, uint16_t packetSeq, uint8_t *resp)
{
    can_frame frame;
    setPacketSeqCmd[CMD_ADR_OFFSET] = addr;
    setPacketSeqCmd[CMD_DAT_OFFSET] = packetSeq & 0xFF;
    setPacketSeqCmd[CMD_DAT_OFFSET + 1] = (packetSeq >> 8) & 0xFF;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(4 + 1, setPacketSeqCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send setPacketSeq command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifySetPacketSeq(frame.data, false)) {
        return -1;
    }
    uint8_t *p = frame.data;
    memcpy(resp, &p[RSP_DAT_OFFSET], 2);
    return 2;
}

int can_setPacketAddrCmd(uint8_t addr, uint32_t packetAddr, uint8_t *resp)
{
    can_frame frame;
    setPacketAddrCmd[CMD_ADR_OFFSET] = addr;
    setPacketAddrCmd[CMD_DAT_OFFSET] = packetAddr & 0xFF;
    setPacketAddrCmd[CMD_DAT_OFFSET + 1] = (packetAddr >> 8) & 0xFF;
    setPacketAddrCmd[CMD_DAT_OFFSET + 2] = (packetAddr >> 16) & 0xFF;
    setPacketAddrCmd[CMD_DAT_OFFSET + 3] = (packetAddr >> 24) & 0xFF;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(6 + 1, setPacketAddrCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send setPacketAddr command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifySetPacketAddr(frame.data, false)) {
        return -1;
    }
    uint8_t *p = frame.data;
    memcpy(resp, &p[RSP_DAT_OFFSET], 4);
    return 4;
}

int can_sendPacketData(uint16_t packetLen, uint8_t *data)
{
    if (packetLen != 8 && packetLen != 16 && packetLen != 32 && packetLen != 64 &&
        packetLen != 128 && packetLen != 256 && packetLen != 512) {
        printf_("packetLen should be 8, 16, 32, 64, 128, 256, 512\n");
		return -1;
    }
    int offset = 0;
    ZCAN_Transmit_Data can_data;
	can_data.frame.can_id = MAKE_CAN_ID(CAN_DAT_ID, 0, 0, 0);   //standard frame, data frame
	can_data.frame.can_dlc = 8; //always 8 bytes
	can_data.transmit_type = TRANSMIT_NORMAL;
    while (offset < packetLen) {
        for (int i = 0; i < 8; ++i) {
            can_data.frame.data[i] = data[i + offset];
        }
        if (ZCAN_Transmit(chn, &can_data, 1) != 1) {
            printf_("send packet data command failed\n");
            return -1;
        }
        offset += 8;
#if 1
        std::this_thread::sleep_for(std::chrono::milliseconds(5));   //5 ms for FW to process the data
#else
        can_frame frame;
        if (!can_waitResponse(frame, CAN_DAT_ID, 3)) {
            printf_("wait CAN response timeout\n");
            return -1;
        }
        if (!verifySendPacketData(frame.data, false)) {
            return -1;
        }
#endif
    }
    return 0;
}

int can_verifyPacketDataCmd(uint8_t addr, uint16_t packetCrc)
{
    can_frame frame;
    verifyPacketDataCmd[CMD_ADR_OFFSET] = addr;
    verifyPacketDataCmd[CMD_DAT_OFFSET] = packetCrc & 0xFF;
    verifyPacketDataCmd[CMD_DAT_OFFSET + 1] = (packetCrc >> 8) & 0xFF;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(4 + 1, verifyPacketDataCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send verifyPacketData command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifyPacketData(frame.data, false)) {
        return -1;
    }
    return 0;
}

int can_verifyAllDataCmd(uint8_t addr, uint8_t crcType, uint32_t fileCrc)
{
    can_frame frame;
    if (crcType != 0 && crcType != 1) {
        printf_("crc type should be 0 - crc16 or 1 - crc32\n");
		return -1;
    }
    ZCAN_Transmit_Data can_cmd;
    if (crcType == 0) {
        verifyAllDataCrc16Cmd[CMD_ADR_OFFSET] = addr;
        verifyAllDataCrc16Cmd[CMD_DAT_OFFSET + 1] = fileCrc & 0xFF;
        verifyAllDataCrc16Cmd[CMD_DAT_OFFSET + 2] = (fileCrc >> 8) & 0xFF;
        can_cmd = can_constructFrame(5 + 1, verifyAllDataCrc16Cmd + CMD_LEN_OFFSET);
    } else {
        verifyAllDataCrc32Cmd[CMD_ADR_OFFSET] = addr;
        verifyAllDataCrc32Cmd[CMD_DAT_OFFSET + 1] = fileCrc & 0xFF;
        verifyAllDataCrc32Cmd[CMD_DAT_OFFSET + 2] = (fileCrc >> 8) & 0xFF;
        verifyAllDataCrc32Cmd[CMD_DAT_OFFSET + 3] = (fileCrc >> 16) & 0xFF;
        verifyAllDataCrc32Cmd[CMD_DAT_OFFSET + 4] = (fileCrc >> 24) & 0xFF;
        can_cmd = can_constructFrame(7 + 1, verifyAllDataCrc32Cmd + CMD_LEN_OFFSET);
    }
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send verifyAllData command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifyAllData(frame.data, false)) {
        return -1;
    }
    return 0;
}

int can_updateAllStationCmd(void)
{
    updateStationCmd[CMD_ADR_OFFSET] = 0x00;
    updateStationCmd[CMD_DAT_OFFSET] = 0x52;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(4 + 1, updateStationCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send verifyAllData command failed\n");
		return -1;
    }
    return 0;
}

int can_updateCurrentStationCmd(uint8_t addr)
{
    updateStationCmd[CMD_ADR_OFFSET] = addr;
    updateStationCmd[CMD_DAT_OFFSET] = 0x51;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(4 + 1, updateStationCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send verifyAllData command failed\n");
		return -1;
    }
    return 0;
}

int can_getUpdateStatusCmd(uint8_t addr, uint8_t *resp)
{
    can_frame frame;
    getUpdateStatusCmd[CMD_ADR_OFFSET] = addr;
    ZCAN_Transmit_Data can_cmd = can_constructFrame(2 + 1, getUpdateStatusCmd + CMD_LEN_OFFSET);
    if (ZCAN_Transmit(chn, &can_cmd, 1) != 1) {
        printf_("send getUpdateStatus command failed\n");
		return -1;
    }
    if (!can_waitResponse(frame, CAN_RSP_ID, 3)) {
        printf_("wait CAN response timeout\n");
        return -1;
    }
    if (!verifyGetUpdateStatus(frame.data, false)) {
        return -1;
    }
    uint8_t *p = frame.data;
    memcpy(resp, &p[RSP_DAT_OFFSET], 3);
    return 3;
}
