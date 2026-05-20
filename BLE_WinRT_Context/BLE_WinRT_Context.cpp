#include "BLE_WinRT_Context.h"
#include "pch.h"

#define __WFILE__ L"BLE_WinRT_Context.cpp"

using namespace std;
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Storage::Streams;

struct BLEWinRTContext::Impl {// All previous global variables become members
    // WinRT BLE objects
    DeviceWatcher deviceWatcher{ nullptr };
    DeviceWatcher::Added_revoker deviceWatcherAddedRevoker;
    DeviceWatcher::Updated_revoker deviceWatcherUpdatedRevoker;
    DeviceWatcher::EnumerationCompleted_revoker deviceWatcherCompletedRevoker;

    std::queue<DeviceUpdate> deviceQueue{};
    std::mutex deviceQueueLock;
    std::condition_variable deviceQueueSignal;
    bool deviceScanFinished = false;

    std::queue<Service> serviceQueue{};
    std::mutex serviceQueueLock;
    std::condition_variable serviceQueueSignal;
    bool serviceScanFinished = false;

    std::queue<Characteristic> characteristicQueue{};
    std::mutex characteristicQueueLock;
    std::condition_variable characteristicQueueSignal;
    bool characteristicScanFinished = false;

    std::mutex quitLock;
    bool quitFlag = false;

    struct Subscription {
        GattCharacteristic characteristic = nullptr;
        GattCharacteristic::ValueChanged_revoker revoker;
    };
    std::list<Subscription*> subscriptions;
    std::mutex subscribeQueueLock;
    std::condition_variable subscribeQueueSignal;
    std::condition_variable unsubscribeQueueSignal;
	bool subscribeFinished = false;
	bool unsubscribeFinished = false;

	mutex readLock;
	condition_variable readSignal;

    std::queue<BLEData> dataQueue{};
    std::mutex dataQueueLock;
    std::condition_variable dataQueueSignal;

    // Error handling
    std::mutex errorLock;
    wchar_t last_error[2048] = L"Ok";

    // Caching
    struct CharacteristicCacheEntry {
        GattCharacteristic characteristic = nullptr;
    };
    struct ServiceCacheEntry {
        GattDeviceService service = nullptr;
        std::map<long, CharacteristicCacheEntry> characteristics;
    };
    struct DeviceCacheEntry {
        BluetoothLEDevice device = nullptr;
        std::map<long, ServiceCacheEntry> services;
    };
    std::map<long, DeviceCacheEntry> cache;

};

BLEWinRTContext::BLEWinRTContext()
	: pImpl(new Impl())
	{

	}
BLEWinRTContext::~BLEWinRTContext() { Quit(); }

union to_guid
{
	uint8_t buf[16];
	guid guid;
};

const uint8_t BYTE_ORDER[] = { 3, 2, 1, 0, 5, 4, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15 };

guid make_guid(const wchar_t* value)
{
	to_guid to_guid;
	memset(&to_guid, 0, sizeof(to_guid));
	int offset = 0;
	for (int i = 0; i < wcslen(value); i++) {
		if (value[i] >= '0' && value[i] <= '9')
		{
			uint8_t digit = value[i] - '0';
			to_guid.buf[BYTE_ORDER[offset / 2]] += offset % 2 == 0 ? digit << 4 : digit;
			offset++;
		}
		else if (value[i] >= 'A' && value[i] <= 'F')
		{
			uint8_t digit = 10 + value[i] - 'A';
			to_guid.buf[BYTE_ORDER[offset / 2]] += offset % 2 == 0 ? digit << 4 : digit;
			offset++;
		}
		else if (value[i] >= 'a' && value[i] <= 'f')
		{
			uint8_t digit = 10 + value[i] - 'a';
			to_guid.buf[BYTE_ORDER[offset / 2]] += offset % 2 == 0 ? digit << 4 : digit;
			offset++;
		}
		else
		{
			// skip char
		}
	}

	return to_guid.guid;
}

long BLEWinRTContext::hsh(wchar_t* wstr) {
	long hash = 5381;
	int c;
	while (c = *wstr++)
		hash = ((hash << 5) + hash) + c;
	return hash;
}

