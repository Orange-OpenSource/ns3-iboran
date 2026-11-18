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

#include "iboran-app-header.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("IbOranAppHeader");

NS_OBJECT_ENSURE_REGISTERED(IbOranAppHeader);

IbOranAppHeader::IbOranAppHeader()
{
    NS_LOG_FUNCTION(this);
}

IbOranAppHeader::IbOranAppHeader(const IbOranFragmentHeader& fields)
    : m_fields(fields)
{
    NS_LOG_FUNCTION(this);
}

void
IbOranAppHeader::SetFields(const IbOranFragmentHeader& fields)
{
    m_fields = fields;
    m_valid = true;
}

const IbOranFragmentHeader&
IbOranAppHeader::GetFields() const
{
    return m_fields;
}

bool
IbOranAppHeader::IsValid() const
{
    return m_valid;
}

TypeId
IbOranAppHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::IbOranAppHeader")
                            .SetParent<Header>()
                            .SetGroupName("IbOran")
                            .AddConstructor<IbOranAppHeader>();
    return tid;
}

TypeId
IbOranAppHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
IbOranAppHeader::Print(std::ostream& os) const
{
    os << "(msg=" << m_fields.messageId << " task=" << m_fields.taskId
       << " round=" << m_fields.round << " frag=" << m_fields.fragmentIndex
       << " off=" << m_fields.offset << " len=" << m_fields.fragmentLength << "/"
       << m_fields.wireSize << " type=" << ContentTypeToString(m_fields.contentType)
       << " crit=" << static_cast<int>(m_fields.criticality) << (m_fields.IsFirst() ? " FIRST" : "")
       << (m_fields.IsLast() ? " LAST" : "") << ")";
}

uint32_t
IbOranAppHeader::GetSerializedSize() const
{
    return IbOranFragmentHeader::kSize;
}

void
IbOranAppHeader::Serialize(Buffer::Iterator start) const
{
    uint8_t buf[IbOranFragmentHeader::kSize];
    m_fields.Serialize(buf);
    start.Write(buf, IbOranFragmentHeader::kSize);
}

uint32_t
IbOranAppHeader::Deserialize(Buffer::Iterator start)
{
    uint8_t buf[IbOranFragmentHeader::kSize];
    start.Read(buf, IbOranFragmentHeader::kSize);
    m_valid = m_fields.Deserialize(buf, IbOranFragmentHeader::kSize);
    return IbOranFragmentHeader::kSize;
}

} // namespace ns3
