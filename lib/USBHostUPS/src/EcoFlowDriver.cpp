#include "EcoFlowDriver.h"
#include "IUSBHostUPS.h"
#include "HIDParser.h"
#include "HIDUsages.h"

/**
 * @brief EcoFlow Driver Implementation
 *
 * ADR 0003 COMPLIANCE:
 * This sub-driver mirrors the official NUT behavior for EcoFlow USB HID devices.
 * - Reference: nut/drivers/ecoflow-hid.c (subdriver ecoflow_subdriver)
 * - Vendor ID: 0x3746 (device table ecoflow_usb_device_table)
 * - RunTimeToEmpty is reported in minutes and converted to seconds (x60), exactly
 *   like ecoflow_battery_runtime_conversion() in the upstream subdriver.
 *
 * Standard USB HID PDC fields are decoded by GenericDriver (ADR 0004); this
 * sub-driver only adds the EcoFlow-specific usages.
 */

EcoFlowDriver::EcoFlowDriver() : _chemStrIdx(0) {}

void EcoFlowDriver::setup() {
    GenericDriver::setup();
    _map.invalidate();
    _chemStrIdx = 0;
}

void EcoFlowDriver::collectStringRequests(IUSBHostUPS* host, const UPSData& data, std::vector<uint8_t>& out) const {
    GenericDriver::collectStringRequests(host, data, out);
    // ecoflow-hid.c: UPS.PowerSummary.iDeviceChemistry via stringid_conversion
    if (_chemStrIdx > 0 && !data.hasKey("battery.type")) out.push_back(_chemStrIdx);
}

void EcoFlowDriver::decodeReport(IUSBHostUPS* host, uint8_t report_id, uint8_t report_type, const uint8_t *data, size_t length, UPSData& ups_data) {
    if (length == 0 || data == NULL || !host) return;

    GenericDriver::decodeReport(host, report_id, report_type, data, length, ups_data);

    typedef UsageMapIndex<EcoFlowDriver>::Mapping Mapping;
    static const Mapping mappings[] = {
        // HIDParser collapses NUT's indexed collection paths, so the upstream
        // "UPS.Flow.[4].ConfigActivePower" is matched as "UPS.Flow.ConfigActivePower".
        // Upstream maps this active-power (W) usage to ups.power.nominal, whose generic
        // producer is ConfigApparentPower (VA); keep the upstream mapping (ADR 0003).
        { "UPS.Flow.ConfigActivePower", [](EcoFlowDriver*, UPSData& d, double v, const HIDUsageDef*) { d.set("ups.power.nominal", String((int)v)); } },
        // PowerSummary.DesignCapacity/FullChargeCapacity are percentages on these
        // devices (ecoflow-hid.c comments say "unit %"), unlike the Ah-based
        // BatterySystem usages handled by GenericDriver.
        { "UPS.PowerSummary.DesignCapacity", [](EcoFlowDriver*, UPSData& d, double v, const HIDUsageDef*) { d.set("battery.capacity.nominal", String((int)v)); } },
        { "UPS.PowerSummary.FullChargeCapacity", [](EcoFlowDriver*, UPSData& d, double v, const HIDUsageDef*) { d.set("battery.capacity", String((int)v)); } },
        { "UPS.PowerSummary.WarningCapacityLimit", [](EcoFlowDriver*, UPSData& d, double v, const HIDUsageDef*) { d.set("battery.charge.warning", String((int)v)); } },
        // ecoflow-hid.c leaves RemainingTimeLimit in minutes; NUT defines
        // battery.runtime.low in seconds, so normalize it like battery.runtime
        // (nut-compliance: time values are exposed in seconds).
        { "UPS.PowerSummary.RemainingTimeLimit", [](EcoFlowDriver*, UPSData& d, double v, const HIDUsageDef*) {
            if (v < 0) return;
            d.set("battery.runtime.low", String((long)(v * 60.0)));
        } },
        // ecoflow-hid.c: ecoflow_battery_runtime_conversion(), minutes -> seconds.
        // On a negative value upstream returns NULL (no value); GenericDriver has
        // already written the raw value, so drop it instead of leaving it behind.
        { "UPS.PowerSummary.RunTimeToEmpty", [](EcoFlowDriver*, UPSData& d, double v, const HIDUsageDef*) {
            if (v < 0) { d.remove("battery.runtime"); return; }
            d.set("battery.runtime", String((long)(v * 60.0)));
        } },
        { "UPS.PowerSummary.iDeviceChemistry", [](EcoFlowDriver* drv, UPSData&, double v, const HIDUsageDef*) { drv->_chemStrIdx = (uint8_t)v; } },
        { "UPS.OutletSystem.Outlet.DelayBeforeReboot", [](EcoFlowDriver*, UPSData& d, double v, const HIDUsageDef*) { d.set("ups.timer.reboot", String((int)v)); } },
        { "UPS.OutletSystem.Outlet.DelayBeforeShutdown", [](EcoFlowDriver*, UPSData& d, double v, const HIDUsageDef*) { d.set("ups.timer.shutdown", String((int)v)); } }
    };

    _map.apply(this, mappings, host->getUsages(), report_id, report_type, data, length, ups_data);
}

void EcoFlowDriver::parseStringDescriptor(IUSBHostUPS* host, uint8_t index, const uint8_t *data, size_t length, UPSData& ups_data) {
    GenericDriver::parseStringDescriptor(host, index, data, length, ups_data);

    if (length < 2 || data[1] != 0x03) return;
    uint8_t str_len = data[0];
    String str = "";
    for (int i = 2; i < str_len && i < length; i += 2) {
        if (data[i] != 0) {
            str += (char)data[i];
        }
    }

    if (_chemStrIdx > 0 && index == _chemStrIdx) { ups_data.set("battery.type", str); }
}