void BLEWinRTContext::clearError() {
	lock_guard error_lock(pImpl->errorLock);
	wcscpy_s(pImpl->last_error, L"Ok");
}

void BLEWinRTContext::saveError(const wchar_t* message, ...) {
	lock_guard error_lock(pImpl->errorLock);
	va_list args;
	va_start(args, message);
	vswprintf_s(pImpl->last_error, message, args);
	va_end(args);
	wcout << pImpl->last_error << endl;
}

IAsyncOperation<BluetoothLEDevice> BLEWinRTContext::retrieveDevice(wchar_t* deviceId) {
	if (pImpl->cache.count(hsh(deviceId)))
		co_return pImpl->cache[hsh(deviceId)].device;
	// !!!! BluetoothLEDevice.FromIdAsync may prompt for consent, in this case bluetooth will fail in unity!
	BluetoothLEDevice result = co_await BluetoothLEDevice::FromIdAsync(deviceId);
	if (result == nullptr) {
		saveError(L"%s:%d Failed to connect to device.", __WFILE__, __LINE__);
		co_return nullptr;
	}
	else {
		clearError();
		pImpl->cache[hsh(deviceId)] = { result };
		co_return pImpl->cache[hsh(deviceId)].device;
	}
}
IAsyncOperation<GattDeviceService> BLEWinRTContext::retrieveService(wchar_t* deviceId, wchar_t* serviceId) {
	auto device = co_await retrieveDevice(deviceId);
	if (device == nullptr) {
		saveError(L"%s:%d Failed to retrieve device.", __WFILE__, __LINE__);
		co_return nullptr;
	}
	if (pImpl->cache[hsh(deviceId)].services.count(hsh(serviceId)))
		co_return pImpl->cache[hsh(deviceId)].services[hsh(serviceId)].service;
	GattDeviceServicesResult result = co_await device.GetGattServicesForUuidAsync(make_guid(serviceId), BluetoothCacheMode::Cached);
	if (result.Status() != GattCommunicationStatus::Success) {
		saveError(L"%s:%d Failed retrieving services with error: %d", __WFILE__, __LINE__, result.Status());
		co_return nullptr;
	}
	else if (result.Services().Size() == 0) {
		saveError(L"%s:%d No service found with uuid ", __WFILE__, __LINE__);
		co_return nullptr;
	}
	else {
		clearError();
		pImpl->cache[hsh(deviceId)].services[hsh(serviceId)] = { result.Services().GetAt(0) };
		co_return pImpl->cache[hsh(deviceId)].services[hsh(serviceId)].service;
	}
}
IAsyncOperation<GattCharacteristic> BLEWinRTContext::retrieveCharacteristic(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId) {
	auto service = co_await retrieveService(deviceId, serviceId);
	if (service == nullptr) {
		saveError(L"%s:%d Failed to retrieve service.", __WFILE__, __LINE__);
		co_return nullptr;
	}
	if (pImpl->cache[hsh(deviceId)].services[hsh(serviceId)].characteristics.count(hsh(characteristicId)))
		co_return pImpl->cache[hsh(deviceId)].services[hsh(serviceId)].characteristics[hsh(characteristicId)].characteristic;
	GattCharacteristicsResult result = co_await service.GetCharacteristicsForUuidAsync(make_guid(characteristicId), BluetoothCacheMode::Cached);
	if (result.Status() != GattCommunicationStatus::Success) {
		saveError(L"%s:%d Error scanning characteristics from service %s with status %d", __WFILE__, __LINE__, serviceId, result.Status());
		co_return nullptr;
	}
	else if (result.Characteristics().Size() == 0) {
		saveError(L"%s:%d No characteristic found with uuid %s", __WFILE__, __LINE__, characteristicId);
		co_return nullptr;
	}
	else {
		clearError();
		pImpl->cache[hsh(deviceId)].services[hsh(serviceId)].characteristics[hsh(characteristicId)] = { result.Characteristics().GetAt(0) };
		co_return pImpl->cache[hsh(deviceId)].services[hsh(serviceId)].characteristics[hsh(characteristicId)].characteristic;
	}
}

