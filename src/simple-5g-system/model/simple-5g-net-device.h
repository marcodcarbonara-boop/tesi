#ifndef SIMPLE_5G_NET_DEVICE_H
#define SIMPLE_5G_NET_DEVICE_H

#include "ns3/data-rate.h"
#include "ns3/mac48-address.h"
#include "ns3/queue-fwd.h"

#include "ns3/event-id.h"
#include "ns3/net-device.h"
#include "ns3/traced-callback.h"

#include <stdint.h>
#include <string>

#include "ns3/nstime.h" // For ns3::Time
#include "ns3/error-model.h"
#include "ns3/mobility-model.h" // Required for MobilityModel
#include "ns3/vector.h"         // Required for ns3::Vector
#include <map>                  
#include <utility>              // Required for std::pair

namespace ns3
{

class Simple5gChannel;
class Node;
class ErrorModel;

/**
 * @ingroup netdevice
 *
 * This device assumes 48-bit mac addressing; there is also the possibility to
 * add an ErrorModel if you want to force losses on the device.
 *
 * The device can be installed on a node through the SimpleNetDeviceHelper.
 * In case of manual creation, the user is responsible for assigning an unique
 * address to the device.
 *
 * By default the device is in Broadcast mode, with infinite bandwidth.
 *
 * @brief simple net device for simple things and testing
 */
class Simple5gNetDevice : public NetDevice
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    Simple5gNetDevice();

    /**
     * Receive a packet from a connected Simple5gChannel.  The
     * Simple5gNetDevice receives packets from its connected channel
     * and then forwards them by calling its rx callback method
     *
     * @param packet Packet received on the channel
     * @param protocol protocol number
     * @param to address packet should be sent to
     * @param from address packet was sent from
     */
    void Receive(Ptr<Packet> packet, uint16_t protocol, Mac48Address to, Mac48Address from);

    /**
     * Attach a channel to this net device.  This will be the
     * channel the net device sends on
     *
     * @param channel channel to assign to this net device
     *
     */
    void SetChannel(Ptr<Simple5gChannel> channel);

    /**
     * Attach a queue to the Simple5gNetDevice.
     *
     * @param queue Ptr to the new queue.
     */
    void SetQueue(Ptr<Queue<Packet>> queue);

    /**
     * Get a copy of the attached Queue.
     *
     * @returns Ptr to the queue.
     */
    Ptr<Queue<Packet>> GetQueue() const;

    /**
     * Attach a receive ErrorModel to the Simple5gNetDevice.
     *
     * The Simple5gNetDevice may optionally include an ErrorModel in
     * the packet receive chain.
     *
     * @see ErrorModel
     * @param em Ptr to the ErrorModel.
     */
    void SetReceiveErrorModel(Ptr<ErrorModel> em);

