// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include "apple_net_helper.h"

#include <Network/Network.h>  // nw_path, nw_interface
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>

// Check if interface is physical (en*)
bool is_physical_interface(const char *name) {
    return name && strncmp(name, "en", 2) == 0;
}

// On iOS, SCDynamicStore is unavailable.
// We walk ifaddrs and return the first active en* interface with an IPv4
// or IPv6 address — on a real device this is almost always en0 (Wi-Fi).
bool get_primary_interface_name(char *dest, size_t bufferSize) {
    struct ifaddrs *iflist = nullptr;
    if (getifaddrs(&iflist) != 0)
        return false;

    bool success = false;

    for (struct ifaddrs *cur = iflist; cur; cur = cur->ifa_next) {
        if (!cur->ifa_name || !cur->ifa_addr)
            continue;
        if (!(cur->ifa_flags & IFF_UP) || (cur->ifa_flags & IFF_LOOPBACK))
            continue;

        const sa_family_t family = cur->ifa_addr->sa_family;
        if (family != AF_INET && family != AF_INET6)
            continue;
        if (!is_physical_interface(cur->ifa_name))
            continue;

        if (strlcpy(dest, cur->ifa_name, bufferSize) < bufferSize) {
            success = true;
            break;
        }
    }

    freeifaddrs(iflist);
    return success;
}

// Identical logic to the macOS version — getifaddrs + sockaddr_dl works on
// iOS, but the OS zeroes the MAC bytes (returns 02:00:00:00:00:00) since iOS 7.
// Kept for API compatibility; callers should use identifierForVendor instead
// if a persistent unique ID is actually needed.
bool get_mac_address(const char *hint, uint8_t mac[6]) {
    struct ifaddrs *iflist = nullptr;
    if (getifaddrs(&iflist) != 0)
        return false;

    const char *target = is_physical_interface(hint) ? hint : nullptr;
    bool success = false;

    for (struct ifaddrs *cur = iflist; cur; cur = cur->ifa_next) {
        if (!cur->ifa_addr || !cur->ifa_name)
            continue;
        if (!(cur->ifa_flags & IFF_UP) || (cur->ifa_flags & IFF_LOOPBACK))
            continue;
        if (cur->ifa_addr->sa_family != AF_LINK)
            continue;
        if (!is_physical_interface(cur->ifa_name))
            continue;
        if (target && strcmp(cur->ifa_name, target) != 0)
            continue;

        auto sdl = reinterpret_cast<struct sockaddr_dl *>(cur->ifa_addr);
        if (sdl->sdl_alen == 6) {
            memcpy(mac, LLADDR(sdl), 6);
            success = true;
            break;
        }
    }

    freeifaddrs(iflist);
    return success;
}
