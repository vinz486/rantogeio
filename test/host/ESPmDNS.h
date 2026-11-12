#pragma once
struct HostMDNS { bool begin(const char*) { return true; } };
extern HostMDNS MDNS;
