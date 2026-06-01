/*
 * Software Name: ib-ORAN
 * SPDX-FileCopyrightText: Copyright (c) Orange SA
 * SPDX-License-Identifier: MIT
 *
 * This software is distributed under the MIT license,
 * see the "LICENSE" file for more details
 * or https://spdx.org/licenses/MIT.html
 *
 * Authors: see CONTRIBUTORS.md
 */

#ifndef IBORAN_RAN_COMMON_H
#define IBORAN_RAN_COMMON_H

#include "../iboran_application/iboran-application.h"
#include "iboran-ran-params.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/lte-ue-rrc.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"

#include <filesystem>

namespace ns3
{

// == MOBILITY FUNCTIONS ==============================================
void SetupBackhaulMobility(const CommonRanParams& commonParams,
                           Ptr<Node> pgw,
                           Ptr<Node> sgw,
                           Ptr<Node> mme,
                           Ptr<Node> remoteHost);

void SetupFronthaulMobility(const CommonRanParams& commonParams,
                            const std::string& ratType,
                            NodeContainer& xnbNodes,
                            NodeContainer& ueNodes);

// == APP FUNCTIONS ===================================================
void SetupIbOranApplications(const CommonRanParams& commonParams,
                             const std::string& ratType,
                             NodeContainer& ueNodes,
                             Ptr<Node> remoteHost,
                             const Ipv4InterfaceContainer& ueIpIfaces,
                             Ipv4Address remoteHostAddr,
                             ApplicationContainer& txApps,
                             ApplicationContainer& rxApps,
                             NetDeviceContainer& ueDevs);

void PrintApplicationStats(const CommonRanParams& commonParams,
                           const std::string& ratType,
                           NodeContainer& ueNodes,
                           ApplicationContainer& txApps,
                           ApplicationContainer& rxApps);

// == ANIMATION FUNCTIONS =============================================
AnimationInterface* SetupNetAnim(const CommonRanParams& commonParams,
                                 const std::string& ratType,
                                 Ptr<Node> remoteHost,
                                 Ptr<Node> pgw,
                                 Ptr<Node> sgw,
                                 Ptr<Node> mme,
                                 NodeContainer& xnbNodes,
                                 NodeContainer& ueNodes,
                                 const std::string& outputFile);

} // namespace ns3

#endif // IBORAN_RAN_COMMON_H
