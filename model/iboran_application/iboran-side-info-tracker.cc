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

#include "iboran-side-info-tracker.h"

namespace ns3
{

SideInfoTracker&
SideInfoTracker::GetInstance()
{
    static SideInfoTracker instance;
    return instance;
}

void
SideInfoTracker::UpdateRntiCriticality(uint16_t rnti, LinkDirection direction, uint8_t criticality)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_criticalityMap[{rnti, direction}] = criticality;
}

uint8_t
SideInfoTracker::GetRntiCriticality(uint16_t rnti, LinkDirection direction)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_criticalityMap.find({rnti, direction});
    if (it != m_criticalityMap.end())
    {
        return it->second;
    }
    return 10; // default
}

void
SideInfoTracker::UpdateRntiObjectId(uint16_t rnti, LinkDirection direction, std::string objectId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_objectIdMap[{rnti, direction}] = objectId;
}

std::string
SideInfoTracker::GetRntiObjectId(uint16_t rnti, LinkDirection direction)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_objectIdMap.find({rnti, direction});
    if (it != m_objectIdMap.end())
    {
        return it->second;
    }
    return "null";
}

void
SideInfoTracker::UpdateUeId(uint16_t cellId, uint16_t rnti, uint64_t ueId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ueIdMap[{cellId, rnti}] = ueId;
}

uint64_t
SideInfoTracker::GetUeId(uint16_t cellId, uint16_t rnti)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_ueIdMap.find({cellId, rnti});
    return (it != m_ueIdMap.end()) ? it->second : 0; // 0: not identified yet
}

} // namespace ns3
