#pragma once

#include <iostream>
#include <vector>
#include <windows.h>
#include <cstdio> // For _wcsicmp on Windows
#include <chrono>
#include <functional>
#include <codecvt>
#include "BLE_WinRT_Context.h"

bool compareMAC(const wchar_t* deviceId, const wchar_t* targetMacStr);
class BluetoothClient {
	// This class is not thread safe, nor does it handle cases of device disconnection.
public:
    BluetoothClient() = default;

    // This must be called to cleanup WinRT resources properly, meaning it needs to be called during CTRL+C handling as well
    ~BluetoothClient() {
        bl_device_->Quit(); // otherwise subsequent use of WinRT Bluetooth APIs will return "Access Denied" errors
        bl_device_.reset();
    }
    
    std::vector<std::string> listDevices(std::chrono::duration<double> timeout);
    void scanDevices(std::function<void(const DeviceUpdate&)> callback, std::chrono::duration<double> timeout);

    bool connectToDevice(const std::wstring& deviceId);
    bool scanForService(const std::wstring& serviceId);
    bool scanForCharacteristic(const std::wstring& serviceId, const std::wstring& characteristicId);
    bool subscribeToCharacteristic(const std::wstring& serviceId, const std::wstring& characteristicId, std::function<void(const BLEData&)> dataCallback);

	bool unsubscribe(const std::wstring& serviceId, const std::wstring& characteristicId);

    wchar_t deviceID_[100] = L"";
    std::shared_ptr<BLEWinRTContext> bl_device_ = std::make_shared<BLEWinRTContext>();
private:
};