bool BLEWinRTContext::QuittableWait(condition_variable& signal, unique_lock<mutex>& waitLock) {
	{
		lock_guard quit_lock(pImpl->quitLock);
		if (pImpl->quitFlag)
			return true;
	}
	signal.wait(waitLock);
	lock_guard quit_lock(pImpl->quitLock);
	return pImpl->quitFlag;
}
bool BLEWinRTContext::QuittableWaitPredicate(condition_variable& signal, unique_lock<mutex>& waitLock, bool& predicate){
	{
		lock_guard quit_lock(pImpl->quitLock);
		if (pImpl->quitFlag)
			return true;
	}
	signal.wait(waitLock, [&predicate, this](){
		lock_guard quit_lock(pImpl->quitLock);
		return pImpl->quitFlag || predicate;
	});
	lock_guard quit_lock(pImpl->quitLock);
	return pImpl->quitFlag;
}
void BLEWinRTContext::DeviceWatcher_Added(DeviceWatcher sender, DeviceInformation deviceInfo) {
	DeviceUpdate deviceUpdate;
	wcscpy_s(deviceUpdate.id, sizeof(deviceUpdate.id) / sizeof(wchar_t), deviceInfo.Id().c_str());
	wcscpy_s(deviceUpdate.name, sizeof(deviceUpdate.name) / sizeof(wchar_t), deviceInfo.Name().c_str());
	deviceUpdate.nameUpdated = true;
	if (deviceInfo.Properties().HasKey(L"System.Devices.Aep.Bluetooth.Le.IsConnectable")) {
		deviceUpdate.isConnectable = unbox_value<bool>(deviceInfo.Properties().Lookup(L"System.Devices.Aep.Bluetooth.Le.IsConnectable"));
		deviceUpdate.isConnectableUpdated = true;
	}
	{
		lock_guard lock(pImpl->quitLock);
		if (pImpl->quitFlag)
			return;
	}
	{
		lock_guard queueGuard(pImpl->deviceQueueLock);
		pImpl->deviceQueue.push(deviceUpdate);
		pImpl->deviceQueueSignal.notify_one();
	}
}
void BLEWinRTContext::DeviceWatcher_Updated(DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
	DeviceUpdate deviceUpdate;
	wcscpy_s(deviceUpdate.id, sizeof(deviceUpdate.id) / sizeof(wchar_t), deviceInfoUpdate.Id().c_str());
	if (deviceInfoUpdate.Properties().HasKey(L"System.Devices.Aep.Bluetooth.Le.IsConnectable")) {
		deviceUpdate.isConnectable = unbox_value<bool>(deviceInfoUpdate.Properties().Lookup(L"System.Devices.Aep.Bluetooth.Le.IsConnectable"));
		deviceUpdate.isConnectableUpdated = true;
	}
	{
		lock_guard lock(pImpl->quitLock);
		if (pImpl->quitFlag)
			return;
	}
	{
		lock_guard queueGuard(pImpl->deviceQueueLock);
		pImpl->deviceQueue.push(deviceUpdate);
		pImpl->deviceQueueSignal.notify_one();
	}
}
void BLEWinRTContext::DeviceWatcher_EnumerationCompleted(DeviceWatcher sender, IInspectable const&) {
	StopDeviceScan();
}

