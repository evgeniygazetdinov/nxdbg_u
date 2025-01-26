#include <switch.h>
#include <cstdarg>
#include <cstdio>
#include "commands.h"

inline void printer(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    printf("%s\n", buffer);
    svcSleepThread(250000000ULL);
    consoleUpdate(NULL);
}


#define USB_DT_BOS 0x0F

// USB descriptor sizes
#define USB_DT_CONFIG_SIZE 9
#define USB_DT_INTERFACE_SIZE 9
#define USB_DT_ENDPOINT_SIZE 7

// USB configuration attributes
#define USB_CONFIG_ATT_ONE      (1 << 7) // Must be set
#define USB_CONFIG_ATT_SELFPOWER    (1 << 6) // Self powered
#define USB_CONFIG_ATT_WAKEUP   (1 << 5) // Can wake up
#define USB_CONFIG_ATT_BATTERY  (1 << 4) // Battery powered

// USB endpoints
static UsbDsEndpoint *g_usbComms_endpoint_in = NULL;
static UsbDsEndpoint *g_usbComms_endpoint_out = NULL;
static u8 g_usbComms_endpoint_in_buffer[0x1000];
static u8 g_usbComms_endpoint_out_buffer[0x1000];
static u8 g_usbComms_buffer[0x1000];
static UsbDsInterface* g_interface = NULL;
static UsbDsEndpoint* g_endpoint_in = NULL;
static UsbDsEndpoint* g_endpoint_out = NULL;


inline const char* getUsbStateName(UsbState state) {
    switch (state) {
        case UsbState_Detached:   return "Detached (0)";
        case UsbState_Attached:   return "Attached (1)";
        case UsbState_Powered:    return "Powered (2)";
        case UsbState_Default:    return "Default (3)";
        case UsbState_Address:    return "Address (4)";
        case UsbState_Configured: return "Configured (5)";
        default:                  return "Unknown";
    }
}

// USB дескрипторы и информация об устройстве
static UsbDsDeviceInfo g_device_info = {
    .idVendor = 0x057E,
    .idProduct = 0x3000,
    .bcdDevice = 0x0100,
    .Manufacturer = "Nintendo",
    .Product = "NXDebugger",
    .SerialNumber = "0001"
};

static usb_interface_descriptor g_interface_descriptor = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = USBDS_DEFAULT_InterfaceNumber,
    .bAlternateSetting = 0x00,
    .bNumEndpoints = 0x02,
    .bInterfaceClass = 0xFF,
    .bInterfaceSubClass = 0xFF,
    .bInterfaceProtocol = 0xFF,
};

static usb_endpoint_descriptor g_endpoint_descriptor_in = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_IN,
    .bmAttributes = USB_TRANSFER_TYPE_BULK,
    .wMaxPacketSize = 0x200,
};

static usb_endpoint_descriptor g_endpoint_descriptor_out = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_OUT,
    .bmAttributes = USB_TRANSFER_TYPE_BULK,
    .wMaxPacketSize = 0x200,
};

static const struct {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor interface;
    struct usb_endpoint_descriptor endpoint_in;
    struct usb_endpoint_descriptor endpoint_out;
} __attribute__((packed)) config_descriptor = {
    .config = {
        .bLength = USB_DT_CONFIG_SIZE,
        .bDescriptorType = USB_DT_CONFIG,
        .wTotalLength = sizeof(config_descriptor),
        .bNumInterfaces = 1,
        .bConfigurationValue = 1,
        .iConfiguration = 0,
        .bmAttributes = USB_CONFIG_ATT_ONE,
        .MaxPower = 250
    },
    .interface = {
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
        .bInterfaceSubClass = 0x93,  // Специальный подкласс для отладки
        .bInterfaceProtocol = 0x01,  // Протокол версии 1
        .iInterface = 0
    },
    .endpoint_in = {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_ENDPOINT_IN | 1,
        .bmAttributes = USB_TRANSFER_TYPE_BULK,
        .wMaxPacketSize = 0x40,
        .bInterval = 0
    },
    .endpoint_out = {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_ENDPOINT_OUT | 2,
        .bmAttributes = USB_TRANSFER_TYPE_BULK,
        .wMaxPacketSize = 0x40,
        .bInterval = 0
    }
};




const int KEY_MINUS = 2048;
const int KEY_PLUS = 1024;

const int KEY_X = 4;
const int KEY_Y = 28;
const int KEY_A = 5;
const int KEY_B = 27;






static const usb_device_descriptor device_descriptor = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0xFF,
    .bDeviceSubClass = 0xFF,
    .bDeviceProtocol = 0xFF,
    .bMaxPacketSize0 = 0x40,
    .idVendor = 0x057E,
    .idProduct = 0x3000,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1
};



// Обновляем VID/PID на более подходящие для отладки
// Используем VID/PID от Nintendo Pro Controller
static const UsbDsDeviceInfo device_info = {
    .idVendor = 0x057E,  // Nintendo
    .idProduct = 0x2009,  // Pro Controller
    .bcdDevice = 0x0100,
    .Manufacturer = u8"Nintendo",
    .Product = u8"Pro Controller",
    .SerialNumber = u8"0001"
};



Result initializeUsb();
int handleUsbCommand();
void sendUsbResponse(DebuggerResponse resp);
bool kernelAbove300(void);
Result tryUsbCommsRead2(void* buffer, size_t size, size_t *transferredSize, u64 timeout = 100000000ULL);