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

#ifndef IBORAN_SIDE_INFO_TRACKER_H
#define IBORAN_SIDE_INFO_TRACKER_H

#include "iboran-application.h"

#include <cstdint>
#include <map>
#include <mutex>

namespace ns3
{

class SideInfoTracker
{
  public:
    static SideInfoTracker& GetInstance();
    void UpdateRntiCriticality(uint16_t rnti, LinkDirection direction, uint8_t criticality);
    uint8_t GetRntiCriticality(uint16_t rnti, LinkDirection direction);

    void UpdateRntiObjectId(uint16_t rnti, LinkDirection direction, std::string objectId);
    std::string GetRntiObjectId(uint16_t rnti, LinkDirection direction);

  private:
    SideInfoTracker() = default;
    std::map<std::pair<uint16_t, LinkDirection>, uint8_t> m_criticalityMap;
    std::map<std::pair<uint16_t, LinkDirection>, std::string> m_objectIdMap;
    std::mutex m_mutex;
};

} // namespace ns3

#endif // IBORAN_SIDE_INFO_TRACKER_H
