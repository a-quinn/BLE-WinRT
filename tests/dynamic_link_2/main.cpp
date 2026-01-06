#include <iostream>
#include <windows.h>

/*
This test dynamically loads the BLE_WinRT.dll at runtime using LoadLibrary and GetProcAddress,
and calls its functions to perform a BLE scan. The other dynamic link test does not give control
to the programmer to check for DLL presence, therefore Windows may not tell you if the DLL is missing.
*/

// Function pointer types matching BLE_WinRT.h usually this is done via a header file but for clarity we define them here.
using StartDeviceScan_t = void (*)();
using PollDevice_t = int (*)(void*, bool);
using StopDeviceScan_t = void (*)();
using Quit_t = void (*)();

struct DeviceUpdate {
    wchar_t id[100];
    bool isConnectable;
    bool isConnectableUpdated;
    wchar_t name[50];
    bool nameUpdated;
};

enum class ScanStatus { PROCESSING, AVAILABLE, FINISHED };

int main() {
    HMODULE hLib = LoadLibraryA("BLE_WinRT.dll");
    if (!hLib) {
        std::cout << "ERROR: BLE_WinRT.dll not found at runtime!" << std::endl;
        return 1;
    }

    auto StartDeviceScan = (StartDeviceScan_t)GetProcAddress(hLib, "StartDeviceScan");
    auto PollDevice = (PollDevice_t)GetProcAddress(hLib, "PollDevice");
    auto StopDeviceScan = (StopDeviceScan_t)GetProcAddress(hLib, "StopDeviceScan");
    auto Quit = (Quit_t)GetProcAddress(hLib, "Quit");

    if (!StartDeviceScan || !PollDevice || !StopDeviceScan || !Quit) {
        std::cout << "ERROR: Could not find required functions in BLE_WinRT.dll!" << std::endl;
        FreeLibrary(hLib);
        return 2;
    }

    std::cout << "Dynamic link test (LoadLibrary): starting BLE scan..." << std::endl;
    StartDeviceScan();
    DeviceUpdate device = {};
    while (true) {
        int status = PollDevice(&device, false);
        if (status == (int)ScanStatus::AVAILABLE) {
            std::wcout << L"Device found: " << device.id << L" Name: " << device.name << std::endl;
        }
        if (status == (int)ScanStatus::FINISHED) break;
    }
    StopDeviceScan();
    std::cout << "Scan finished." << std::endl;
    Quit();
    FreeLibrary(hLib);
    return 0;
}
