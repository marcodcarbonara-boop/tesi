#include "simple-sat-channel.h"

#include "simple-sat-net-device.h"

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SimpleSatChannel");

NS_OBJECT_ENSURE_REGISTERED(SimpleSatChannel);

TypeId
SimpleSatChannel::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SimpleSatChannel")
                            .SetParent<Channel>()
                            .SetGroupName("Network")
                            .AddConstructor<SimpleSatChannel>()
                            .AddAttribute("Delay",
                                          "Transmission delay through the channel",
                                          TimeValue(Seconds(0)),
                                          MakeTimeAccessor(&SimpleSatChannel::m_delay),
                                          MakeTimeChecker());
    return tid;
}

SimpleSatChannel::SimpleSatChannel()
{
    NS_LOG_FUNCTION(this);
}

void
SimpleSatChannel::Send(Ptr<Packet> p,
                    uint16_t protocol,
                    Mac48Address to,
                    Mac48Address from,
                    Ptr<SimpleSatNetDevice> sender)
{

    NS_LOG_FUNCTION(this << p << protocol << to << from << sender);
    //scorre tutti i device del canale
    for (auto i = m_devices.begin(); i != m_devices.end(); ++i)
    {
        Ptr<SimpleSatNetDevice> tmp = *i;
        if (tmp == sender)
        {
            continue;
        }
            //se il ricevitore ha in blacklist il sender, non invio
        if (m_blackListedDevices.find(tmp) != m_blackListedDevices.end())
        {
            if (find(m_blackListedDevices[tmp].begin(), m_blackListedDevices[tmp].end(), sender) !=
                m_blackListedDevices[tmp].end())
            {
                continue;
            }
        }
           // NS_LOG_UNCOND("Sending packet to SimpleSatNetDevice " << tmp->GetAddress());

        //tra m_delay secondi fa partire la funzione Receive del device tmp, con i parametri del pacchetto e degli indirizzi
        Simulator::ScheduleWithContext(tmp->GetNode()->GetId(),
                                       m_delay,
                                       &SimpleSatNetDevice::Receive,
                                       tmp,
                                       p->Copy(),
                                       protocol,
                                       to,
                                       from);
    }
}

void
SimpleSatChannel::Add(Ptr<SimpleSatNetDevice> device)
{
    NS_LOG_FUNCTION(this << device);
    m_devices.push_back(device);
}

std::size_t
SimpleSatChannel::GetNDevices() const
{
    NS_LOG_FUNCTION(this);
    return m_devices.size();
}

Ptr<NetDevice>
SimpleSatChannel::GetDevice(std::size_t i) const
{
    NS_LOG_FUNCTION(this << i);
    return m_devices[i];
}

void
SimpleSatChannel::BlackList(Ptr<SimpleSatNetDevice> from, Ptr<SimpleSatNetDevice> to)
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
SimpleSatChannel::UnBlackList(Ptr<SimpleSatNetDevice> from, Ptr<SimpleSatNetDevice> to)
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