void BLEWinRTContext::StartDeviceScan() {
	// as this is the first function that must be called, if Quit() was called before, assume here that the client wants to restart
	{
		lock_guard lock(pImpl->quitLock);
		pImpl->quitFlag = false;
		clearError();
	}

	IVector<hstring> requestedProperties = single_threaded_vector<hstring>({ L"System.Devices.Aep.DeviceAddress", L"System.Devices.Aep.IsConnected", L"System.Devices.Aep.Bluetooth.Le.IsConnectable" });
	hstring aqsAllBluetoothLEDevices = L"(System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\")"; // list Bluetooth LE devices
	pImpl->deviceWatcher = DeviceInformation::CreateWatcher(
		aqsAllBluetoothLEDevices,
		requestedProperties,
		DeviceInformationKind::AssociationEndpoint);

	// see https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/handle-events#revoke-a-registered-delegate
	//deviceWatcherAddedRevoker = deviceWatcher.Added(auto_revoke, &DeviceWatcher_Added);
	//deviceWatcherUpdatedRevoker = deviceWatcher.Updated(auto_revoke, &DeviceWatcher_Updated);
	//deviceWatcherCompletedRevoker = deviceWatcher.EnumerationCompleted(auto_revoke, &DeviceWatcher_EnumerationCompleted);
	pImpl->deviceWatcherAddedRevoker = pImpl->deviceWatcher.Added(
        auto_revoke,
        [this](DeviceWatcher sender, DeviceInformation deviceInfo) {
            this->DeviceWatcher_Added(sender, deviceInfo);
        }
    );
    pImpl->deviceWatcherUpdatedRevoker = pImpl->deviceWatcher.Updated(
        auto_revoke,
        [this](DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
            this->DeviceWatcher_Updated(sender, deviceInfoUpdate);
        }
    );
    pImpl->deviceWatcherCompletedRevoker = pImpl->deviceWatcher.EnumerationCompleted(
        auto_revoke,
        [this](DeviceWatcher sender, IInspectable const& args) {
            this->DeviceWatcher_EnumerationCompleted(sender, args);
        }
    );
    // ~30 seconds scan ; for permanent scanning use BluetoothLEAdvertisementWatcher, see the BluetoothAdvertisement.zip sample
	pImpl->deviceScanFinished = false;
	pImpl->deviceWatcher.Start();
}

ScanStatus BLEWinRTContext::PollDevice(DeviceUpdate* device, bool block) {
	ScanStatus res;
	unique_lock<mutex> lock(pImpl->deviceQueueLock);
	if (block && pImpl->deviceQueue.empty() && !pImpl->deviceScanFinished)
		if (QuittableWait(pImpl->deviceQueueSignal, lock))
			return ScanStatus::FINISHED;
	if (!pImpl->deviceQueue.empty()) {
		*device = pImpl->deviceQueue.front();
		pImpl->deviceQueue.pop();
		res = ScanStatus::AVAILABLE;
	}
	else if (pImpl->deviceScanFinished)
		res = ScanStatus::FINISHED;
	else
		res = ScanStatus::PROCESSING;
	return res;
}

void BLEWinRTContext::StopDeviceScan() {
	lock_guard lock(pImpl->deviceQueueLock);
	if (pImpl->deviceWatcher != nullptr) {
		pImpl->deviceWatcherAddedRevoker.revoke();
		pImpl->deviceWatcherUpdatedRevoker.revoke();
		pImpl->deviceWatcherCompletedRevoker.revoke();
		pImpl->deviceWatcher.Stop();
		pImpl->deviceWatcher = nullptr;
	}
	pImpl->deviceScanFinished = true;
	pImpl->deviceQueueSignal.notify_one();
}

fire_and_forget BLEWinRTContext::ScanServicesAsync(wchar_t* deviceId) {
	{
		lock_guard queueGuard(pImpl->serviceQueueLock);
		pImpl->serviceScanFinished = false;
	}
	try {
		auto bluetoothLeDevice = co_await retrieveDevice(deviceId);
		if (bluetoothLeDevice != nullptr) {
			GattDeviceServicesResult result = co_await bluetoothLeDevice.GetGattServicesAsync(BluetoothCacheMode::Uncached);
			if (result.Status() == GattCommunicationStatus::Success) {
				IVectorView<GattDeviceService> services = result.Services();
				for (auto&& service : services)
				{
					Service serviceStruct;
					wcscpy_s(serviceStruct.uuid, sizeof(serviceStruct.uuid) / sizeof(wchar_t), to_hstring(service.Uuid()).c_str());
					{
						lock_guard lock(pImpl->quitLock);
						if (pImpl->quitFlag)
							break;
					}
					{
						lock_guard queueGuard(pImpl->serviceQueueLock);
						pImpl->serviceQueue.push(serviceStruct);
						pImpl->serviceQueueSignal.notify_one();
					}
				}
			}
			else {
				saveError(L"%s:%d Failed retrieving services with error: %d", __WFILE__, __LINE__, (int)result.Status());
			}
		}
	}
	catch (hresult_error& ex)
	{
		saveError(L"%s:%d ScanServicesAsync catch: %s", __WFILE__, __LINE__, ex.message().c_str());
	}
	{
		lock_guard queueGuard(pImpl->serviceQueueLock);
		pImpl->serviceScanFinished = true;
		pImpl->serviceQueueSignal.notify_one();
	}
}
void BLEWinRTContext::ScanServices(wchar_t* deviceId) {
	ScanServicesAsync(deviceId);
}

