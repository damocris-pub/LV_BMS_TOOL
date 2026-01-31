//only Support ZLG & CX USBCAN
//Author : richard xu (junzexu@outlook.com)
//Date : Dec 02, 2026

#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <Windows.h>

#define USED_CAN_CHN        0       //check which CAN channel is connected
#define USED_CAN_SPEED      500000  //500kbps

typedef void (*out_fct_type)(char character, void *buffer, size_t idx, size_t maxlen);
typedef void (*RegisterInternalPutchar)(out_fct_type custom_putchar);
typedef uint16_t (*Crc16)(uint8_t *buffer, uint32_t len, uint16_t start);
typedef uint32_t (*Crc32)(uint8_t *buffer, uint32_t len, uint32_t start);
typedef bool (*CanConnect)(int can_chan, int can_speed);
typedef bool (*CanDisconnect)(void);
typedef bool (*CanGetDeviceInfo)(char *sn);
typedef int (*CanPrepareCmd)(uint8_t addr, uint8_t *resp);
typedef int (*CanGetBootloaderVerCmd)(uint8_t addr, uint8_t *resp);
typedef int (*CanGetBatterySN)(uint8_t addr, uint8_t *resp);
typedef int (*CanGetHardwareInfoCmd)(uint8_t addr, uint8_t *resp);
typedef int (*CanGetHardwareTypeCmd)(uint8_t addr, uint8_t *resp);
typedef int (*CanGetApplicationVerCmd)(uint8_t addr, uint8_t *resp);
typedef int (*CanGetPacketLenCmd)(uint8_t addr, uint8_t *resp);
typedef int (*CanSetPacketLenCmd)(uint8_t addr, uint16_t packetLen);
typedef int (*CanSetApplicationLenCmd)(uint8_t addr, uint32_t applicationLen, uint8_t *resp);
typedef int (*CanSetPacketSeqCmd)(uint8_t addr, uint16_t packetSeq, uint8_t *resp);
typedef int (*CanSetPacketAddrCmd)(uint8_t addr, uint32_t packetAddr, uint8_t *resp);
typedef int (*CanSendPacketData)(uint16_t packetLen, uint8_t *data);
typedef int (*CanVerifyPacketDataCmd)(uint8_t addr, uint16_t packetCrc);
typedef int (*CanVerifyAllDataCmd)(uint8_t addr, uint8_t crcType, uint32_t fileCrc);
typedef int (*CanUpdateAllStationCmd)(void);
typedef int (*CanUpdateCurrentStationCmd)(uint8_t addr);
typedef int (*CanGetUpdateStatusCmd)(uint8_t addr, uint8_t *resp);

static RegisterInternalPutchar register_internal_putchar = NULL;
static Crc16 crc16 = NULL;
static Crc32 crc32 = NULL;
static CanConnect can_connect = NULL;
static CanDisconnect can_disconnect = NULL;
static CanGetDeviceInfo can_getDeviceInfo = NULL;
static CanPrepareCmd can_prepareCmd = NULL;
static CanGetBootloaderVerCmd can_getBootloaderVerCmd = NULL;
static CanGetBatterySN can_getBatterySN = NULL;
static CanGetHardwareInfoCmd can_getHardwareInfoCmd = NULL;
static CanGetHardwareTypeCmd can_getHardwareTypeCmd = NULL;
static CanGetApplicationVerCmd can_getApplicationVerCmd = NULL;
static CanGetPacketLenCmd can_getPacketLenCmd = NULL;
static CanSetPacketLenCmd can_setPacketLenCmd = NULL;
static CanSetApplicationLenCmd can_setApplicationLenCmd = NULL;
static CanSetPacketSeqCmd can_setPacketSeqCmd = NULL;
static CanSetPacketAddrCmd can_setPacketAddrCmd = NULL;
static CanSendPacketData can_sendPacketData = NULL;
static CanVerifyPacketDataCmd can_verifyPacketDataCmd = NULL;
static CanVerifyAllDataCmd can_verifyAllDataCmd = NULL;
static CanUpdateAllStationCmd can_updateAllStationCmd = NULL;
static CanUpdateCurrentStationCmd can_updateCurrentStationCmd = NULL;
static CanGetUpdateStatusCmd can_getUpdateStatusCmd = NULL;

inline void putchar_(char character, void* buffer, size_t idx, size_t maxlen)
{
    putchar(character);
}

