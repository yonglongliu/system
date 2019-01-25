/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _TETHER_CONTROLLER_H
#define _TETHER_CONTROLLER_H

#include <netinet/in.h>
#include <set>
#include <string>

#include "List.h"

#define NETD_SUPPORT_USB_IPV6_TETHERING

typedef android::netd::List<char *> InterfaceCollection;
typedef android::netd::List<struct in_addr> NetAddressCollection;

class TetherController {
    InterfaceCollection  *mInterfaces;
    // NetId to use for forwarded DNS queries. This may not be the default
    // network, e.g., in the case where we are tethering to a DUN APN.
    unsigned              mDnsNetId;
    NetAddressCollection *mDnsForwarders;
    pid_t                 mDaemonPid;
    int                   mDaemonFd;
    std::set<std::string> mForwardingRequests;

#ifdef NETD_SUPPORT_USB_IPV6_TETHERING
    int                   mRadvdPid;
    bool                 mRadvdStarted;
    char                 mV6Interface[32];
    char                 mV6TetheredInterface[32];
    char                mV6networkaddr[64];
    //bool                mV6InterfaceChanged;
    int                   mDhcp6sPid;
#endif

public:
    TetherController();
    virtual ~TetherController();

    bool enableForwarding(const char* requester);
    bool disableForwarding(const char* requester);
    size_t forwardingRequestCount();

    int startTethering(int num_addrs, struct in_addr* addrs);
    int stopTethering();
    bool isTetheringStarted();

    unsigned getDnsNetId();
    int setDnsForwarders(unsigned netId, char **servers, int numServers);
    NetAddressCollection *getDnsForwarders();

    int tetherInterface(const char *interface);
    int untetherInterface(const char *interface);
    InterfaceCollection *getTetheredInterfaceList();

#ifdef NETD_SUPPORT_USB_IPV6_TETHERING
    int addV6RadvdIface(const char *iface);
    int rmV6RadvdIface(const char *iface);

    int startRadvd(char *up_interface, bool idle_check);
    int stopRadvd(void);
#endif

private:
    int applyDnsInterfaces();
    bool setIpFwdEnabled();
#ifdef NETD_SUPPORT_USB_IPV6_TETHERING
    int startDhcp6s(char *dns1, char *dns2);
    int stopDhcp6s(void);
    int applyIpV6Rule(void);
    int clearIpV6Rule(void);
#endif
};

#endif
