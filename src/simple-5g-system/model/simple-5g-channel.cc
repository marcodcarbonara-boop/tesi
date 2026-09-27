#include "simple-5g-channel.h"

#include "simple-5g-net-device.h"

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Simple5gChannel");

NS_OBJECT_ENSURE_REGISTERED(Simple5gChannel);

TypeId
Simple5gChannel::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Simple5gChannel")
                            .SetParent<Channel>()
                            .SetGroupName("Network")
                            .AddConstructor<Simple5gChannel>()
                            .AddAttribute("Delay",
                                          "Transmission delay through the channel",
                                          TimeValue(Seconds(0)),
                                          MakeTimeAccessor(&Simple5gChannel::m_delay),
                                          MakeTimeChecker());
    return tid;
}

Simple5gChannel::Simple5gChannel()
{
    NS_LOG_FUNCTION(this);
}

void
Simple5gChannel::Send(Ptr<Packet> p,
                    uint16_t protocol,
                    Mac48Address to,
                    Mac48Address from,
                    Ptr<Simple5gNetDevice> sender)
{
    //NS_LOG_UNCOND("Device " << from << " sending packet " << p->GetSize() << " bytes to destination " << to<< " protocol " << protocol);
    NS_LOG_FUNCTION(this << p << protocol << to << from << sender);
    for (auto i = m_devices.begin(); i != m_devices.end(); ++i)
    {
        Ptr<Simple5gNetDevice> tmp = *i;
        if (tmp == sender)
        {
            continue;
        }
        if (m_blackListedDevices.find(tmp) != m_blackListedDevices.end())
        {
            if (find(m_blackListedDevices[tmp].begin(), m_blackListedDevices[tmp].end(), sender) !=
                m_blackListedDevices[tmp].end())
            {
                continue;
            }
        }
        Simulator::ScheduleWithContext(tmp->GetNode()->GetId(),
                                       m_delay,
                                       &Simple5gNetDevice::Receive,
                                       tmp,
                                       p->Copy(),
                                       protocol,
                                       to,
                                       from);
    }
}

void
Simple5gChannel::Add(Ptr<Simple5gNetDevice> device)
{
    NS_LOG_FUNCTION(this << device);
    m_devices.push_back(device);
}

std::size_t
Simple5gChannel::GetNDevices() const
{
    NS_LOG_FUNCTION(this);
    return m_devices.size();
}

Ptr<NetDevice>
Simple5gChannel::GetDevice(std::size_t i) const
{
    NS_LOG_FUNCTION(this << i);
    return m_devices[i];
}

void
Simple5gChannel::BlackList(Ptr<Simple5gNetDevice> from, Ptr<Simple5gNetDevice> to)
{
    if (m_blackListedDevices.find(to) != m_blackListedDevices.end())
    {
        if (find(m_blackListedDevices[to].begin(), m_blackListedDevices[to].end(), from) ==
            m_blackListedDevices[to].end())
        {
            m_blackListedDevices[to].push_back(from);
        }
    }
    else
    {
        m_blackListedDevices[to].push_back(from);
    }
}

void
Simple5gChannel::UnBlackList(Ptr<Simple5gNetDevice> from, Ptr<Simple5gNetDevice> to)
{
    if (m_blackListedDevices.find(to) != m_blackListedDevices.end())
    {
        auto iter = find(m_blackListedDevices[to].begin(), m_blackListedDevices[to].end(), from);
        if (iter != m_blackListedDevices[to].end())
        {
            m_blackListedDevices[to].erase(iter);
        }
    }
}

} // namespace ns3