ScanStatus BLEWinRTContext::PollService(Service* service, bool block) {
	ScanStatus res;
	unique_lock<mutex> lock(pImpl->serviceQueueLock);
	if (block && pImpl->serviceQueue.empty() && !pImpl->serviceScanFinished)
		if (QuittableWait(pImpl->serviceQueueSignal, lock))
			return ScanStatus::FINISHED;
	if (!pImpl->serviceQueue.empty()) {
		*service = pImpl->serviceQueue.front();
		pImpl->serviceQueue.pop();
		res = ScanStatus::AVAILABLE;
	}
	else if (pImpl->serviceScanFinished)
		res = ScanStatus::FINISHED;
	else
		res = ScanStatus::PROCESSING;
	return res;
}

fire_and_forget BLEWinRTContext::ScanCharacteristicsAsync(wchar_t* deviceId, wchar_t* serviceId) {
	{
		lock_guard lock(pImpl->characteristicQueueLock);
		pImpl->characteristicScanFinished = false;
	}
	try {
		auto service = co_await retrieveService(deviceId, serviceId);
		if (service != nullptr) {
			GattCharacteristicsResult charScan = co_await service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
			if (charScan.Status() != GattCommunicationStatus::Success)
				saveError(L"%s:%d Error scanning characteristics from service %s with status %d", __WFILE__, __LINE__, serviceId, (int)charScan.Status());
			else {
				for (auto c : charScan.Characteristics())
				{
					Characteristic charStruct;
					wcscpy_s(charStruct.uuid, sizeof(charStruct.uuid) / sizeof(wchar_t), to_hstring(c.Uuid()).c_str());
					// retrieve user description
					GattDescriptorsResult descriptorScan = co_await c.GetDescriptorsForUuidAsync(make_guid(L"00002901-0000-1000-8000-00805F9B34FB"), BluetoothCacheMode::Uncached);
					if (descriptorScan.Descriptors().Size() == 0) {
						const wchar_t* defaultDescription = L"no description available";
						wcscpy_s(charStruct.userDescription, sizeof(charStruct.userDescription) / sizeof(wchar_t), defaultDescription);
					}
					else {
						GattDescriptor descriptor = descriptorScan.Descriptors().GetAt(0);
						auto nameResult = co_await descriptor.ReadValueAsync();
						if (nameResult.Status() != GattCommunicationStatus::Success)
							saveError(L"%s:%d couldn't read user description for charasteristic %s, status %d", __WFILE__, __LINE__, to_hstring(c.Uuid()).c_str(), nameResult.Status());
						else {
							auto dataReader = DataReader::FromBuffer(nameResult.Value());
							auto output = dataReader.ReadString(dataReader.UnconsumedBufferLength());
							wcscpy_s(charStruct.userDescription, sizeof(charStruct.userDescription) / sizeof(wchar_t), output.c_str());
							clearError();
						}
					}
					{
						lock_guard lock(pImpl->quitLock);
						if (pImpl->quitFlag)
							break;
					}
					{
						lock_guard queueGuard(pImpl->characteristicQueueLock);
						pImpl->characteristicQueue.push(charStruct);
						pImpl->characteristicQueueSignal.notify_one();
					}
				}
			}
		}
	}
	catch (hresult_error& ex)
	{
		saveError(L"%s:%d ScanCharacteristicsAsync catch: %s", __WFILE__, __LINE__, ex.message().c_str());
	}
	{
		lock_guard lock(pImpl->characteristicQueueLock);
		pImpl->characteristicScanFinished = true;
		pImpl->characteristicQueueSignal.notify_one();
	}
}

void BLEWinRTContext::ScanCharacteristics(wchar_t* deviceId, wchar_t* serviceId) {
	ScanCharacteristicsAsync(deviceId, serviceId);
}

