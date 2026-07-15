#include "simple-sat-net-device-helper.h"

#include "ns3/trace-helper.h"

#include "ns3/abort.h"
#include "ns3/boolean.h"
#include "ns3/config.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/object-factory.h"
#include "ns3/packet.h"
#include "ns3/simple-sat-channel.h"
#include "ns3/simple-sat-net-device.h"
#include "ns3/simulator.h"

#include <string>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SimpleSatNetDeviceHelper");

SimpleSatNetDeviceHelper::SimpleSatNetDeviceHelper()
{
    m_queueFactory.SetTypeId("ns3::DropTailQueue<Packet>");
    m_deviceFactory.SetTypeId("ns3::SimpleSatNetDevice");
    m_channelFactory.SetTypeId("ns3::SimpleSatChannel");
    m_pointToPointMode = false;
    m_enableFlowControl = true;
}

void
SimpleSatNetDeviceHelper::SetDeviceAttribute(std::string n1, const AttributeValue& v1)
{
    m_deviceFactory.Set(n1, v1);
    NS_LOG_INFO("Log in Setdeviceattribute");
}

void
SimpleSatNetDeviceHelper::SetChannelAttribute(std::string n1, const AttributeValue& v1)
{
    m_channelFactory.Set(n1, v1);
}

void
SimpleSatNetDeviceHelper::SetNetDevicePointToPointMode(bool pointToPointMode)
{
    m_pointToPointMode = pointToPointMode;
}

void
SimpleSatNetDeviceHelper::DisableFlowControl()
{
    m_enableFlowControl = false;
}

NetDeviceContainer
SimpleSatNetDeviceHelper::Install(Ptr<Node> node) const
{
    Ptr<SimpleSatChannel> channel = m_channelFactory.Create<SimpleSatChannel>();
    return Install(node, channel);
}

NetDeviceContainer
SimpleSatNetDeviceHelper::Install(Ptr<Node> node, Ptr<SimpleSatChannel> channel) const
{
    return NetDeviceContainer(InstallPriv(node, channel));
}

NetDeviceContainer
SimpleSatNetDeviceHelper::Install(const NodeContainer& c) const
{
    Ptr<SimpleSatChannel> channel = m_channelFactory.Create<SimpleSatChannel>();

    return Install(c, channel);
}

NetDeviceContainer
SimpleSatNetDeviceHelper::Install(const NodeContainer& c, Ptr<SimpleSatChannel> channel) const
{
    NetDeviceContainer devs;

    for (auto i = c.Begin(); i != c.End(); i++)
    {
        devs.Add(InstallPriv(*i, channel));
    }

    return devs;
}

Ptr<NetDevice>
SimpleSatNetDeviceHelper::InstallPriv(Ptr<Node> node, Ptr<SimpleSatChannel> channel) const
{
    Ptr<SimpleSatNetDevice> device = m_deviceFactory.Create<SimpleSatNetDevice>();
    device->SetAttribute("PointToPointMode", BooleanValue(m_pointToPointMode));
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
