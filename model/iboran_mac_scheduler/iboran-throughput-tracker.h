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

#ifndef IBORAN_THROUGHPUT_TRACKER_H
#define IBORAN_THROUGHPUT_TRACKER_H

#include <cmath>
#include <cstdint>
#include <map>

namespace ns3
{

/**
 * Served-throughput estimate per RNTI with an exponential kernel:
 *   r(t) = sum_i b_i * exp(-(t - t_i) / tau) / tau   [bit/s]
 * Converges to the true rate for any constant service pattern and
 * decays to zero when the UE stops being served.
 */
class ThroughputTracker
{
  public:
    explicit ThroughputTracker(double tauMs = 100.0)
        : m_tauNs(tauMs * 1e6)
    {
    }

    void AddServed(uint16_t rnti, uint64_t nowNs, uint32_t bytes)
    {
        State& s = m_state[rnti];
        Decay(s, nowNs);
        s.rateBps += 8.0 * bytes / (m_tauNs * 1e-9);
    }

    double GetMbps(uint16_t rnti, uint64_t nowNs) const
    {
        auto it = m_state.find(rnti);
        if (it == m_state.end())
        {
            return 0.0;
        }
        const double dt =
            (nowNs > it->second.lastNs) ? static_cast<double>(nowNs - it->second.lastNs) : 0.0;
        return it->second.rateBps * std::exp(-dt / m_tauNs) / 1e6;
    }

    void Erase(uint16_t rnti)
    {
        m_state.erase(rnti);
    }

  private:
    struct State
    {
        double rateBps{0.0};
        uint64_t lastNs{0};
    };

    void Decay(State& s, uint64_t nowNs) const
    {
        if (nowNs > s.lastNs)
        {
            s.rateBps *= std::exp(-static_cast<double>(nowNs - s.lastNs) / m_tauNs);
        }
        s.lastNs = nowNs;
    }

    double m_tauNs;
    std::map<uint16_t, State> m_state;
};

} // namespace ns3

#endif
