// Simple Bluetooth library interface implementation for Windows using WinRT APIs
// 
// This file is used to demonstrate how to use the BLEWinRTContext class, and can
// be used as a starting point for users of the library to implement their own interface.

#define NOMINMAX 1 // prevent windows.h from defining max and min macro
#include <windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <queue>

#include "BLE_WinRT_Context.h"

BLEWinRTContext* bl_device_ptr = new BLEWinRTContext();

void ble_backend_quit() {
    bl_device_ptr->Quit();
}

// Helper func that also handles UTF-8 encoding, but requires windows.h.
std::wstring string_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

// convert string address to uint64_t
uint16_t hexCharToByte(const std::string& address) {
    uint16_t byte = 0;
    for (char c : address) {
        byte <<= 4;
        if (c >= '0' && c <= '9') {
            byte |= (c - '0');
        } else if (c >= 'A' && c <= 'F') {
            byte |= (c - 'A' + 10);
        } else if (c >= 'a' && c <= 'f') {
            byte |= (c - 'a' + 10);
        }
    }
    return byte;
}

auto formattedUUID = [](const std::string& str) {
    // force lowercase
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    // add a '{' and '}' around the string and convert to wstring
    std::string wrapped = "{" + str + "}";
    return string_to_wstring(wrapped);
};

bool compareMAC(const wchar_t* deviceId, const wchar_t* targetMacStr) {
    // device.id look like "BluetoothLE#BluetoothLE88:d8:2e:42:c4:e3-d9:32:8a:f9:2a:ad"
    // targetMacStr look like "88d82e42c4e3"
    // from device.id remove prefix by scanning until '-' then remove all ':'

    const wchar_t* macStart = wcschr(deviceId, L'-');
    if (macStart) {
        macStart++; // move past '-'
        std::wstring macStr;
        for (const wchar_t* p = macStart; *p != L'\0'; ++p) {
            if (*p != L':') {
                macStr += *p;
            }
        }
        return macStr == targetMacStr;
    }
    return false;
}

struct CharKey {
    std::wstring deviceId;
    std::wstring serviceId;
    std::wstring characteristicId;

    // force service and characteristic UUIDs to lowercase as this is what WinRT uses
    CharKey(const wchar_t* devId, const wchar_t* servId, const wchar_t* charId)
        : deviceId(devId), serviceId(servId), characteristicId(charId) {
        std::transform(serviceId.begin(), serviceId.end(), serviceId.begin(), ::towlower);
        std::transform(characteristicId.begin(), characteristicId.end(), characteristicId.begin(), ::towlower);
    }

    bool operator==(const CharKey& other) const {
        return deviceId == other.deviceId &&
               serviceId == other.serviceId &&
               characteristicId == other.characteristicId;
    }
    bool operator==(const std::tuple<const wchar_t*, const wchar_t*, const wchar_t*>& other) const {
        return deviceId == std::get<0>(other) &&
               serviceId == std::get<1>(other) &&
               characteristicId == std::get<2>(other);
    }
};

template<>
struct std::hash<CharKey> {
    std::size_t operator()(const CharKey& k) const {
        return hash<std::wstring>()(k.deviceId) ^
                (hash<std::wstring>()(k.serviceId) << 1) ^
                (hash<std::wstring>()(k.characteristicId) << 2);
    }
};

class BluetoothDevices {
public:
    BluetoothDevices() {
        
    }
    ~BluetoothDevices() {
        shutdown_ = true;
        ble_backend_quit();
        if (dataPollingThread_ && dataPollingThread_->joinable()) {
            dataPollingThread_->join();
        }
    }
private:
    bool shutdown_ = false;
    // pair of connected devices and their addresses
    std::vector<std::pair<std::wstring, std::string>> connectedDevices_;
    // tuple for subscribed characteristics: deviceId, serviceId, characteristicId
    std::vector<std::tuple<std::wstring, std::wstring, std::wstring>> subscribedCharacteristics_;
    // a version using wchar_t for faster comparison, including a function callback
    std::unordered_map<CharKey, std::function<void(const BLEData&)>> subscribedCharacteristicsW_;

