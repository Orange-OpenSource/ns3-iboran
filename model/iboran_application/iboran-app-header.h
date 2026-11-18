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

#ifndef IBORAN_APP_HEADER_H
#define IBORAN_APP_HEADER_H

#include "iboran-app-message.h"

#include "ns3/header.h"

namespace ns3
{

class IbOranAppHeader : public Header
{
  public:
    IbOranAppHeader();
    explicit IbOranAppHeader(const IbOranFragmentHeader& fields);

    void SetFields(const IbOranFragmentHeader& fields);
    const IbOranFragmentHeader& GetFields() const;

    /** False if the last Deserialize() met a bad magic, version or bound. */
    bool IsValid() const;

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;

  private:
    IbOranFragmentHeader m_fields;
    bool m_valid{true};
};

} // namespace ns3

#endif // IBORAN_APP_HEADER_H
