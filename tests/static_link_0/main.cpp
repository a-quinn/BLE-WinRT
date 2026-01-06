#include <iostream>
#include "BLE_WinRT.h"

int main() {
    // Simple static link test for CI: just check if a function can be called
    try {
        StartDeviceScan(); // Should not fail if library is linked
        StopDeviceScan();
        Quit();
        std::cout << "Static link test: BLE_WinRT loaded successfully." << std::endl;
        return 0;
    } catch (...) {
        std::cout << "ERROR: BLE_WinRT static link failed!" << std::endl;
        return 1;
    }
}
