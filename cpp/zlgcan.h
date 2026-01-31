#pragma once

//only Support ZLG USBCAN
//Author : richard xu (junzexu@outlook.com)
//Date : Dec 02, 2026

#include <stdint.h>

//constants
#define CAN_EFF_FLAG 0x80000000u    //EFF/SFF is set in the MSB
#define CAN_RTR_FLAG 0x40000000u    //remote transmission request
#define CAN_ERR_FLAG 0x20000000u    //error message frame
#define CAN_ID_FLAG  0x1FFFFFFFu    //id

#define CAN_SEF_MASK 0x000007FFu    //standard frame format 
#define CAN_EFF_MASK 0x1FFFFFFFu    //extended frame format
#define CAN_ERR_MASK 0x1FFFFFFFu    //omit EFF, RTR, ERR flags

#define CAN_SFF_ID_BITS     11
#define CAN_EFF_ID_BITS     29

#define CANFD_BRS 0x01  // bit rate switch (second bitrate for payload data)
#define CANFD_ESI 0x02  // error state indicator of the transmitting node

#define ZCAN_USBCAN1  3
#define ZCAN_USBCAN2  4

#define TRANSMIT_NORMAL 0
#define TRANSMIT_SINGLE 1
#define TRANSMIT_SELFLOOP 2
#define TRANSMIT_SINGLE_SELFLOOP 3

#define STATUS_ERR              0
#define STATUS_OK               1
#define STATUS_ONLINE           2
#define STATUS_OFFLINE          3
#define STATUS_UNSUPPORTED      4
#define STATUS_BUFFER_TOO_SMALL 5

#define INVALID_DEVICE_HANDLE   0
#define INVALID_CHANNEL_HANDLE  0

//types
typedef struct {
    char const *type;
    char const *value;
    char const *desc;
} Options;

typedef struct {
    char const *type;
    char const *desc;
    int read_only;
    char const *format;
    double min_value;
    double max_value;
    char const *unit;
    double delta;
    char const *visible;
    char const *enable;
    int editable;
    Options **options;
} Meta;

typedef struct {
    char const *key;
    char const *value;
} Pair;

typedef struct ConfigNode_ {
    char const *name;
    char const *value;
    char const *binding_value;
    char const *path;
    Meta *meta_info;
    struct ConfigNode_ **children;
    Pair **attributes;
} ConfigNode;

typedef ConfigNode const *(*GetPropertysFunc)();
typedef int (*SetValueFunc)(char const *path, char const *value);
typedef char const *(*GetValueFunc)(char const *path);

typedef struct {
    SetValueFunc SetValue;
    GetValueFunc GetValue;
    GetPropertysFunc GetPropertys;
} IProperty;

typedef struct {
    uint32_t can_id;
    uint8_t can_dlc;
    uint8_t __pad;
    uint8_t __res0;
    uint8_t __res1;
    uint8_t data[8];  // __attribute__((aligned(8)))
} can_frame;

typedef struct {
    uint16_t hw_Version;
    uint16_t fw_Version;
    uint16_t dr_Version;
    uint16_t in_Version;
    uint16_t irq_Num;
    uint8_t can_Num;
    char str_Serial_Num[20];
    char str_hw_Type[40];
    uint16_t reserved[4];
} ZCAN_DEVICE_INFO;

typedef struct {
    uint32_t can_type;
    union {
        struct {
            uint32_t acc_code;
            uint32_t acc_mask;
            uint32_t reserved;
            uint8_t filter;
            uint8_t timing0;
            uint8_t timing1;
            uint8_t mode;
        } can;
        struct {
            uint32_t acc_code;
            uint32_t acc_mask;
            uint32_t abit_timing;
            uint32_t dbit_timing;
            uint32_t brp;
            uint8_t filter;
            uint8_t mode;
            uint16_t pad;
            uint32_t reserved;
        } canfd;
    };
} ZCAN_CHANNEL_INIT_CONFIG;

typedef struct {
    can_frame frame;
    uint32_t transmit_type;
} ZCAN_Transmit_Data;

typedef struct {
    can_frame frame;
    uint64_t timestamp; //us
} ZCAN_Receive_Data;

typedef void *DEVICE_HANDLE;
typedef void *CHANNEL_HANDLE;

//macros
#define MAKE_CAN_ID(id, eff, rtr, err)  (id | (!!(eff) << 31) | (!!(rtr) << 30) | (!!(err) << 29))
#define IS_EFF(id)  (!!(id & CAN_EFF_FLAG)) //1:extend frame, 0:standard frame
#define IS_RTR(id)  (!!(id & CAN_RTR_FLAG)) //1:remote frame, 0:data frame
#define IS_ERR(id)  (!!(id & CAN_ERR_FLAG)) //1:error frame, 0:normal frame
#define GET_ID(id)  (id & CAN_ID_FLAG)

typedef DEVICE_HANDLE (__stdcall *can_openDevice)(uint32_t device_type, uint32_t device_index, uint32_t reserved);
typedef uint32_t (__stdcall *can_closeDevice)(DEVICE_HANDLE device_handle);
typedef uint32_t (__stdcall *can_getDeviceInf)(DEVICE_HANDLE device_handle, ZCAN_DEVICE_INFO *pInfo);
typedef uint32_t (__stdcall *can_isDeviceOnLine)(DEVICE_HANDLE device_handle);
typedef CHANNEL_HANDLE (__stdcall *can_initCAN)(DEVICE_HANDLE device_handle, uint32_t can_index, ZCAN_CHANNEL_INIT_CONFIG *pInitConfig);
typedef uint32_t (__stdcall *can_startCAN)(CHANNEL_HANDLE channel_handle);
typedef uint32_t (__stdcall *can_resetCAN)(CHANNEL_HANDLE channel_handle);
typedef uint32_t (__stdcall *can_clearBuffer)(CHANNEL_HANDLE channel_handle);
typedef uint32_t (__stdcall *can_getReceiveNum)(CHANNEL_HANDLE channel_handle, uint8_t type);
typedef uint32_t (__stdcall *can_transmit)(CHANNEL_HANDLE channel_handle, ZCAN_Transmit_Data *pTransmit, uint32_t len);
typedef uint32_t (__stdcall *can_receive)(CHANNEL_HANDLE channel_handle, ZCAN_Receive_Data *pReceive, uint32_t len, int wait_time);
typedef uint32_t (__stdcall *can_setValue)(DEVICE_HANDLE device_handle, char const *path, void const *value);
typedef void const *(__stdcall *can_getValue)(DEVICE_HANDLE device_handle, char const *path);
typedef IProperty *(__stdcall *can_getIProperty)(DEVICE_HANDLE device_handle);
typedef uint32_t (__stdcall *can_geleaseIProperty)(IProperty *pIProperty);
