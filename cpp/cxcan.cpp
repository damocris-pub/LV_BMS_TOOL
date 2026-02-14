//only Support ZLG USBCAN
//Author : richard xu (junzexu@outlook.com)
//Date : Dec 02, 2026

#define _CRT_SECURE_NO_WARNINGS

#include <string.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <Windows.h>
#include "cxcan.h"
#include "dfu_common.h"
#include "dfu_can.h"
#include "printf.h"

//global variable
static uint32_t gDevice = 0;
static uint32_t gChannel = 0;
static can_openDevice VCI_OpenDevice = NULL;
static can_closeDevice VCI_CloseDevice = NULL;
static can_initCAN VCI_InitCAN = NULL;
static can_readBoardInfo VCI_ReadBoardInfo = NULL;
static can_startCAN VCI_StartCAN = NULL;
static can_resetCAN VCI_ResetCAN = NULL;
static can_clearBuffer VCI_ClearBuffer = NULL;
static can_getReceiveNum VCI_GetReceiveNum = NULL;
static can_transmit VCI_Transmit = NULL;
static can_receive VCI_Receive = NULL;
static can_setReference VCI_SetReference = NULL;
static can_usbDeviceReset VCI_UsbDeviceReset = NULL;
static SPSCQueue que;

const static int speed_option[] = {
    20000, 33333, 40000, 50000, 66666, 80000, 83333, 100000, 
    125000, 200000, 250000, 400000, 500000, 666666, 800000, 1000000, 
};

const static uint16_t timing_code[] = {
    0x1C18, 0x6F09, 0xFF87, 0x1C09, 0x6F04, 0xFF83, 0x6F03, 0x1C04, 
    0x1C03, 0xFA81, 0x1C01, 0xFA80, 0x1C00, 0xB680, 0x1600, 0x1400, 
};


static bool SPSCQueuePush(const VCI_CAN_OBJ &item)
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

static bool SPSCQueuePop(VCI_CAN_OBJ &item)
{
    size_t current_tail = que.tail.load(std::memory_order_relaxed);
    if (current_tail == que.head.load(std::memory_order_relaxed)) {
        return false;
    }
    item = que.buffer[current_tail];
    que.tail.store((current_tail + 1) % kCapacity, std::memory_order_release);
    return true;
}

static VCI_CAN_OBJ can_constructFrame(uint8_t len, uint8_t *data)
{
    VCI_CAN_OBJ can_data;
	memset(&can_data, 0, sizeof(can_data));
	can_data.ID = CAN_CMD_ID & 0x7FF;   //standard frame, data frame
    can_data.TimeFlag = 0;  //1 - enable timestamp, 0 - disable timestamp
    can_data.TimeStamp = 0;
    can_data.ExternFlag = 0;
    can_data.RemoteFlag = 0;
    can_data.SendType = TRANSMIT_NORMAL;
    can_data.DataLen = 8;   //always 8 bytes
	for (int i=0; i<len; ++i) {
        can_data.data[i] = data[i];
	}
    for (int i=len; i<8; ++i) {
        can_data.data[i] = 0x00;    //padding with 0x00
    }
    return can_data;
}

