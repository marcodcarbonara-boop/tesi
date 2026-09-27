#include "simple-5g-net-device-helper.h"

#include "ns3/trace-helper.h"

#include "ns3/abort.h"
#include "ns3/boolean.h"
#include "ns3/config.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/object-factory.h"
#include "ns3/packet.h"
#include "ns3/simple-5g-channel.h"
#include "ns3/simple-5g-net-device.h"
#include "ns3/simulator.h"

#include <string>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Simple5gNetDeviceHelper");

Simple5gNetDeviceHelper::Simple5gNetDeviceHelper()
{
    m_queueFactory.SetTypeId("ns3::DropTailQueue<Packet>");
    m_deviceFactory.SetTypeId("ns3::Simple5gNetDevice");
    m_channelFactory.SetTypeId("ns3::Simple5gChannel");
    m_pointToPointMode = false;
    m_isBaseStation = false; 
    m_enableFlowControl = true;
}

void
Simple5gNetDeviceHelper::SetDeviceAttribute(std::string n1, const AttributeValue& v1)
{
    m_deviceFactory.Set(n1, v1);
}

void
Simple5gNetDeviceHelper::SetChannelAttribute(std::string n1, const AttributeValue& v1)
{
    m_channelFactory.Set(n1, v1);
}

void
Simple5gNetDeviceHelper::SetNetDevicePointToPointMode(bool pointToPointMode)
{
    m_pointToPointMode = pointToPointMode;
}

void
Simple5gNetDeviceHelper::SetIsBaseStation(bool isBaseStation)
{
    m_isBaseStation = isBaseStation;
}

void
Simple5gNetDeviceHelper::DisableFlowControl()
{
    m_enableFlowControl = false;
}

NetDeviceContainer
Simple5gNetDeviceHelper::Install(Ptr<Node> node) const
{
    Ptr<Simple5gChannel> channel = m_channelFactory.Create<Simple5gChannel>();
    return Install(node, channel);
}

NetDeviceContainer
Simple5gNetDeviceHelper::Install(Ptr<Node> node, Ptr<Simple5gChannel> channel) const
{
    return NetDeviceContainer(InstallPriv(node, channel));
}

NetDeviceContainer
Simple5gNetDeviceHelper::Install(const NodeContainer& c) const
{
    Ptr<Simple5gChannel> channel = m_channelFactory.Create<Simple5gChannel>();

    return Install(c, channel);
}

NetDeviceContainer
Simple5gNetDeviceHelper::Install(const NodeContainer& c, Ptr<Simple5gChannel> channel) const
{
    NetDeviceContainer devs;

    for (auto i = c.Begin(); i != c.End(); i++)
    {
        devs.Add(InstallPriv(*i, channel));
    }

    return devs;
}

Ptr<NetDevice>
Simple5gNetDeviceHelper::InstallPriv(Ptr<Node> node, Ptr<Simple5gChannel> channel) const
{
    Ptr<Simple5gNetDevice> device = m_deviceFactory.Create<Simple5gNetDevice>();
    device->SetAttribute("PointToPointMode", BooleanValue(m_pointToPointMode));
    device->SetAttribute("IsBaseStation", BooleanValue(m_isBaseStation));
    device->SetAddress(Mac48Address::Allocate());
    node->AddDevice(device);
    device->SetChannel(channel);
    Ptr<Queue<Packet>> queue = m_queueFactory.Create<Queue<Packet>>();
    device->SetQueue(queue);
    NS_ASSERT_MSG(!m_pointToPointMode || (channel->GetNDevices() <= 2),
                  "Device set to PointToPoint and more than 2 devices on the channel.");
    if (m_enableFlowControl)
    {
        // Aggregate a NetDeviceQueueInterface object
        Ptr<NetDeviceQueueInterface> ndqi = CreateObject<NetDeviceQueueInterface>();
        ndqi->GetTxQueue(0)->ConnectQueueTraces(queue);
        device->AggregateObject(ndqi);
    }
    return device;
}

} // namespace ns3