inline void print_usage(void)
{
    printf("Usage: can_update_app.exe -a <addr> -p <packetLen> -m <updateMode> -c <crcType> -f <dfuFile>\n");
    printf("addr : battery addresss start from 0\n");
    printf("packetLen : packet length, should be 8, 16, 32, 64, 128, 256 or 512\n");
    printf("updateMode : 0 - only update current station, 1 - update all stations\n");
    printf("crcType : 0 - crc16 for app file, 1 - crc32 for app file\n");
}

int main(int argc, char **argv)
{
    char sn[20];
    uint16_t packetLen = 0x80;  //8, 16, 32, 64, 128, 256, 512
    uint8_t addr = 0x00;
    uint8_t mode = 0;
    uint8_t crcType = 0;    //0: crc16, 1:crc32
    uint16_t seq = 0x01;
    uint16_t crc = 0;
    uint32_t fileCrc = 0;
    uint8_t resp[8];
    char batterySN[33] = { '\0' };
    uint32_t newPacketLen;
    uint8_t status;
    int retCode = 0;

    //load library
    HINSTANCE handle = LoadLibraryA("cx_can_update.dll");
    if (handle == NULL) {
        handle = LoadLibraryA("zlg_can_update.dll");
        if (handle ==NULL) {
            printf("could not load cx_can_update.dll or zlg_can_update.dll\n");
            return false;
        }
    }
    register_internal_putchar = (RegisterInternalPutchar)GetProcAddress(handle, "register_internal_putchar");
    crc16 = (Crc16)GetProcAddress(handle, "crc16");
    crc32 = (Crc32)GetProcAddress(handle, "crc32");
    can_connect = (CanConnect)GetProcAddress(handle, "can_connect");
    can_disconnect = (CanDisconnect)GetProcAddress(handle, "can_disconnect");
    can_getDeviceInfo = (CanGetDeviceInfo)GetProcAddress(handle, "can_getDeviceInfo");
    can_prepareCmd = (CanPrepareCmd)GetProcAddress(handle, "can_prepareCmd");
    can_getBootloaderVerCmd = (CanGetBootloaderVerCmd)GetProcAddress(handle, "can_getBootloaderVerCmd");
    can_getBatterySN = (CanGetBatterySN)GetProcAddress(handle, "can_getBatterySN");
    can_getHardwareInfoCmd = (CanGetHardwareInfoCmd)GetProcAddress(handle, "can_getHardwareInfoCmd");
    can_getHardwareTypeCmd = (CanGetHardwareTypeCmd)GetProcAddress(handle, "can_getHardwareTypeCmd");
    can_getApplicationVerCmd = (CanGetApplicationVerCmd)GetProcAddress(handle, "can_getApplicationVerCmd");
    can_getPacketLenCmd = (CanGetPacketLenCmd)GetProcAddress(handle, "can_getPacketLenCmd");
    can_setPacketLenCmd = (CanSetPacketLenCmd)GetProcAddress(handle, "can_setPacketLenCmd");
    can_setApplicationLenCmd = (CanSetApplicationLenCmd)GetProcAddress(handle, "can_setApplicationLenCmd");
    can_setPacketSeqCmd = (CanSetPacketSeqCmd)GetProcAddress(handle, "can_setPacketSeqCmd");
    can_setPacketAddrCmd = (CanSetPacketAddrCmd)GetProcAddress(handle, "can_setPacketAddrCmd");
    can_sendPacketData = (CanSendPacketData)GetProcAddress(handle, "can_sendPacketData");
    can_verifyPacketDataCmd = (CanVerifyPacketDataCmd)GetProcAddress(handle, "can_verifyPacketDataCmd");
    can_verifyAllDataCmd = (CanVerifyAllDataCmd)GetProcAddress(handle, "can_verifyAllDataCmd");
    can_updateAllStationCmd = (CanUpdateAllStationCmd)GetProcAddress(handle, "can_updateAllStationCmd");
    can_updateCurrentStationCmd = (CanUpdateCurrentStationCmd)GetProcAddress(handle, "can_updateCurrentStationCmd");
    can_getUpdateStatusCmd = (CanGetUpdateStatusCmd)GetProcAddress(handle, "can_getUpdateStatusCmd");

    fflush(stdout);
    if (argc != 3 && argc !=5 && argc != 7 && argc != 9 && argc != 11) {
        print_usage();
        return -1;
    }
    int i=1;
    int filePos = 1;
    while (i<argc) {
        if (argv[i][0] == '-') {
            char ch = argv[i][1];
            switch (ch) {
            case 'a':
                ++i;
                addr = (uint8_t)strtol(argv[i], nullptr, 10);
                break;
            case 'p':
                ++i;
                packetLen = (uint16_t)strtol(argv[i], nullptr, 10);
                if (packetLen != 8 && packetLen != 16 && packetLen != 32 && packetLen != 64 &&
                    packetLen != 128 && packetLen != 256 && packetLen != 512) {
                    printf("packetLen should be 8, 16, 32, 64, 128, 256, 512\n");
		            return -1;
                }
                break;
            case 'm':
                ++i;
                mode = (uint16_t)strtol(argv[i], nullptr, 10);
                if (mode != 0 && mode != 1) {
                    printf("mode should be 0: current or 1: all\n");
                    return -1;
                }
                break;
            case 'c':
                ++i;
                crcType = (uint16_t)strtol(argv[i], nullptr, 10);
                if (crcType != 0 && crcType != 1) {
                    printf("crcType should be 0: crc16 or 1: crc32\n");
                    return -1;
                }
                break;
            case 'f':
                ++i;
                filePos = i;
                break;
            default:
                printf("illegal arguments, only supports a, p, c and f\n");
                print_usage();
                return -1;
            }
            ++i;
        }
    }
    printf("target address is %d, packet length is %d, mode is %d, and crc type is %d\n", addr, packetLen, mode, crcType);
    register_internal_putchar(putchar_);

    FILE* fd = fopen(argv[filePos], "rb");
    if (fd == nullptr) {
        printf("Cannot open file %s\n", argv[filePos]);
        return -1;
    }
    fseek(fd, 0, SEEK_END);
    size_t fileLen = ftell(fd);
    rewind(fd);
    size_t newFileLen = fileLen;
    if ((fileLen & (packetLen - 1)) != 0) {
        printf("file length is not multiple of packetLen, need padding");
        newFileLen = fileLen + packetLen - (fileLen & (packetLen - 1));
    }
    uint8_t* buffer = (uint8_t*)malloc(newFileLen * sizeof(uint8_t));
    if (buffer == nullptr) {
        printf("Could not allocate buffer\n");
        fclose(fd);
        return -1;
    }
    fread(buffer, sizeof(uint8_t), fileLen, fd);
    if (fileLen != newFileLen) {
        for (int i=fileLen; i<newFileLen; ++i) {
            buffer[i] = 0xFF;   //padding with 0xFF
        }
        fileLen = newFileLen;
    }
    fclose(fd);
    if (!can_connect(USED_CAN_CHN, USED_CAN_SPEED)) {
        printf("USBCAN connection failed");
        free(buffer);
        return -1;
    }
    if (!can_getDeviceInfo(sn)) {
        printf("could not fetch USBCAN serial number\n");
        retCode = -1;
        goto bailout;
    }
    printf("connected USBCAN's serial number is %s\n", sn);

    if (can_getBootloaderVerCmd(addr, resp) < 0) {
        printf("could not fetch LV BMS bootloader's version\n");
        printf("Are you sure the board fw is in the dfu mode?\n");
        retCode = -1;
        goto bailout;
    }
    printf("LV BMS FW bootloader's version is %d.%d.%d, build is %d, HW is %d\n",
        resp[3], resp[2], resp[1], resp[0], resp[4]);

    if (can_getApplicationVerCmd(addr, resp) < 0) {
        printf("could not fetch LV BMS app's version\n");
        retCode = -1;
        goto bailout;
    }
    printf("LV BMS FW application's version is %d.%d.%d, build is %d\n",
        resp[4], resp[3], resp[2], (resp[1] << 8) | resp[0]);
    memset(resp, '\0', 8);
    if (can_getHardwareTypeCmd(addr, resp) < 0) {
        printf("could not get hardware type\n");
        retCode = -1;
        goto bailout;
    }
#if 0
    printf("LV BMS's hardware type is %s\n", resp);
    if (can_getHardwareInfoCmd(addr, resp) < 0) {
        printf("could not get hardware info\n");
        retCode = -1;
        goto bailout;
    }
    printf("LV BMS's hardware build date is year:20%02d, month:%02d, day:%02d, batch no: %04d\n",
        resp[0], resp[1], resp[2], (resp[4] << 8) | resp[3]);

    if (can_getBatterySN(addr, (uint8_t *)batterySN) < 0) {
        printf("could not get battery sn\n");
        retCode = -1;
        goto bailout;
    }
    printf("LV BMS's battery sn is %s\n", batterySN);
#endif 
    if (can_prepareCmd(addr, resp) < 0) {
        printf("could not prepare dfu upgrade\n");
        retCode = -1;
        goto bailout;
    }
    printf("LV BMS' battery cell num is %d\n", resp[0]);
    if (packetLen != 0x80) {
        if (can_setPacketLenCmd(addr, packetLen) < 0) {
            printf("try to set packet length %d failed\n", packetLen);
            retCode = -1;
            goto bailout;
        }
        if (can_getPacketLenCmd(addr, resp) < 0) {
            printf("try to get packet length failed\n");
            retCode = -1;
            goto bailout;
        }
        else {
            newPacketLen = *(uint32_t*)resp;
            printf("new packet length %u\n", newPacketLen);
        }
        if (newPacketLen != packetLen) {
            printf("packetLen are not equal\n");
            retCode = -2;
            goto bailout;
        }
    }
    if (can_setApplicationLenCmd(addr, fileLen, resp) < 0 || *(uint32_t *)resp != fileLen) {
        printf("try to set application length %u failed\n", fileLen);
        retCode = -1;
        goto bailout;
    }
    
    while (seq <= fileLen/packetLen) {
        if (can_setPacketSeqCmd(addr, seq, resp) < 0 || *(uint16_t*)resp != seq) {
            printf("try to set packet sequence num %d failed\n", seq);
            retCode = -1;
            goto bailout;
        }
        if (can_sendPacketData(packetLen, buffer + (seq-1) * packetLen) < 0) {
            printf("try to send packet data for seq %d failed\n", seq);
            retCode = -1;
            goto bailout;
        }
        crc = crc16(buffer + (seq-1) * packetLen, packetLen, 0xFFFF);
        printf("packet seq %d 's crc is 0x%04x\n", seq, crc);
        if (can_verifyPacketDataCmd(addr, crc) < 0) {
            printf("try to verify packet crc for seq %d failed\n", seq);
            retCode = -1;
            goto bailout;
        }
        ++seq;
    }
    if (crcType == 0) {
        fileCrc = crc16(buffer, fileLen, 0xFFFF);
    } else {    //crcType == 1
        fileCrc = crc32(buffer, fileLen, 0);
    }
    if (can_verifyAllDataCmd(addr, crcType, fileCrc) < 0) {
        printf("try to set verify application failed\n");
        retCode = -1;
        goto bailout;
    } else {
        if (crcType == 0) {
            printf("whole file length is %u, crc uses crc16 : 0x%04x\n", fileLen, fileCrc);
        } else {
            printf("whole file length is %u, crc uses crc32 : 0x%08x\n", fileLen, fileCrc);
        }
    }
    if (mode == 0) {
        if (can_updateCurrentStationCmd(addr) < 0) {
            printf("try to update current station failed\n");
            retCode = -1;
            goto bailout;
        }
    } else {  //mode == 1  
        if (can_updateAllStationCmd() < 0) {
            printf("try to update all stations failed\n");
            retCode = -1;
            goto bailout;
        }
    }
    while (true) {
        if (can_getUpdateStatusCmd(addr, resp) < 0) {
            printf("try to get update status failed\n");
            retCode = -1;
            goto bailout;
        }
        status = *resp;
        if (status == 0xAA) {
            printf("update bms app successfully\n");
            retCode = 0;
            break;
        } else if (status == 0x0C) {
            printf("progressing: transfer bms app internal data\n");
            std::this_thread::sleep_for(std::chrono::microseconds(10000));   //10ms
        } else if (status == 0x0D) {
            printf("progressing: verify bms internal crc\n");
            std::this_thread::sleep_for(std::chrono::microseconds(10000));   //10ms
        } else {
            printf("update bms app failed, error code is %hhu\n", status);
            retCode = -1;
            goto bailout;
        }
    }
bailout:
    can_disconnect();
    printf("USBCAN disconnect successfully\n");
    free(buffer);
	return retCode;
}
