#include <switch.h>
#include "variable.hpp"


Result initializeUsb() {
    Result rc = 0;
    int retryCount = 0;
    const int maxRetries = 10;
    
    printer("Starting USB initialization...");
    usbDsExit();
    svcSleepThread(1000000000ULL); // 1 секунда

    UsbState state;
    rc = usbDsGetState(&state);
    if (R_SUCCEEDED(rc)) {
        printer("Initial USB state: %s", getUsbStateName(state));
    }
    
    while (retryCount < maxRetries) {
        printer("\nTry %d/%d:", retryCount + 1, maxRetries);
        
        // Инициализируем USB
        rc = usbDsInitialize();
        if (R_FAILED(rc)) {
            printer("usbDsInitialize failed: %x", rc);
            retryCount++;
            if (retryCount < maxRetries) {
                printer("Retrying USB initialization...");
                svcSleepThread(1000000000ULL);
                continue;
            }
            return rc;
        }
        printer("usbDsInitialize success");

        // Проверяем состояние сразу после инициализации
        rc = usbDsGetState(&state);
        if (R_SUCCEEDED(rc)) {
            printer("USB state after init: %s", getUsbStateName(state));
        }

        // Пробуем принудительно активировать USB
        rc = usbDsEnable();
        if (R_FAILED(rc)) {
            printer("Failed to enable USB: %x", rc);
            usbDsExit();
            continue;
        }
        printer("USB enabled successfully");

        // Даем время на применение настроек
        svcSleepThread(1000000000ULL); // 1 секунда

        // Проверяем состояние после активации
        rc = usbDsGetState(&state);
        if (R_SUCCEEDED(rc)) {
            printer("USB state after enable: %s", getUsbStateName(state));
        }

        // Ждем состояния Address
        int waitCount = 0;
        while (waitCount < 50 && state != UsbState_Address) {
            svcSleepThread(100000000ULL);
            rc = usbDsGetState(&state);
            if (R_SUCCEEDED(rc)) {
                if (waitCount % 10 == 0) {
                    printer("Current USB state: %s", getUsbStateName(state));
                }
            }
        }

        if (state != UsbState_Address) {
            printer("Failed to reach Address state, current: %s", getUsbStateName(state));
            usbDsExit();
            retryCount++;
            continue;
        }
        retryCount++;
        printer("Reached Address state, creating interface...");

        // Копируем настройки из конфигурации
        g_interface_descriptor = config_descriptor.interface;
        g_endpoint_descriptor_in = config_descriptor.endpoint_in;
        g_endpoint_descriptor_out = config_descriptor.endpoint_out;

        UsbDsInterface* interface = NULL;
        rc = usbDsGetDsInterface(&interface, &g_interface_descriptor, "nx-debug");
        if (R_FAILED(rc)) {
            printer("Failed to get interface: %x, current state: %s", rc, getUsbStateName(state));
            usbDsExit();
            continue;
        }
        g_interface = interface;  // Сохраняем указатель
        printer("Interface created");

        // // Активируем интерфейс
        // rc = usbDsInterface_EnableInterface(g_interface);
        // if (R_FAILED(rc)) {
        //     printer("Failed to enable interface: %x", rc);
        //     usbDsExit();
        //     continue;
        // }
        // printer("Interface enabled");

        // Ждем, пока USB сконфигурируется
        int configWaitCount = 0;
        const int maxConfigWaits = 50;
        bool configured = false;

        while (configWaitCount < maxConfigWaits) {
            rc = usbDsGetState(&state);
            if (R_SUCCEEDED(rc)) {
                if (state == UsbState_Configured) {
                    configured = true;
                    printer("USB configured successfully, state: %s", getUsbStateName(state));
                    break;
                }
                if (configWaitCount % 10 == 0) {
                    printer("Waiting for USB configuration... Current state: %s", getUsbStateName(state));
                }
                if (state == UsbState_Address) {
                    printer("USB in address state");
                    break;
                }
            }
            svcSleepThread(100000000ULL);
            configWaitCount++;
        }

        if (!configured) {
            printer("USB configuration timeout, last state: %s", getUsbStateName(state));
            usbDsExit();
            retryCount++;
            continue;
        }

        // Создаем endpoints
        rc = usbDsInterface_GetDsEndpoint(g_interface, &g_endpoint_in, &g_endpoint_descriptor_in);
        if (R_FAILED(rc)) {
            printer("Failed to get input endpoint: %x", rc);
            usbDsExit();
            continue;
        }
        printer("Input endpoint created");

        rc = usbDsInterface_GetDsEndpoint(g_interface, &g_endpoint_out, &g_endpoint_descriptor_out);
        if (R_FAILED(rc)) {
            printer("Failed to get output endpoint: %x", rc);
            usbDsExit();
            continue;
        }
        printer("Output endpoint created");

        printer("USB initialization complete!");
        return 0;
    }

    printer("Failed to initialize USB after all retries");
    return -1;
}