ScanStatus BLEWinRTContext::PollCharacteristic(Characteristic* characteristic, bool block) {
	ScanStatus res;
	unique_lock<mutex> lock(pImpl->characteristicQueueLock);
	if (block && pImpl->characteristicQueue.empty() && !pImpl->characteristicScanFinished)
		if (QuittableWait(pImpl->characteristicQueueSignal, lock))
			return ScanStatus::FINISHED;
	if (!pImpl->characteristicQueue.empty()) {
		*characteristic = pImpl->characteristicQueue.front();
		pImpl->characteristicQueue.pop();
		res = ScanStatus::AVAILABLE;
	}
	else if (pImpl->characteristicScanFinished)
		res = ScanStatus::FINISHED;
	else
		res = ScanStatus::PROCESSING;
	return res;
}

void BLEWinRTContext::Characteristic_NewValue(GattCharacteristic const& characteristic, IBuffer buffer) {
	
	BLEData data;
	wcscpy_s(data.characteristicUuid, sizeof(data.characteristicUuid) / sizeof(wchar_t), to_hstring(characteristic.Uuid()).c_str());
	wcscpy_s(data.serviceUuid, sizeof(data.serviceUuid) / sizeof(wchar_t), to_hstring(characteristic.Service().Uuid()).c_str());
	wcscpy_s(data.deviceId, sizeof(data.deviceId) / sizeof(wchar_t), characteristic.Service().Device().DeviceId().c_str());

	data.size = buffer.Length();
	// IBuffer to array, copied from https://stackoverflow.com/a/55974934
	memcpy(data.buf, buffer.data(), data.size);

	{
		lock_guard lock(pImpl->quitLock);
		if (pImpl->quitFlag)
			return;
	}
	{
		lock_guard queueGuard(pImpl->dataQueueLock);
		pImpl->dataQueue.push(data);
		pImpl->dataQueueSignal.notify_one();
	}
}
void BLEWinRTContext::Characteristic_ValueChanged(GattCharacteristic const& characteristic, GattValueChangedEventArgs args)
{
	this->Characteristic_NewValue(characteristic, args.CharacteristicValue());
}

fire_and_forget BLEWinRTContext::SubscribeCharacteristicAsync(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, bool* result) {
	try {
		auto characteristic = co_await retrieveCharacteristic(deviceId, serviceId, characteristicId);
		if (characteristic != nullptr) {
			auto status = co_await characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify);
			if (status != GattCommunicationStatus::Success)
				saveError(L"%s:%d Error subscribing to characteristic with uuid %s and status %d", __WFILE__, __LINE__, characteristicId, status);
			else {
				Impl::Subscription *subscription = new Impl::Subscription();
				subscription->characteristic = characteristic;
				//subscription->revoker = characteristic.ValueChanged(auto_revoke, &Characteristic_ValueChanged);
				subscription->revoker = characteristic.ValueChanged(
                    auto_revoke,
                    [this](GattCharacteristic const& characteristic, GattValueChangedEventArgs args) {
                        this->Characteristic_ValueChanged(characteristic, args);
                    }
                );
                pImpl->subscriptions.push_back(subscription);
				if (result != 0)
					*result = true;
			}
		}
	}
	catch (hresult_error& ex)
	{
		saveError(L"%s:%d SubscribeCharacteristicAsync catch: %s", __WFILE__, __LINE__, ex.message().c_str());
	}
	pImpl->subscribeFinished = true;
	pImpl->subscribeQueueSignal.notify_one();
}
bool BLEWinRTContext::SubscribeCharacteristic(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, bool block) {
	unique_lock<mutex> lock(pImpl->subscribeQueueLock);
	bool result = false;
	pImpl->subscribeFinished = false;
	SubscribeCharacteristicAsync(deviceId, serviceId, characteristicId, block ? &result : 0);
	if (block && QuittableWaitPredicate(pImpl->subscribeQueueSignal, lock, pImpl->subscribeFinished))
		return false;

	return result;
}

