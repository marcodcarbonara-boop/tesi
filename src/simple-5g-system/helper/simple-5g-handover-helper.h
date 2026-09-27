#ifndef SIMPLE_5G_HANDOVER_HELPER_H
#define SIMPLE_5G_HANDOVER_HELPER_H

#include "ns3/node.h"
#include "ns3/mobility-model.h"
#include "ns3/vector.h"
#include "ns3/simple-5g-net-device.h"
#include <cmath>
#include <map>

namespace ns3
{

/**
 * @brief Helper class to manage simple 5G handovers.
 * This class provides utility functions to facilitate a simple handover
 * process between a device and a base station.
 */
class Simple5gHandoverHelper
{
  public:
    /**
     * @brief Calculates the Euclidean distance between two nodes.
     * This function retrieves the positions of the two nodes from their
     * respective mobility models and calculates the 3D distance between them.
     * @param deviceNode The node representing the device (e.g., UE).
     * @param bsNode The node representing the base station.
     * @return The distance in meters, or -1.0 if a mobility model is not found.
     */
    static double CalculateDistance(Ptr<Node> deviceNode, Ptr<Node> bsNode);

   /**
     * @brief Performs a simple handover check and execution.
     * This function checks if the current device data rate is zero. If so, it
     * calculates the distance to a new potential base station and performs a handover
     * if the new distance is shorter than the old one.
     * @param device The Simple5gNetDevice of the user equipment (UE).
     * @param oldBs The Simple5gNetDevice of the old base station.
     * @param newBs The Simple5gNetDevice of the new base station.
     * @return true if a handover was performed, false otherwise.
     */
      bool CheckAndPerformHandover(Ptr<Simple5gNetDevice> device,
                                   Ptr<Simple5gNetDevice> oldBs,
                                   Ptr<Simple5gNetDevice> newBs);
};
}
// namespace ns3

#endif /* SIMPLE_5G_HANDOVER_HELPER_H */