static bool can_waitResponse(VCI_CAN_OBJ &item, uint32_t expected_id, int timeout_ms) 
{
    auto start = std::chrono::high_resolution_clock::now();
    auto deadline = start + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::high_resolution_clock::now() < deadline) {
        if (SPSCQueuePop(item)) {
            if (item.ID == expected_id) {
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
        int num = VCI_GetReceiveNum(gDevice, 0, gChannel);   //0 - CAN, 1 - CANFD
        if (num != 0) {
            VCI_CAN_OBJ response_data;
            if (VCI_Receive(gDevice, 0, gChannel, &response_data, 1, 0) != 1) {
                printf_("CAN : receive packet timeout\n");
            } else {
#if 0
                printf_("new packet with can id: %d, data ", response_data.ID);
                for (int i=0; i<response_data.DataLen; ++i) {
                    printf_("%02X ", response_data.data[i]);
                }
                printf_("\n");
#endif
                while (!SPSCQueuePush(response_data)) {
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
    if (can_chan != 0 && can_chan != 1) {
        printf_("CX USBCAN channel should be 0 or 1\n");
        return false;
    }
    int idx = std::lower_bound(speed_option, speed_option + sizeof(speed_option)/sizeof(int), can_speed) - speed_option;
    if (speed_option[idx] != can_speed) {
        printf_("CX USBCAN speed doesn't support %d\n", can_speed);
        return false;
    }

    //load library
    HINSTANCE handle = LoadLibraryA("cxcan.dll");
    if (handle == NULL) {
        printf_("could not load cxcan.dll\n");
        return false;
    }

    VCI_OpenDevice = (can_openDevice)GetProcAddress(handle, "VCI_OpenDevice");
    VCI_CloseDevice = (can_closeDevice)GetProcAddress(handle, "VCI_CloseDevice");
    VCI_ReadBoardInfo = (can_readBoardInfo)GetProcAddress(handle, "VCI_ReadBoardInfo");
    VCI_InitCAN = (can_initCAN)GetProcAddress(handle, "VCI_InitCAN");
    VCI_StartCAN = (can_startCAN)GetProcAddress(handle, "VCI_StartCAN");
    VCI_ResetCAN = (can_resetCAN)GetProcAddress(handle, "VCI_ResetCAN");
    VCI_ClearBuffer = (can_clearBuffer)GetProcAddress(handle, "VCI_ClearBuffer");
    VCI_GetReceiveNum = (can_getReceiveNum)GetProcAddress(handle, "VCI_GetReceiveNum");
    VCI_Transmit = (can_transmit)GetProcAddress(handle, "VCI_Transmit");
    VCI_Receive = (can_receive)GetProcAddress(handle, "VCI_Receive");
    VCI_SetReference = (can_setReference)GetProcAddress(handle, "VCI_SetReference");
    VCI_UsbDeviceReset = (can_usbDeviceReset)GetProcAddress(handle, "VCI_UsbDeviceReset");

	if (VCI_OpenDevice(VCI_USBCAN2, 0, 0) != STATUS_OK) {
		printf_("could not open CX USBCAN\n");
		return false;
	}
    VCI_INIT_CONFIG config;
	memset(&config, 0, sizeof(config));
    config.timing0 = timing_code[idx] & 0xFF;
    config.timing1 = (timing_code[idx] >> 8) & 0xFF; 
    config.acc_code = 0;
    config.acc_mask = 0xffffffff;
    config.mode = 0; //0 - normal mode, 1 - listen only mode, 2 - loopback mode
	if (VCI_InitCAN(VCI_USBCAN2, 0, can_chan, &config) != STATUS_OK) {
        printf_("could not init CAN channel %d\n", can_chan);
		return false;
	}
	if (VCI_StartCAN(VCI_USBCAN2, 0, can_chan) != STATUS_OK) {
        printf_("could not start CAN channel %d\n", can_chan);
		return false;
	}
    gDevice = VCI_USBCAN2;
    gChannel = can_chan;
    que.head = 0;
    que.tail = 0;
    return true;
}

bool can_disconnect(void)
{
	if (VCI_ResetCAN(gDevice, 0, gChannel) != STATUS_OK) {
        printf_("could not reset CAN channel\n");
		return false;
    }
	if (VCI_CloseDevice(gDevice, 0) != STATUS_OK) {
        printf_("could not close CAN device\n");
		return false;
    }
    return true;
}

bool can_getDeviceInfo(char *sn)
{
    VCI_BOARD_INFO info;
    if (VCI_ReadBoardInfo(gDevice, 0, &info) != STATUS_OK) {
        return false;
    }
    printf_("USBCAN HW version is %04X, FW version is %04X, Driver Version is %04X, API version is %04X\n", 
        info.hw_Version, info.fw_Version, info.dr_Version, info.in_Version);
    strcpy(sn, info.str_Serial_Num);
    return true;
}

int can_prepareCmd(uint8_t addr, uint8_t *resp)
{
    VCI_CAN_OBJ frame;
    prepareCmd[CMD_ADR_OFFSET] = addr;
    VCI_CAN_OBJ can_cmd = can_constructFrame(4 + 1, prepareCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
    VCI_CAN_OBJ frame;
    getBootloaderVerCmd[CMD_ADR_OFFSET] = addr;
    VCI_CAN_OBJ can_cmd = can_constructFrame(4 + 1, getBootloaderVerCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
    uint8_t* p = frame.data;
    memcpy(resp, &p[RSP_DAT_OFFSET - 1], 5);
    return 5;
}

int can_getBatterySN(uint8_t addr, uint8_t *resp)
{
    VCI_CAN_OBJ frame;
    getBatterySNCmd[CMD_ADR_OFFSET] = addr;
    VCI_CAN_OBJ can_cmd = can_constructFrame(4 + 1, getBatterySNCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
        printf_("send getBatterySN command failed\n");
		return -1;
    }
    int seq = 1;
    int idx = 0;
    while(true) {
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
    VCI_CAN_OBJ frame;
    getHardwareInfoCmd[CMD_ADR_OFFSET] = addr;
    VCI_CAN_OBJ can_cmd = can_constructFrame(4 + 1, getHardwareInfoCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
    VCI_CAN_OBJ frame;
    getHardwareTypeCmd[CMD_ADR_OFFSET] = addr;
    VCI_CAN_OBJ can_cmd = can_constructFrame(4 + 1, getHardwareTypeCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
    return -1;
}

int can_getApplicationVerCmd(uint8_t addr, uint8_t *resp)
{
    VCI_CAN_OBJ frame;
    getApplicationVerCmd[CMD_ADR_OFFSET] = addr;
    VCI_CAN_OBJ can_cmd = can_constructFrame(4 + 1, getApplicationVerCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
    VCI_CAN_OBJ frame;
    getPacketLenCmd[CMD_ADR_OFFSET] = addr;
    VCI_CAN_OBJ can_cmd = can_constructFrame(4 + 1, getPacketLenCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
    VCI_CAN_OBJ frame;
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
    VCI_CAN_OBJ can_cmd = can_constructFrame(6 + 1, setPacketLenCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
    VCI_CAN_OBJ frame;
    setApplicationLenCmd[CMD_ADR_OFFSET] = addr;
    setApplicationLenCmd[CMD_DAT_OFFSET] = applicationLen & 0xFF;
    setApplicationLenCmd[CMD_DAT_OFFSET + 1] = (applicationLen >> 8) & 0xFF;
    setApplicationLenCmd[CMD_DAT_OFFSET + 2] = (applicationLen >> 16) & 0xFF;
    setApplicationLenCmd[CMD_DAT_OFFSET + 3] = (applicationLen >> 24) & 0xFF;
    VCI_CAN_OBJ can_cmd = can_constructFrame(6 + 1, setApplicationLenCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
    VCI_CAN_OBJ frame;
    setPacketSeqCmd[CMD_ADR_OFFSET] = addr;
    setPacketSeqCmd[CMD_DAT_OFFSET] = packetSeq & 0xFF;
    setPacketSeqCmd[CMD_DAT_OFFSET + 1] = (packetSeq >> 8) & 0xFF;
    VCI_CAN_OBJ can_cmd = can_constructFrame(4 + 1, setPacketSeqCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
    VCI_CAN_OBJ frame;
    setPacketAddrCmd[CMD_ADR_OFFSET] = addr;
    setPacketAddrCmd[CMD_DAT_OFFSET] = packetAddr & 0xFF;
    setPacketAddrCmd[CMD_DAT_OFFSET + 1] = (packetAddr >> 8) & 0xFF;
    setPacketAddrCmd[CMD_DAT_OFFSET + 2] = (packetAddr >> 16) & 0xFF;
    setPacketAddrCmd[CMD_DAT_OFFSET + 3] = (packetAddr >> 24) & 0xFF;
    VCI_CAN_OBJ can_cmd = can_constructFrame(6 + 1, setPacketAddrCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
    VCI_CAN_OBJ can_data;
	memset(&can_data, 0, sizeof(can_data));
	can_data.ID = CAN_DAT_ID & 0x7FF;   //standard frame, data frame
    can_data.TimeFlag = 0;  //1 - enable timestamp, 0 - disable timestamp
    can_data.TimeStamp = 0;
    can_data.ExternFlag = 0;
    can_data.RemoteFlag = 0;
    can_data.SendType = TRANSMIT_NORMAL;
    can_data.DataLen = 8;   //always 8 bytes
    while (offset < packetLen) {
        for (int i = 0; i < 8; ++i) {
            can_data.data[i] = data[i + offset];
        }
        if (VCI_Transmit(gDevice, 0, gChannel, &can_data, 1) != 1) {
            printf_("send packet data command failed\n");
            return -1;
        }
        offset += 8;
#if 1
        std::this_thread::sleep_for(std::chrono::milliseconds(5));   //5 ms for FW to process the data
#else
        VCI_CAN_OBJ frame;
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
    VCI_CAN_OBJ frame;
    verifyPacketDataCmd[CMD_ADR_OFFSET] = addr;
    verifyPacketDataCmd[CMD_DAT_OFFSET] = packetCrc & 0xFF;
    verifyPacketDataCmd[CMD_DAT_OFFSET + 1] = (packetCrc >> 8) & 0xFF;
    VCI_CAN_OBJ can_cmd = can_constructFrame(4 + 1, verifyPacketDataCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
    VCI_CAN_OBJ frame;
    if (crcType != 0 && crcType != 1) {
        printf_("crc type should be 0 - crc16 or 1 - crc32\n");
		return -1;
    }
    VCI_CAN_OBJ can_cmd;
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
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
    VCI_CAN_OBJ can_cmd = can_constructFrame(4 + 1, updateStationCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
        printf_("send verifyAllData command failed\n");
		return -1;
    }
    return 0;
}

int can_updateCurrentStationCmd(uint8_t addr)
{
    updateStationCmd[CMD_ADR_OFFSET] = addr;
    updateStationCmd[CMD_DAT_OFFSET] = 0x51;
    VCI_CAN_OBJ can_cmd = can_constructFrame(4 + 1, updateStationCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
        printf_("send verifyAllData command failed\n");
		return -1;
    }
    return 0;
}

int can_getUpdateStatusCmd(uint8_t addr, uint8_t *resp)
{
    VCI_CAN_OBJ frame;
    getUpdateStatusCmd[CMD_ADR_OFFSET] = addr;
    VCI_CAN_OBJ can_cmd = can_constructFrame(2 + 1, getUpdateStatusCmd + CMD_LEN_OFFSET);
    if (VCI_Transmit(gDevice, 0, gChannel, &can_cmd, 1) != 1) {
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
