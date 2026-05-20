#pragma once
#include <mutex>
#include <queue>
#include <condition_variable>
#include "pch.h"

#if defined(BLE_WINRT_CONTEXT_DLL_EXPORTS)
    #define DLL_API __declspec(dllexport)
#elif defined(BLE_WINRT_CONTEXT_DLL_IMPORTS)
    #define DLL_API __declspec(dllimport)
#else
    #define DLL_API
#endif

using namespace std;

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Web::Syndication;

using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;

using namespace Windows::Storage::Streams;

struct DeviceUpdate {
	wchar_t id[100];
	bool isConnectable = false;
	bool isConnectableUpdated = false;
	wchar_t name[50];
	bool nameUpdated = false;
};

struct Service {
	wchar_t uuid[100];
};

struct Characteristic {
	wchar_t uuid[100];
	wchar_t userDescription[100];
};

struct BLEData {
	uint8_t buf[512];
	uint16_t size;
	wchar_t deviceId[256];
	wchar_t serviceUuid[256];
	wchar_t characteristicUuid[256];
};

struct ErrorMessage {
	wchar_t msg[1024];
};

enum class ScanStatus { PROCESSING, AVAILABLE, FINISHED };

class DLL_API BLEWinRTContext {
public:
    BLEWinRTContext();
    ~BLEWinRTContext();

    // BLE API wrappers
    void StartDeviceScan();
    ScanStatus PollDevice(DeviceUpdate* device, bool block);
    void StopDeviceScan();
    void ScanServices(wchar_t* deviceId);
    ScanStatus PollService(Service* service, bool block);
    void ScanCharacteristics(wchar_t* deviceId, wchar_t* serviceId);
    ScanStatus PollCharacteristic(Characteristic* characteristic, bool block);
    bool SubscribeCharacteristic(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, bool block);
    bool UnsubscribeCharacteristic(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, bool block);
    bool TriggerCharacteristicRead(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, bool block);
    bool ReadCharacteristic(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, BLEData* data, bool block);
    bool PollData(BLEData* data, bool block);
    bool SendData(BLEData* data, bool block);
    void Quit();
    void GetError(ErrorMessage* buf);
    void ClearCache(bool onlyServices = false);
private:
    struct Impl; // Forward declaration
    Impl* pImpl; // Pointer to implementation

    
    IAsyncOperation<BluetoothLEDevice> retrieveDevice(wchar_t* deviceId);
    IAsyncOperation<GattDeviceService> retrieveService(wchar_t* deviceId, wchar_t* serviceId);
    IAsyncOperation<GattCharacteristic> retrieveCharacteristic(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId);

    bool QuittableWait(condition_variable& signal, unique_lock<mutex>& waitLock);
    bool QuittableWaitPredicate(condition_variable& signal, unique_lock<mutex>& waitLock, bool& predicate);
    void DeviceWatcher_Updated(DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate);
    void DeviceWatcher_Added(DeviceWatcher sender, DeviceInformation deviceInfo);
    void DeviceWatcher_EnumerationCompleted(DeviceWatcher sender, IInspectable const&);
    fire_and_forget ScanServicesAsync(wchar_t* deviceId);
    fire_and_forget ScanCharacteristicsAsync(wchar_t* deviceId, wchar_t* serviceId);
    void Characteristic_ValueChanged(GattCharacteristic const& characteristic, GattValueChangedEventArgs args);
    void Characteristic_NewValue(GattCharacteristic const& characteristic, IBuffer buffer);
    fire_and_forget SubscribeCharacteristicAsync(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, bool* result);
    fire_and_forget UnsubscribeCharacteristicAsync(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, bool* result);
    fire_and_forget TriggerCharacteristicReadAsync(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, bool* result);
    fire_and_forget ReadCharacteristicAsync(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, BLEData* data, std::condition_variable* signal, bool* result);
    fire_and_forget SendDataAsync(BLEData data, std::condition_variable* signal, bool* result);
    
    // Utility
    long hsh(wchar_t* wstr);
    void clearError();
    void saveError(const wchar_t* message, ...);
};
