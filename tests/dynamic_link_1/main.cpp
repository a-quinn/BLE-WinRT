#include <iostream>
#include <windows.h>
#include "BLE_WinRT.h"

int main() {
    std::cout << "Dynamic link test: starting BLE scan..." << std::endl;
    StartDeviceScan();
    DeviceUpdate device = {};
    while (true) {
        ScanStatus status = PollDevice(&device, false);
        if (status == ScanStatus::AVAILABLE) {
            std::wcout << L"Device found: " << device.id << L" Name: " << device.name << std::endl;
        }
        if (status == ScanStatus::FINISHED) break;
    }
    StopDeviceScan();
    std::cout << "Scan finished." << std::endl;
    Quit();
    return 0;
}
