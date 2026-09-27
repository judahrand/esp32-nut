#ifndef ECOFLOW_DRIVER_H
#define ECOFLOW_DRIVER_H

#include "GenericDriver.h"
#include <Arduino.h>

class EcoFlowDriver : public GenericDriver {
public:
    const char* getDriverName() const override { return "EcoFlowDriver"; }

    EcoFlowDriver();
    virtual ~EcoFlowDriver() = default;

    void setup() override;
    void decodeReport(IUSBHostUPS* host, uint8_t report_id, uint8_t report_type, const uint8_t *data, size_t length, UPSData& ups_data) override;
    void parseStringDescriptor(IUSBHostUPS* host, uint8_t index, const uint8_t *data, size_t length, UPSData& ups_data) override;

protected:
    void collectStringRequests(IUSBHostUPS* host, const UPSData& data, std::vector<uint8_t>& out) const override;

private:
    UsageMapIndex<EcoFlowDriver> _map;
    uint8_t _chemStrIdx;
};

#endif // ECOFLOW_DRIVER_H