    // reference to thread that will poll data
    std::shared_ptr<std::thread> dataPollingThread_;

    bool is_subscribed(const std::wstring& deviceID, const std::wstring& serviceId, const std::wstring& characteristicId) {
        if (std::find(subscribedCharacteristics_.begin(), subscribedCharacteristics_.end(),
                std::make_tuple(deviceID, serviceId, characteristicId)) != subscribedCharacteristics_.end()) {
            return true;
        }
        return false;
    }

    bool is_subscribed(const CharKey& key) {
        if (subscribedCharacteristicsW_.find(key) != subscribedCharacteristicsW_.end()) {
            return true;
        }
        return false;
    }
    bool is_subscribed(const wchar_t* deviceID, const wchar_t* serviceId, const wchar_t* characteristicId) {
        return is_subscribed(CharKey{deviceID, serviceId, characteristicId});
    }

    std::function<void(const BLEData&)> get_callback(const CharKey& key) {
        auto it = subscribedCharacteristicsW_.find(key);
        if (it != subscribedCharacteristicsW_.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::function<void(const BLEData&)> get_callback(const wchar_t* deviceID, const wchar_t* serviceId, const wchar_t* characteristicId) {
        return get_callback(CharKey{deviceID, serviceId, characteristicId});
    }

public:

    bool connect(const std::string& address) {
        wchar_t targetMacStr[100];
        size_t j = 0;
        for (char c : address) {
            if (c != ':') {
                targetMacStr[j++] = c;
            }
        }
        targetMacStr[j] = L'\0';

        bl_device_ptr->StartDeviceScan();
        DeviceUpdate device = {};
        bool found = false;
        std::wcout << L"Scanning for devices..." << std::endl;
        while (!found) {
            ScanStatus status = bl_device_ptr->PollDevice(&device, true);
            if (status == ScanStatus::AVAILABLE || status == ScanStatus::FINISHED) {
                //std::wcout << L"Device found: " << device.id << L" Name: " << device.name << std::endl;
                if (compareMAC(device.id, targetMacStr)) {
                    std::wcout << L"Found device: " << device.id << L" Name: " << device.name << std::endl;
                    found = true;
                }
            }
            if (status == ScanStatus::FINISHED) break;
        }
        bl_device_ptr->StopDeviceScan();
        if (found) {
            connectedDevices_.push_back(std::make_pair(device.id, address));
        }
        return found;
    }

    void disconnect(const std::string& address) {
        for (auto it = connectedDevices_.begin(); it != connectedDevices_.end(); ++it) {
            if (it->second == address) {
                connectedDevices_.erase(it);
                break;
            }
        }
    }

    // TODO: implement actual connection check
    bool is_connected(const std::string& address) {
        for (const auto& device : connectedDevices_) {
            if (device.second == address) {
                return true;
            }
        }
        return false;
    }

    bool checkServiceExists(const std::string& address, const std::string& serviceUUID, bool verbose = false) {
        std::wstring serviceIdW = formattedUUID(serviceUUID);
        std::wstring deviceIdW;
        if (!get_device_id_from_address(address, deviceIdW)) {
            std::cerr << "Device at " << address << " not connected." << std::endl;
            return false;
        }
        bl_device_ptr->ScanServices((wchar_t*)deviceIdW.c_str());
        Service service = {};
	    if (verbose) {
            std::wcout << L"Services:" << std::endl;
        }
        while (true) {
            ScanStatus status = bl_device_ptr->PollService(&service, false);
            if (status == ScanStatus::AVAILABLE) {
			    if (verbose) {
                    std::wcout << L"  " << service.uuid << std::endl;
                }
                if (serviceIdW == service.uuid) {
                    return true;
                }
            } else if (status == ScanStatus::FINISHED) {
                break;
            }
        }
        return false;
    }

    bool checkCharacteristicExists(const std::string& address, const std::string& serviceUUID, const std::string& characteristicUUID, bool verbose = false) {
        std::wstring serviceIdW = formattedUUID(serviceUUID);
        std::wstring characteristicIdW = formattedUUID(characteristicUUID);
        std::wstring deviceIdW;
        if (!get_device_id_from_address(address, deviceIdW)) {
            std::cerr << "Device at " << address << " not connected." << std::endl;
            return false;
        }
        
        // use ScanCharacteristics
        bl_device_ptr->ScanServices((wchar_t*)deviceIdW.c_str());
        Service service = {};
        if (verbose) {
            std::wcout << L"Services:" << std::endl;
        }
        while (true) {
            ScanStatus status = bl_device_ptr->PollService(&service, false);
            if (status == ScanStatus::AVAILABLE) {
                std::wcout << L"  " << service.uuid << std::endl;
                if (serviceIdW == service.uuid) {
                    // found service, now scan characteristics
                    bl_device_ptr->ScanCharacteristics((wchar_t*)deviceIdW.c_str(), (wchar_t*)serviceIdW.c_str());
                    Characteristic characteristic = {};
                    if (verbose) {
                        std::wcout << L"Characteristics:" << std::endl;
                    }
                    while (true) {
                        ScanStatus charStatus = bl_device_ptr->PollCharacteristic(&characteristic, false);
                        if (charStatus == ScanStatus::AVAILABLE) {
                            if (verbose) {
                                std::wcout << L"  " << characteristic.uuid << std::endl;
                            }
                            if (characteristicIdW == characteristic.uuid) {
                                return true;
                            }
                        } else if (charStatus == ScanStatus::FINISHED) {
                            break;
                        }
                    }
                }
            } else if (status == ScanStatus::FINISHED) {
                break;
            }
        }
        return false;
    }

    bool subscribeToCharacteristic(const std::wstring& deviceID, const std::wstring& serviceId, const std::wstring& characteristicId, std::function<void(const BLEData&)> dataCallback, bool debug = false) {
        if (debug) {
            std::wcout << L"Subscribing to characteristic:" << std::endl;
            std::wcout << L"  Device ID: " << deviceID << std::endl;
            std::wcout << L"  Service ID: " << serviceId << std::endl;
            std::wcout << L"  Characteristic ID: " << characteristicId << std::endl;
        }
        bool result = bl_device_ptr->SubscribeCharacteristic((wchar_t*)deviceID.c_str(), (wchar_t*)serviceId.c_str(), (wchar_t*)characteristicId.c_str(), true);
        if (result) {
            if (is_subscribed(deviceID, serviceId, characteristicId)) {
                if (debug) {
                    std::wcout << L"Characteristic already subscribed." << std::endl;
                }
                return result;
            }

            subscribedCharacteristics_.push_back(std::make_tuple(deviceID, serviceId, characteristicId));
            subscribedCharacteristicsW_.insert({CharKey{deviceID.c_str(), serviceId.c_str(), characteristicId.c_str()}, dataCallback});
            
            if (!dataPollingThread_ || !dataPollingThread_->joinable()) {
                
                if (debug) {
                    std::wcout << L"Starting data polling thread..." << std::endl;
                }
                dataPollingThread_ = std::make_shared<std::thread>([this]() {
                    BLEData data = {};
                    while (!shutdown_) {
                        if (bl_device_ptr->PollData(&data, true)) {
                            auto cb = get_callback(CharKey{data.deviceId, data.serviceUuid, data.characteristicUuid});
                            if (cb) {
                                cb(data);
                            } else {
                                std::wcout << L"Received data for unmatched characteristic/service/device. Ignoring." << std::endl;
                                std::wcout << L"Characteristic: " << data.characteristicUuid << std::endl;
                                std::wcout << L"Service: " << data.serviceUuid << std::endl;
                                std::wcout << L"Device: " << data.deviceId << std::endl;
                            }
                        }
                    }
                });
            }
        }
        return result;
    }

    bool unsubscribe(const std::wstring& deviceID, const std::wstring& serviceId, const std::wstring& characteristicId) {
        if (!is_subscribed(deviceID, serviceId, characteristicId)) {
            std::cerr << "Characteristic not subscribed." << std::endl;
            return true;
        }

        if (!bl_device_ptr->UnsubscribeCharacteristic((wchar_t*)deviceID.c_str(), (wchar_t*)serviceId.c_str(), (wchar_t*)characteristicId.c_str(), true)) {
            std::wcerr << L"Failed to unsubscribe from characteristic " << characteristicId << L" on device " << deviceID << std::endl;
            return false;
        }

        subscribedCharacteristics_.erase(
            std::remove(subscribedCharacteristics_.begin(), subscribedCharacteristics_.end(),
                std::make_tuple(deviceID, serviceId, characteristicId)),
            subscribedCharacteristics_.end());
        subscribedCharacteristicsW_.erase(CharKey{deviceID.c_str(), serviceId.c_str(), characteristicId.c_str()});
        return true;
    }

    bool get_device_id_from_address(const std::string& address, std::wstring& deviceID) {
        for (const auto& pair : connectedDevices_) {
            if (pair.second == address) {
                deviceID = pair.first;
                return true;
            }
        }
        return false;
    }

    bool start_notify(const std::string& address, const std::string& serviceUUID, const std::string& characteristicUUID, std::function<void(const std::vector<uint8_t>&)> cb, bool verbose = false) {
        std::wstring serviceIdW = formattedUUID(serviceUUID);
        std::wstring characteristicIdW = formattedUUID(characteristicUUID);
        std::wstring deviceIdW;
        if (!get_device_id_from_address(address, deviceIdW)) {
            std::cerr << "Device at " << address << " not connected." << std::endl;
            return false;
        }

        bool ret = subscribeToCharacteristic(deviceIdW, serviceIdW, characteristicIdW, [cb](const BLEData& data) {
            std::vector<uint8_t> vec(data.buf, data.buf + data.size);
            cb(vec);
        }, verbose);
        if (!ret) {
            std::cerr << "Failed to subscribe to characteristic " << characteristicUUID << " on device " << address << std::endl;
        }
        return ret;
    }

    bool stop_notify(const std::string& address, const std::string& serviceUUID, const std::string& characteristicUUID) {
        std::wstring serviceIdW = formattedUUID(serviceUUID);
        std::wstring characteristicIdW = formattedUUID(characteristicUUID);
        std::wstring deviceIdW;
        if (!get_device_id_from_address(address, deviceIdW)) {
            std::cerr << "Device at " << address << " not connected." << std::endl;
            return false;
        }
        if (!unsubscribe(deviceIdW, serviceIdW, characteristicIdW)) {
            std::cerr << "Failed to unsubscribe from characteristic " << characteristicUUID << " on device " << address << std::endl;
            return false;
        }
        return true;
    }

    // the read will trigger a notification that will be caught by the polling thread
    bool read_value(const std::string& address, const std::string& serviceUUID, const std::string& characteristicUUID) {
        std::wstring serviceIdW = formattedUUID(serviceUUID);
        std::wstring characteristicIdW = formattedUUID(characteristicUUID);
        std::wstring deviceIdW;
        if (!get_device_id_from_address(address, deviceIdW)) {
            std::cerr << "Device at " << address << " not connected." << std::endl;
            return false;
        }
        bool ret = bl_device_ptr->TriggerCharacteristicRead((wchar_t*)deviceIdW.c_str(), (wchar_t*)serviceIdW.c_str(), (wchar_t*)characteristicIdW.c_str(), true);
        if (!ret) {
            std::cerr << "Failed to read characteristic " << characteristicUUID << " on device " << address << std::endl;
        }
        return ret;
    }

};