fire_and_forget BLEWinRTContext::UnsubscribeCharacteristicAsync(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, bool* result) {
	try {
		auto characteristic = co_await retrieveCharacteristic(deviceId, serviceId, characteristicId);
		if (characteristic != nullptr) {
			auto it = pImpl->subscriptions.begin();
			while (it != pImpl->subscriptions.end()) {
				if ((*it)->characteristic == characteristic) {
					(*it)->revoker.revoke();
					delete (*it);
					it = pImpl->subscriptions.erase(it);
					if (result != 0)
						*result = true;
				}
				else {
					++it;
				}
			}
		}
	}
	catch (hresult_error& ex)
	{
		saveError(L"%s:%d UnsubscribeCharacteristicAsync catch: %s", __WFILE__, __LINE__, ex.message().c_str());
	}
	pImpl->unsubscribeFinished = true;
	pImpl->unsubscribeQueueSignal.notify_one();
}
bool BLEWinRTContext::UnsubscribeCharacteristic(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, bool block) {
	unique_lock<mutex> lock(pImpl->subscribeQueueLock);
	bool result = false;
	pImpl->unsubscribeFinished = false;
	UnsubscribeCharacteristicAsync(deviceId, serviceId, characteristicId, block ? &result : 0);
	if (block && QuittableWaitPredicate(pImpl->unsubscribeQueueSignal, lock, pImpl->unsubscribeFinished))
		return false;

	return result;
}

fire_and_forget BLEWinRTContext::TriggerCharacteristicReadAsync(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, bool* result) {
	try {
		auto characteristic = co_await retrieveCharacteristic(deviceId, serviceId, characteristicId);
		if (characteristic != nullptr) {
			auto status = co_await characteristic.ReadValueAsync(BluetoothCacheMode::Uncached);
			if (status.Status() != GattCommunicationStatus::Success) {
				saveError(L"%s:%d Error reading characteristic with uuid %s and status %d", __WFILE__, __LINE__, characteristicId, status.Status());
			}
			else {
				this->Characteristic_NewValue(characteristic, status.Value());
				if (result != 0)
					*result = true;
			}
		}
	}
	catch (hresult_error& ex)
	{
		saveError(L"%s:%d TriggerCharacteristicReadAsync catch: %s", __WFILE__, __LINE__, ex.message().c_str());
	}
	pImpl->readSignal.notify_one();
}
bool BLEWinRTContext::TriggerCharacteristicRead(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, bool block) {
	unique_lock<mutex> lock(pImpl->readLock);
	bool result = false;
	// copy data to stack so that caller can free its memory in non-blocking mode
	TriggerCharacteristicReadAsync(deviceId, serviceId, characteristicId, block ? &result : 0);
	if (block)
		pImpl->readSignal.wait(lock);

	return result;
}

fire_and_forget BLEWinRTContext::ReadCharacteristicAsync(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, BLEData* data, std::condition_variable* signal, bool* result) {
	try {
		auto characteristic = co_await retrieveCharacteristic(deviceId, serviceId, characteristicId);
		if (characteristic != nullptr) {
			auto status = co_await characteristic.ReadValueAsync(BluetoothCacheMode::Uncached);
			if (status.Status() != GattCommunicationStatus::Success) {
				saveError(L"%s:%d Error reading characteristic with uuid %s and status %d", __WFILE__, __LINE__, characteristicId, status.Status());
			}
			else {
				// IBuffer to array, copied from https://stackoverflow.com/a/55974934
				data->size = status.Value().Length();
				memcpy(data->buf, status.Value().data(), data->size);
				wcscpy_s(data->characteristicUuid, sizeof(data->characteristicUuid) / sizeof(wchar_t), characteristicId);
				wcscpy_s(data->serviceUuid, sizeof(data->serviceUuid) / sizeof(wchar_t), serviceId);
				wcscpy_s(data->deviceId, sizeof(data->deviceId) / sizeof(wchar_t), deviceId);
				if (result != 0)
					*result = true;
			}
		}
	}
	catch (hresult_error& ex)
	{
		saveError(L"%s:%d ReadCharacteristic catch: %s", __WFILE__, __LINE__, ex.message().c_str());
	}
	if (signal != 0)
		signal->notify_one();
}
bool BLEWinRTContext::ReadCharacteristic(wchar_t* deviceId, wchar_t* serviceId, wchar_t* characteristicId, BLEData* data, bool block) {
	mutex _mutex;
	unique_lock<mutex> lock(_mutex);
	condition_variable signal;
	bool result = false;
	// copy data to stack so that caller can free its memory in non-blocking mode
	ReadCharacteristicAsync(deviceId, serviceId, characteristicId, data, block ? &signal : 0, block ? &result : 0);
	if (block)
		signal.wait(lock);
	return result;
}

