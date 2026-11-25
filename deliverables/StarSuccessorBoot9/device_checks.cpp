#include "boot_devices.h"
#include <cstdio>
#include <stdexcept>
void Check(bool condition) { if (!condition) throw std::runtime_error("device check failed"); }
int main() {
    StarBoot::Devices devices;
    uint16_t value16 = 0;
    uint32_t value32 = 0;
    Check(devices.Write32(0xCC003004u, 0xF0u));
    Check(devices.Read32(0xCC003004u, value32) && value32 == 0xF0u);
    Check(devices.Write32(0xCD000034u, 0x40000000u));
    Check(devices.Read32(0xCD000034u, value32) && value32 == 0x40000000u);
    Check(devices.Write16(0xCC00401Cu, 0xFFFFu));
    Check(devices.Read16(0xCC00401Cu, value16) && value16 == 0x1Fu);
    Check(devices.Read16(0xCC00500Au, value16) && value16 == 4u);
    Check(devices.Write16(0xCC00500Au, 0x01FCu));
    Check(devices.Read16(0xCC00500Au, value16) && value16 == 0x0154u);
    Check(devices.Write32(0xCD006C00u, 0x2Cu));
    Check(devices.Read32(0xCD006C00u, value32) && value32 == 4u);
    Check(devices.Read32(0xCD006C08u, value32) && value32 == 0);
    for (uint32_t address : {0xCD006800u, 0xCD006814u, 0xCD006828u}) {
        Check(devices.Write32(address, 0x40Fu));
        Check(devices.Read32(address, value32) && value32 == 0x405u);
    }
    Check(devices.Read32(0xCD00680Cu, value32) && value32 == 0);
    Check(devices.Write32(0xCD006804u, 0x80004000u));
    Check(devices.Read32(0xCD006804u, value32) && value32 == 0x80004000u);
    // Select AD16, send init command, then read its four-byte ID.
    Check(devices.Write32(0xCD006800u, 0x200u));
    Check(devices.Write32(0xCD006810u, 0));
    Check(devices.Write32(0xCD00680Cu, 0x15u));
    Check(devices.Read32(0xCD00680Cu, value32) && !(value32 & 1u));
    Check(devices.Write32(0xCD00680Cu, 0x31u));
    Check(devices.Read32(0xCD006810u, value32) && value32 == 0x04120000u);
    Check(devices.Read32(0xCD006800u, value32) && (value32 & 8u));
    Check(devices.Write32(0xCD006800u, 0x2008u));
    Check(devices.Read32(0xCD006800u, value32) && value32 == 0x2000u);
    Check(devices.Read32(0xCD006024u, value32) && value32 == 1u);
    Check(!devices.Read32(0xCC00401Cu, value32));
    Check(!devices.Write16(0xCD006C00u, 0));
    Check(!devices.Write32(0xCD006C08u, 1));
    auto mustStop = [](auto operation) {
        try { operation(); } catch (const std::runtime_error&) { return; }
        throw std::runtime_error("unsupported execution request was accepted");
    };
    Check(devices.Write16(0xCC00500Au, 0x0805u));
    mustStop([&] { devices.Write16(0xCC00500Au, 0); });
    mustStop([&] { devices.Write32(0xCD006C00u, 1); });
    mustStop([&] { devices.Write32(0xCD00680Cu, 1); });
    std::puts("Device state checks passed.");
}
