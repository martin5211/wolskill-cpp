#pragma once
#include <string>
#include <vector>
#include <map>

struct AdapterInfo {
    std::string mac;
    std::string ipv4;
    std::string ipv6;
};

// Returns map of adapter name -> info, similar to Node macaddress.all()
std::map<std::string, AdapterInfo> GetAllAdapters();

// Returns JSON string matching the Node.js macaddress.all() output format
std::string GetAdaptersJson();

// Returns all local MAC addresses in XX-XX-XX-XX-XX-XX format (uppercase)
std::vector<std::string> GetLocalMacAddresses();