bool BLEWinRTContext::PollData(BLEData* data, bool block) {
	unique_lock<mutex> lock(pImpl->dataQueueLock);
	if (block && pImpl->dataQueue.empty())
		if (QuittableWait(pImpl->dataQueueSignal, lock))
			return false;
	if (!pImpl->dataQueue.empty()) {
		*data = pImpl->dataQueue.front();
		pImpl->dataQueue.pop();
		return true;
	}
	return false;
}

fire_and_forget BLEWinRTContext::SendDataAsync(BLEData data, std::condition_variable* signal, bool* result) {
	try {
		auto characteristic = co_await retrieveCharacteristic(data.deviceId, data.serviceUuid, data.characteristicUuid);
		if (characteristic != nullptr) {
			// create IBuffer from data
			DataWriter writer;
			writer.WriteBytes(array_view<uint8_t const> (data.buf, data.buf + data.size));
			IBuffer buffer = writer.DetachBuffer();
			auto status = co_await characteristic.WriteValueAsync(buffer, GattWriteOption::WriteWithoutResponse);
			if (status != GattCommunicationStatus::Success)
				saveError(L"%s:%d Error writing value to characteristic with uuid %s", __WFILE__, __LINE__, data.characteristicUuid);
			else if (result != 0)
				*result = true;
		}
	}
	catch (hresult_error& ex)
	{
		saveError(L"%s:%d SendDataAsync catch: %s", __WFILE__, __LINE__, ex.message().c_str());
	}
	if (signal != 0)
		signal->notify_one();
}

bool BLEWinRTContext::SendData(BLEData* data, bool block) {
	mutex _mutex;
	unique_lock<mutex> lock(_mutex);
	condition_variable signal;
	bool result = false;
	// copy data to stack so that caller can free its memory in non-blocking mode
	SendDataAsync(*data, block ? &signal : 0, block ? &result : 0);
	if (block)
		signal.wait(lock);

	return result;
}

void BLEWinRTContext::Quit() {
	{
		lock_guard lock(pImpl->quitLock);
		pImpl->quitFlag = true;
	}
	StopDeviceScan();
	pImpl->deviceQueueSignal.notify_one();
	{
		lock_guard lock(pImpl->deviceQueueLock);
		pImpl->deviceQueue = {};
	}
	pImpl->serviceQueueSignal.notify_one();
	{
		lock_guard lock(pImpl->serviceQueueLock);
		pImpl->serviceQueue = {};
	}
	pImpl->characteristicQueueSignal.notify_one();
	{
		lock_guard lock(pImpl->characteristicQueueLock);
		pImpl->characteristicQueue = {};
	}
	pImpl->subscribeQueueSignal.notify_one();
	pImpl->unsubscribeQueueSignal.notify_one();
	{
		lock_guard lock(pImpl->subscribeQueueLock);
		for (auto subscription : pImpl->subscriptions)
			subscription->revoker.revoke();
		pImpl->subscriptions = {};
	}
	pImpl->dataQueueSignal.notify_one();
	{
		lock_guard lock(pImpl->dataQueueLock);
		pImpl->dataQueue = {};
	}
    ClearCache();
}

void BLEWinRTContext::GetError(ErrorMessage* buf) {
	lock_guard error_lock(pImpl->errorLock);
	wcscpy_s(buf->msg, pImpl->last_error);
}

void BLEWinRTContext::ClearCache(bool onlyServices) {	
	for (auto device : pImpl->cache) {
		if (!onlyServices)
			device.second.device.Close();
		for (auto service : device.second.services) {
			service.second.service.Close();
			service.second.service = nullptr;
			for (auto characteristic : service.second.characteristics) {
				characteristic.second.characteristic = nullptr;
			}
			service.second.characteristics.clear();
		}
		device.second.services.clear();
	}
	pImpl->cache.clear();
}
