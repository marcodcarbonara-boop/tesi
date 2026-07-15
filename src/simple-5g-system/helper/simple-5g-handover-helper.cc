#include "simple-5g-handover-helper.h"
#include "ns3/log.h"
#include "ns3/abort.h"


namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Simple5gHandoverHelper");

double
Simple5gHandoverHelper::CalculateDistance(Ptr<Node> deviceNode, Ptr<Node> bsNode)
{
    NS_LOG_FUNCTION(deviceNode << bsNode);

    // Get the mobility models from the nodes.
    Ptr<MobilityModel> deviceMobility = deviceNode->GetObject<MobilityModel>();
    Ptr<MobilityModel> bsMobility = bsNode->GetObject<MobilityModel>();

    // Check if mobility models exist.
    if (deviceMobility == nullptr || bsMobility == nullptr)
    {
        NS_LOG_ERROR("Mobility models not found on one or both nodes.");
        return -1.0; // Return a negative value to indicate an error.
    }

    // Get the positions from the mobility models.
    Vector devicePosition = deviceMobility->GetPosition();
    Vector bsPosition = bsMobility->GetPosition();

    // Calculate the Euclidean distance.
    double distance = (devicePosition - bsPosition).GetLength();

    NS_LOG_INFO("Distance between device and base station: " << distance << " meters.");

    return distance;
}

bool
Simple5gHandoverHelper::CheckAndPerformHandover(Ptr<Simple5gNetDevice> device,
                                                Ptr<Simple5gNetDevice> oldBs,
                                                Ptr<Simple5gNetDevice> newBs)
{
    NS_LOG_FUNCTION(device << oldBs << newBs);

    // Get the current data rate of the device.
    DataRate currentDataRate = device->GetDataRate();

    // Check if the data rate is zero.
    if (currentDataRate.GetBitRate() == 0)
    {
        // Get the nodes of the devices.
        Ptr<Node> deviceNode = device->GetNode();
        Ptr<Node> oldBsNode = oldBs->GetNode();
        Ptr<Node> newBsNode = newBs->GetNode();

        // Calculate distances to both old and new base stations.

        double distanceToOldBs = Simple5gHandoverHelper::CalculateDistance(deviceNode, oldBsNode);
        double distanceToNewBs = Simple5gHandoverHelper::CalculateDistance(deviceNode, newBsNode);

        // Check for valid distances before comparison.
        if (distanceToOldBs >= 0 && distanceToNewBs >= 0)
        {
            // Perform handover if the device is closer to the new base station.
            if (distanceToNewBs < distanceToOldBs)
            {
                NS_LOG_INFO("Handover triggered: device is closer to the new BS.");

                // Disconnect the device from the old base station.
                oldBs->RemoveConnectedUeAddress(Mac48Address::ConvertFrom(device->GetAddress()));

                // Connect the device to the new base station.
                newBs->AddConnectedUeAddress(Mac48Address::ConvertFrom(device->GetAddress()));

                device->SetConnectedBsAddress(Mac48Address::ConvertFrom(newBs->GetAddress()), newBs);

                NS_LOG_INFO("Handover successful from " << oldBs->GetAddress() << " to " << newBs->GetAddress());
                return true;
            }
        }
    }

    NS_LOG_INFO("No handover performed.");
    return false;
}



} // namespace ns3