    // inherited from NetDevice base class.
    void SetIfIndex(const uint32_t index) override;
    uint32_t GetIfIndex() const override;
    Ptr<Channel> GetChannel() const override;
    void SetAddress(Address address) override;
    Address GetAddress() const override;
    bool SetMtu(const uint16_t mtu) override;
    uint16_t GetMtu() const override;
    bool IsLinkUp() const override;
    void AddLinkChangeCallback(Callback<void> callback) override;
    bool IsBroadcast() const override;
    Address GetBroadcast() const override;
    bool IsMulticast() const override;
    Address GetMulticast(Ipv4Address multicastGroup) const override;
    bool IsPointToPoint() const override;
    bool IsBridge() const override;
    bool Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber) override;
    bool SendFrom(Ptr<Packet> packet,
                  const Address& source,
                  const Address& dest,
                  uint16_t protocolNumber) override;
    Ptr<Node> GetNode() const override;
    void SetNode(Ptr<Node> node) override;
    bool NeedsArp() const override;
    void SetReceiveCallback(NetDevice::ReceiveCallback cb) override;

    Address GetMulticast(Ipv6Address addr) const override;

    void SetPromiscReceiveCallback(PromiscReceiveCallback cb) override;
    bool SupportsSendFrom() const override;

      
    void SetIsBaseStation(bool isBs);
    bool IsBaseStation() const;

    // Methods for connection handling
    // for UEs:
    void SetConnectedBsAddress(Mac48Address addr, Ptr<Simple5gNetDevice> bs);
    Mac48Address GetConnectedBsAddress() const;
    void DisconnectBsAddress(); // Set as connectedbsaddress a broadcast value to indicate that the UE is not connected to any bs
    // for BSs:
    void AddConnectedUeAddress(Mac48Address addr);
    void RemoveConnectedUeAddress(Mac48Address addr);
    std::vector<Mac48Address> GetConnectedUeAddresses() const;

    /**
     * @brief Schedules a DataRate update for this device at a specific simulation time.
     * @param newDataRate The new DataRate to set.
     * @param updateTime The simulation time at which the DataRate should be updated.
     */
    void ScheduleDataRateUpdate(DataRate newDataRate, Time updateTime);

    /**
     * @brief Starts periodic updates of the DataRate based on device position.
     * This function will schedule a recurring event to check the device's current
     * position and update its DataRate according to the configured matrix.
     * @param checkIntervalSeconds How often to check the position (in seconds).
     * @param gridResolution The size of each square grid cell (in meters).
     */
    void StartPositionBasedDataRateUpdates(double checkIntervalSeconds, double gridResolution);

    /**
     * @brief Loads a throughput matrix from a CSV file.
     * Each cell in the CSV represents a DataRate in Mbps for a 5x5 meter area.
     * @param filePath The absolute path to the CSV file (e.g., "/home/user/Desktop/resources/tput1km/1.csv").
     * @return true if the file was successfully loaded and parsed, false otherwise.
     */
    bool LoadThroughputMatrixFromCsv(const std::string& filePath);

    /**
     * @brief Returns the number of PRBs assigned to the device.
     * @return The number of PRBs.
     */
    uint32_t GetAssignedPrbs() const;

    /**
     * @brief Set the number of PRB assigned to this device.
     * This method is called by the BS to modify the number of assigned PRB.
     * @param prbs Number of PRBs to be assigned.
     */
    void SetAssignedPrbs(uint32_t prbs);

     /**
     * @brief Returns the base path of the CSV file used for the throughput matrix.
     * @return string of the base path.
     */
    std::string GetCsvBasePath() const;

    /**
     * @brief Sets the CSV Base path for the throughput matrix.
     * 
     * @param csvBasePath String of the base path.
     */
    void SetCsvBasePath(std::string csvBasePath);

    /**
     * @brief Returns current data rate.
     */
    DataRate GetDataRate() const;

    /**
     * @brief Getter method for m_xbs variable.
     */
    double GetXBs() const;

    /**
     * @brief Getter method for m_ybs variable.
     */ 
    double GetYBs() const;

    /**
     * @brief Setter method for m_xbs variable.
     */
    void SetXBs(double newxbs);

        /**
     * @brief Setter method for m_ybs variable.
     */
    void SetYBs(double newybs);




  protected:
    void DoDispose() override;

  private:
    Ptr<Simple5gChannel> m_channel;                        //!< the channel the device is connected to
    NetDevice::ReceiveCallback m_rxCallback;             //!< Receive callback
    NetDevice::PromiscReceiveCallback m_promiscCallback; //!< Promiscuous receive callback
    Ptr<Node> m_node;                                    //!< Node this netDevice is associated to
    uint16_t m_mtu;                                      //!< MTU
    uint32_t m_ifIndex;                                  //!< Interface index
    Mac48Address m_address;                              //!< MAC address
    Ptr<ErrorModel> m_receiveErrorModel;                 //!< Receive error model.
    double m_xbs;                                        //!< For BS: x coordinate - for UEs, coordinate of the connected BS
    double m_ybs;                                        //!< For BS: y coordinate - for UEs, coordinate of the connected BS
    
    std::string m_csvBasePath;                            //Base path for .csv for throughput assignment

    /**
     * The trace source fired when the phy layer drops a packet it has received
     * due to the error model being active.  Although Simple5gNetDevice doesn't
     * really have a Phy model, we choose this trace source name for alignment
     * with other trace sources.
     *
     * @see class CallBackTraceSource
     */
    TracedCallback<Ptr<const Packet>> m_phyRxDropTrace;

    /**
     * The StartTransmission method is used internally to start the process
     * of sending a packet out on the channel, by scheduling the
     * FinishTransmission method at a time corresponding to the transmission
     * delay of the packet.
     */
    void StartTransmission();

    /**
     * The FinishTransmission method is used internally to finish the process
     * of sending a packet out on the channel.
     * @param packet The packet to send on the channel
     */
    void FinishTransmission(Ptr<Packet> packet);

    bool m_linkUp; //!< Flag indicating whether or not the link is up

    /**
     * Flag indicating whether or not the NetDevice is a Point to Point model.
     * Enabling this will disable Broadcast and Arp.
     */
    bool m_pointToPointMode;

    Ptr<Queue<Packet>> m_queue;      //!< The Queue for outgoing packets.
    DataRate m_bps;                  //!< The device nominal Data rate. Zero means infinite
    EventId FinishTransmissionEvent; //!< the Tx Complete event

    /**
     * List of callbacks to fire if the link changes state (up or down).
     */
    TracedCallback<> m_linkChangeCallbacks;

    bool m_isBaseStation;  
    // Attributi per la gestione delle connessioni
    Mac48Address m_connectedBsAddress;
    std::vector<Mac48Address> m_connectedUeAddresses;
    
    /**
     * @brief Performs the actual DataRate update.
     * This function is scheduled to be called at the specified update time.
     * @param newDataRate The new DataRate to set.
     */
    void DoUpdateDataRate(DataRate newDataRate);

    std::vector<std::vector<DataRate>> m_throughputMatrix; 
    double m_gridResolution;                                  //!< Resolution of the grid for data rate zones (meters per cell)
    EventId m_dataRateUpdateEvent;                            //!< Event ID for the scheduled periodic data rate update
    double m_checkInterval;                                   //!< Stores the interval for periodic checks (in seconds)

    // --- Private methods for position-based DataRate update ---
    /**
     * @brief Performs the actual DataRate update based on current device position.
     * This function is scheduled to be called periodically by StartPositionBasedDataRateUpdates.
     */
    void DoUpdateDataRateBasedOnPosition();

       uint32_t m_assignedPrbs; //!< Number of PRB assigned to this device

    /**
     * @brief Calculate and distributes PRBs.
     * This method is called by the BS whenever a device connects or disconnects.
     */
    void CalculateAndDistributePrbs();


};

} // namespace ns3

#endif /* SIMPLE_5G_NET_DEVICE_H */
