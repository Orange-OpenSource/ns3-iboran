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

#pragma once

#include <cstdint>
#include <iterator>
#include <map>
#include <set>

namespace ns3
{

class WaitingTimeTracker
{
  public:
    explicit WaitingTimeTracker(uint64_t maxGapNs = 20'000'000ULL) // 20 ms
        : m_maxGapNs(maxGapNs)
    {
    }

    uint32_t GetWaitingMs(uint16_t rnti, uint64_t nowNs)
    {
        auto it = m_entries.find(rnti);
        if (it == m_entries.end() || nowNs - it->second.lastSeenNs > m_maxGapNs)
        {
            m_entries[rnti] = Entry{nowNs, nowNs};
            return 0;
        }
        it->second.lastSeenNs = nowNs;
        return static_cast<uint32_t>((nowNs - it->second.sinceNs) / 1'000'000ULL);
    }

    void MarkServed(uint16_t rnti, uint64_t nowNs)
    {
        m_entries[rnti] = Entry{nowNs, nowNs};
    }

    void Retain(const std::set<uint16_t>& backlogged)
    {
        for (auto it = m_entries.begin(); it != m_entries.end();)
        {
            it = backlogged.count(it->first) ? std::next(it) : m_entries.erase(it);
        }
    }

  private:
    struct Entry
    {
        uint64_t sinceNs;
        uint64_t lastSeenNs;
    };

    std::map<uint16_t, Entry> m_entries;
    uint64_t m_maxGapNs;
};

} // namespace ns3
