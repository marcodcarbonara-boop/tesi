#include "simple-5g-net-device.h"

#include "ns3/error-model.h"
#include "ns3/queue.h"
#include "ns3/simple-5g-channel.h"

#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/tag.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/mobility-model.h" // Required for MobilityModel
#include "ns3/vector.h"         // Required for ns3::Vector
#include <cmath>                // Required for std::floor
#include <fstream>              // Required for std::ifstream
#include <sstream>              // Required for std::stringstream
#include <limits>               // Required for std::numeric_limits
//NTOTPRB = 100
//IN BASE A QUANTI UE STANNO COLLEGATI ALLA BS, VIENE DATO AL SINGOLO UE UN DETERMINATO NUMERO DI PRB (PRBTOT / N)

//DOPODICHE CON QUEL NUMERO DI PRB VIENE ASSEGNATO IL CSV CORRISPONDENTE (PIU PRB = PIU DATARATE) 
//IL CSV DA CUI PRENDE IL DATARATE E' UNA MATRICE 2D (posizioni x,y) CHE CONTIENE I DATARATE CORRISPONDENTI A QUELLE POSIZIONI. 
//OGNI VOLTA CHE AGGIORNA IL DEVICE PRENDE LA SUA POSIZIONE CORRENTE x,y E LA CERCA NEL CSV PER PRENDERE IL DATARATE
namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Simple5gNetDevice");

/**
 * @brief Simple5gNetDevice tag to store source, destination and protocol of each packet.
 */
class Simple5gTag : public Tag
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;

    uint32_t GetSerializedSize() const override;
    void Serialize(TagBuffer i) const override;
    void Deserialize(TagBuffer i) override;

    /**
     * Set the source address
     * @param src source address
     */
    void SetSrc(Mac48Address src);
    /**
     * Get the source address
     * @return the source address
     */
    Mac48Address GetSrc() const;

    /**
     * Set the destination address
     * @param dst destination address
     */
    void SetDst(Mac48Address dst);
    /**
     * Get the destination address
     * @return the destination address
     */
    Mac48Address GetDst() const;

    /**
     * Set the protocol number
     * @param proto protocol number
     */
    void SetProto(uint16_t proto);
    /**
     * Get the protocol number
     * @return the protocol number
     */
    uint16_t GetProto() const;

    void Print(std::ostream& os) const override;

  private:
    Mac48Address m_src;        //!< source address
    Mac48Address m_dst;        //!< destination address
    uint16_t m_protocolNumber; //!< protocol number
};

NS_OBJECT_ENSURE_REGISTERED(Simple5gTag);

TypeId
Simple5gTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Simple5gTag")
                            .SetParent<Tag>()
                            .SetGroupName("Network")
                            .AddConstructor<Simple5gTag>();
    return tid;
}

TypeId
Simple5gTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
Simple5gTag::GetSerializedSize() const
{
    return 8 + 8 + 2;
}

void
Simple5gTag::Serialize(TagBuffer i) const
{
    uint8_t mac[6];
    m_src.CopyTo(mac);
    i.Write(mac, 6);
    m_dst.CopyTo(mac);
    i.Write(mac, 6);
    i.WriteU16(m_protocolNumber);
}

void
Simple5gTag::Deserialize(TagBuffer i)
{
    uint8_t mac[6];
    i.Read(mac, 6);
    m_src.CopyFrom(mac);
    i.Read(mac, 6);
    m_dst.CopyFrom(mac);
    m_protocolNumber = i.ReadU16();
}

void
Simple5gTag::SetSrc(Mac48Address src)
{
    m_src = src;
}

Mac48Address
Simple5gTag::GetSrc() const
{
    return m_src;
}

void
Simple5gTag::SetDst(Mac48Address dst)
{
    m_dst = dst;
}

Mac48Address
Simple5gTag::GetDst() const
{
    return m_dst;
}

void
Simple5gTag::SetProto(uint16_t proto)
{
    m_protocolNumber = proto;
}

uint16_t
Simple5gTag::GetProto() const
{
    return m_protocolNumber;
}

void
Simple5gTag::Print(std::ostream& os) const
{
    os << "src=" << m_src << " dst=" << m_dst << " proto=" << m_protocolNumber;
}

NS_OBJECT_ENSURE_REGISTERED(Simple5gNetDevice);

const uint32_t TOTAL_PRB = 100;

TypeId
Simple5gNetDevice::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Simple5gNetDevice")
            .SetParent<NetDevice>()
            .SetGroupName("Network")
            .AddConstructor<Simple5gNetDevice>()
            .AddAttribute("ReceiveErrorModel",
                          "The receiver error model used to simulate packet loss",
                          PointerValue(),
                          MakePointerAccessor(&Simple5gNetDevice::m_receiveErrorModel),
                          MakePointerChecker<ErrorModel>())
            .AddAttribute("Address",
                        "The MAC address of this device.",
                          Mac48AddressValue(Mac48Address::Allocate()),
                          MakeMac48AddressAccessor(&Simple5gNetDevice::m_address),
                          MakeMac48AddressChecker())
            .AddAttribute("PointToPointMode",
                          "The device is configured in Point to Point mode",
                          BooleanValue(false),
                          MakeBooleanAccessor(&Simple5gNetDevice::m_pointToPointMode),
                          MakeBooleanChecker())
            .AddAttribute("TxQueue",
                          "A queue to use as the transmit queue in the device.",
                          StringValue("ns3::DropTailQueue<Packet>"),
                          MakePointerAccessor(&Simple5gNetDevice::m_queue),
                          MakePointerChecker<Queue<Packet>>())
            .AddAttribute("DataRate",
                          "The default data rate for point to point links. Zero means infinite",
                          DataRateValue(DataRate("0b/s")),
                          MakeDataRateAccessor(&Simple5gNetDevice::m_bps),
                          MakeDataRateChecker())
            .AddAttribute("IsBaseStation", 
                          "A boolean value to identify a base station device.", 
                          BooleanValue(false), 
                          MakeBooleanAccessor(&Simple5gNetDevice::m_isBaseStation), 
                          MakeBooleanChecker()) 
            .AddAttribute("CsvBasePath", 
                          "Path to the directory containing throughput CSV files (1.csv-100.csv).",
                          StringValue("/home/pasta1/ns-allinone-3.38/ns-3.38/scratch/moduli/resources/tput1km/"), 
                          MakeStringAccessor(&Simple5gNetDevice::m_csvBasePath),
                          MakeStringChecker())             
            .AddTraceSource("PhyRxDrop",
                            "Trace source indicating a packet has been dropped "
                            "by the device during reception",
                            MakeTraceSourceAccessor(&Simple5gNetDevice::m_phyRxDropTrace),
                            "ns3::Packet::TracedCallback");
    return tid;
}

Simple5gNetDevice::Simple5gNetDevice()
    : m_channel(nullptr),
      m_node(nullptr),
      m_mtu(0xffff),
      m_ifIndex(0),
      m_csvBasePath(""),
      m_linkUp(false),
      m_isBaseStation(false),
      m_assignedPrbs(0),
      m_xbs(0),
      m_ybs(0)
      
{
    m_connectedBsAddress = Mac48Address();
    NS_LOG_FUNCTION(this);
}


void
Simple5gNetDevice::SetConnectedBsAddress(Mac48Address addr, Ptr<Simple5gNetDevice> bs)
{
    // Method intended for user equipment
    if (!IsBaseStation())
    {
        m_connectedBsAddress = addr;
        m_xbs = bs->GetXBs();
        m_ybs = bs->GetYBs();
    }
    else
    {
        NS_LOG_WARN("Attempting to set a BS address to a Base Station device. Aborting operation.");
    }
}

Mac48Address
Simple5gNetDevice::GetConnectedBsAddress() const
{
    return m_connectedBsAddress;
}

void
Simple5gNetDevice::DisconnectBsAddress() 
{
    if (!IsBaseStation())
    {
        m_connectedBsAddress = ns3::Mac48Address::GetBroadcast();
        m_xbs = 99999;
        m_ybs = 99999;
    }
    else
    {
        NS_LOG_WARN("Attempting disconnect a BS address from a Base Station device. Aborting operation.");
    }
}

void
Simple5gNetDevice::AddConnectedUeAddress(Mac48Address addr)
{
    // Method intended for base stations
    if (IsBaseStation())
    {
        // Check if address is already registered
        if (std::find(m_connectedUeAddresses.begin(), m_connectedUeAddresses.end(), addr) == m_connectedUeAddresses.end())
        {
            m_connectedUeAddresses.push_back(addr);
            NS_LOG_INFO("Base Station " << m_address << " has registered a new connected mobile device: " << addr);

        }
        CalculateAndDistributePrbs();
    }
    else
    {
        NS_LOG_WARN("Attempting to add a connected device to a non-base station device. Aborting operation.");
    }
}

void
Simple5gNetDevice::RemoveConnectedUeAddress(Mac48Address addr)
{
    NS_LOG_FUNCTION(this << addr);
    auto it = std::find(m_connectedUeAddresses.begin(), m_connectedUeAddresses.end(), addr);
    if (it != m_connectedUeAddresses.end())
    {
        m_connectedUeAddresses.erase(it);
        NS_LOG_INFO("Base Station " << m_address << " removed mobile device " << addr);
        CalculateAndDistributePrbs();
    }
    else
    {
        NS_LOG_WARN("Mobile device with address " << addr << " not found in the connected UE list. No removal.");
    }
}

std::vector<Mac48Address>
Simple5gNetDevice::GetConnectedUeAddresses() const
{
    return m_connectedUeAddresses;
}


void
Simple5gNetDevice::Receive(Ptr<Packet> packet, uint16_t protocol, Mac48Address to, Mac48Address from)
{
    NS_LOG_FUNCTION(this << packet << protocol << to << from);
    NetDevice::PacketType packetType;

    if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt(packet))
    {
        m_phyRxDropTrace(packet);
        return;
    }

    if (to == m_address)
    {
        packetType = NetDevice::PACKET_HOST;
    }
    else if (to.IsBroadcast())
    {
        packetType = NetDevice::PACKET_BROADCAST;
    }
    else if (to.IsGroup())
    {
        packetType = NetDevice::PACKET_MULTICAST;
    }
    else
    {
        packetType = NetDevice::PACKET_OTHERHOST;
    }

    if (packetType != NetDevice::PACKET_OTHERHOST)
    {
        m_rxCallback(this, packet, protocol, from);
    }

    if (!m_promiscCallback.IsNull())
    {
        m_promiscCallback(this, packet, protocol, from, to, packetType);
    }
}

void
Simple5gNetDevice::SetChannel(Ptr<Simple5gChannel> channel)
{
    NS_LOG_FUNCTION(this << channel);
    m_channel = channel;
    m_channel->Add(this);
    m_linkUp = true;
    m_linkChangeCallbacks();
}

Ptr<Queue<Packet>>
Simple5gNetDevice::GetQueue() const
{
    NS_LOG_FUNCTION(this);
    return m_queue;
}

void
Simple5gNetDevice::SetQueue(Ptr<Queue<Packet>> q)
{
    NS_LOG_FUNCTION(this << q);
    m_queue = q;
}

void
Simple5gNetDevice::SetReceiveErrorModel(Ptr<ErrorModel> em)
{
    NS_LOG_FUNCTION(this << em);
    m_receiveErrorModel = em;
}

void
Simple5gNetDevice::SetIfIndex(const uint32_t index)
{
    NS_LOG_FUNCTION(this << index);
    m_ifIndex = index;
}

uint32_t
Simple5gNetDevice::GetIfIndex() const
{
    NS_LOG_FUNCTION(this);
    return m_ifIndex;
}

Ptr<Channel>
Simple5gNetDevice::GetChannel() const
{
    NS_LOG_FUNCTION(this);
    return m_channel;
}

void
Simple5gNetDevice::SetAddress(Address address)
{
    NS_LOG_FUNCTION(this << address);
    m_address = Mac48Address::ConvertFrom(address);
}

Address
Simple5gNetDevice::GetAddress() const
{
    //
    // Implicit conversion from Mac48Address to Address
    //
    NS_LOG_FUNCTION(this);
    return m_address;
}

bool
Simple5gNetDevice::SetMtu(const uint16_t mtu)
{
    NS_LOG_FUNCTION(this << mtu);
    m_mtu = mtu;
    return true;
}

uint16_t
Simple5gNetDevice::GetMtu() const
{
    NS_LOG_FUNCTION(this);
    return m_mtu;
}

bool
Simple5gNetDevice::IsLinkUp() const
{
    NS_LOG_FUNCTION(this);
    return m_linkUp;
}

void
Simple5gNetDevice::AddLinkChangeCallback(Callback<void> callback)
{
    NS_LOG_FUNCTION(this << &callback);
    m_linkChangeCallbacks.ConnectWithoutContext(callback);
}

bool
Simple5gNetDevice::IsBroadcast() const
{
    NS_LOG_FUNCTION(this);
    return !m_pointToPointMode;
}

Address
Simple5gNetDevice::GetBroadcast() const
{
    NS_LOG_FUNCTION(this);
    return Mac48Address::GetBroadcast();
}

bool
Simple5gNetDevice::IsMulticast() const
{
    NS_LOG_FUNCTION(this);
    return !m_pointToPointMode;
}

Address
Simple5gNetDevice::GetMulticast(Ipv4Address multicastGroup) const
{
    NS_LOG_FUNCTION(this << multicastGroup);
    return Mac48Address::GetMulticast(multicastGroup);
}

Address
Simple5gNetDevice::GetMulticast(Ipv6Address addr) const
{
    NS_LOG_FUNCTION(this << addr);
    return Mac48Address::GetMulticast(addr);
}

bool
Simple5gNetDevice::IsPointToPoint() const
{
    NS_LOG_FUNCTION(this);
    return m_pointToPointMode;
}

bool
Simple5gNetDevice::IsBridge() const
{
    NS_LOG_FUNCTION(this);
    return false;
}

bool
Simple5gNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
    //NS_LOG_UNCOND("send 5g");
   // NS_LOG_UNCOND("Device " << m_address << " sending packet " << packet->GetSize() << " bytes to destination " << dest<< " protocol " << protocolNumber);
    NS_LOG_FUNCTION(this << packet << dest << protocolNumber);

    return SendFrom(packet, m_address, dest, protocolNumber);
}

bool
Simple5gNetDevice::SendFrom(Ptr<Packet> p,
                          const Address& source,
                          const Address& dest,
                          uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this << p << source << dest << protocolNumber);
    if (p->GetSize() > GetMtu())
    {
        return false;
    }

    Mac48Address to = Mac48Address::ConvertFrom(dest);
    Mac48Address from = Mac48Address::ConvertFrom(source);

    Simple5gTag tag;
    tag.SetSrc(from);
    tag.SetDst(to);
    tag.SetProto(protocolNumber);

    p->AddPacketTag(tag);

    if (m_queue->Enqueue(p))
    {
        if (m_queue->GetNPackets() == 1 && !FinishTransmissionEvent.IsRunning())
        {
            StartTransmission();
        }
        return true;
    }
    //NS_LOG_UNCOND("PACCHETTO SCARTATO");
    return false;
}

void
Simple5gNetDevice::StartTransmission()
{

    if (m_queue->GetNPackets() == 0)
    {
        return;
    }
    NS_ASSERT_MSG(!FinishTransmissionEvent.IsRunning(),
                  "Tried to transmit a packet while another transmission was in progress");
    Ptr<Packet> packet = m_queue->Dequeue();
    /**
     * Simple5gChannel will deliver the packet to the far end(s) of the link as soon as Send is called
     * (or after its fixed delay, if one is configured). So we have to handle the rate of the link
     * here, which we do by scheduling FinishTransmission (packetSize / linkRate) time in the
     * future. While that event is running, the transmit path of this NetDevice is busy, so we can't
     * send other packets.
     *
     * Simple5gChannel doesn't have a locking mechanism, and doesn't check for collisions, so there's
     * nothing we need to do with the channel until the transmission has "completed" from the
     * perspective of this NetDevice.
     */
    Time txTime = Time(0);
   // NS_LOG_UNCOND("[" << Simulator::Now().GetSeconds() << "s] Device " << m_address << " starting transmission - DataRate: " << m_bps << ", packet size: " << packet->GetSize() << " bytes");
    if (m_bps > DataRate(0))
    {
        txTime = m_bps.CalculateBytesTxTime(packet->GetSize());
    }
    FinishTransmissionEvent =
        Simulator::Schedule(txTime, &Simple5gNetDevice::FinishTransmission, this, packet);
}

void
Simple5gNetDevice::FinishTransmission(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this);
    //NS_LOG_UNCOND("[" << Simulator::Now().GetSeconds() << "s] Device " << m_address << " finished transmission - DataRate was: " << m_bps << ", packet size: " << packet->GetSize() << " bytes");

    Simple5gTag tag;
    packet->RemovePacketTag(tag);

    Mac48Address src = tag.GetSrc();
    Mac48Address dst = tag.GetDst();
    uint16_t proto = tag.GetProto();

    m_channel->Send(packet, proto, dst, src, this);

    StartTransmission();
}

Ptr<Node>
Simple5gNetDevice::GetNode() const
{
    NS_LOG_FUNCTION(this);
    return m_node;
}

void
Simple5gNetDevice::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    m_node = node;
}

bool
Simple5gNetDevice::NeedsArp() const
{
    NS_LOG_FUNCTION(this);
    return !m_pointToPointMode;
}

void
Simple5gNetDevice::SetReceiveCallback(NetDevice::ReceiveCallback cb)
{
    NS_LOG_FUNCTION(this << &cb);
    m_rxCallback = cb;
}

// --- Modified DoDispose to cancel the scheduled event ---
void
Simple5gNetDevice::DoDispose()
{
    NS_LOG_FUNCTION(this);
    if (m_dataRateUpdateEvent.IsRunning())
    {
        m_dataRateUpdateEvent.Cancel(); // Cancel any pending position-based update events
    }
    m_channel = nullptr;
    m_node = nullptr;
    m_receiveErrorModel = nullptr;
    m_queue->Dispose();
    if (FinishTransmissionEvent.IsRunning())
    {
        FinishTransmissionEvent.Cancel();
    }
    NetDevice::DoDispose();
}

void
Simple5gNetDevice::SetPromiscReceiveCallback(PromiscReceiveCallback cb)
{
    NS_LOG_FUNCTION(this << &cb);
    m_promiscCallback = cb;
}

bool
Simple5gNetDevice::SupportsSendFrom() const
{
    NS_LOG_FUNCTION(this);
    return true;
}


void
Simple5gNetDevice::SetIsBaseStation(bool isBs)
{
    NS_LOG_FUNCTION(this << isBs);
    m_isBaseStation = isBs;
}

bool
Simple5gNetDevice::IsBaseStation() const
{
    NS_LOG_FUNCTION(this);
    return m_isBaseStation;
}

void
Simple5gNetDevice::SetCsvBasePath(std::string csvBasePath)
{
    NS_LOG_FUNCTION(this << csvBasePath);
    m_csvBasePath = csvBasePath;
}

std::string
Simple5gNetDevice::GetCsvBasePath() const
{
    NS_LOG_FUNCTION(this);
    return m_csvBasePath;
}

DataRate Simple5gNetDevice::GetDataRate() const
{
    NS_LOG_FUNCTION(this);
    return m_bps;
}

double Simple5gNetDevice::GetXBs() const
{
    NS_LOG_FUNCTION(this);
    return m_xbs;
}

double Simple5gNetDevice::GetYBs() const
{
    NS_LOG_FUNCTION(this);
    return m_ybs;
}

void Simple5gNetDevice::SetXBs(double newxbs) 
{
    NS_LOG_FUNCTION(this);
    m_xbs = newxbs;
}

void Simple5gNetDevice::SetYBs(double newybs) 
{
    NS_LOG_FUNCTION(this);
    m_ybs = newybs;
}
//Methods for data rate update based on timestamps
void
Simple5gNetDevice::ScheduleDataRateUpdate(DataRate newDataRate, Time updateTime)
{
    NS_LOG_FUNCTION(this << newDataRate << updateTime);
    // Plan an event to call the private function at the specificed updateTime witht the requested data rate
    Simulator::Schedule(updateTime, &Simple5gNetDevice::DoUpdateDataRate, this, newDataRate);

    NS_LOG_INFO("Scheduled DataRate update for device " << GetAddress()
                                                        << " to " << newDataRate
                                                        << " at simulation time " << updateTime);
}

// Method that executes the data rate update - to be used with ScheduleDataRateUpdate.
void
Simple5gNetDevice::DoUpdateDataRate(DataRate newDataRate)
{
    NS_LOG_FUNCTION(this << newDataRate);
    m_bps = newDataRate; // Aggiorna il membro DataRate

    // Logga l'aggiornamento con il tempo di simulazione corrente
    NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s, DataRate for device "
                           << GetAddress() << " was set to " << newDataRate);
}


void
Simple5gNetDevice::StartPositionBasedDataRateUpdates(double checkIntervalSeconds, double gridResolution)
{
    NS_LOG_FUNCTION(this << checkIntervalSeconds << gridResolution);
    m_gridResolution = gridResolution;
    m_checkInterval = checkIntervalSeconds;

    // DataRate update based on current position 
    DoUpdateDataRateBasedOnPosition(); 

    // Plan the following updates
    m_dataRateUpdateEvent = Simulator::Schedule(Seconds(m_checkInterval),
                                                 &Simple5gNetDevice::DoUpdateDataRateBasedOnPosition,
                                                 this);
    NS_LOG_INFO("Started position-based DataRate updates for device " << GetAddress()
                                                                       << " with interval " << checkIntervalSeconds
                                                                       << "s and grid resolution " << gridResolution << "m.");
}

bool
Simple5gNetDevice::LoadThroughputMatrixFromCsv(const std::string& filePath)
{
    NS_LOG_FUNCTION(this << filePath);
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        NS_LOG_ERROR("Failed to open CSV file: " << filePath);
        return false;
    }

    m_throughputMatrix.clear(); // Clear any existing data

    std::string line;
    int rowCount = 0;
    while (std::getline(file, line))
    {
        std::vector<DataRate> rowData;
        std::stringstream ss(line);
        std::string cell;
        int colCount = 0;
        while (std::getline(ss, cell, ','))
        {
            try
            {
                double tputMbps = std::stod(cell);
                rowData.push_back(DataRate(static_cast<uint64_t>(tputMbps * 1e6))); // Convert Mbps to bps
                colCount++;
            }
            catch (const std::invalid_argument& e)
            {
                NS_LOG_ERROR("Invalid number format in CSV at row " << rowCount << ", col " << colCount << ": " << cell << " (" << e.what() << ")");
                m_throughputMatrix.clear(); // Clear partial data
                return false;
            }
            catch (const std::out_of_range& e)
            {
                NS_LOG_ERROR("Value out of range in CSV at row " << rowCount << ", col " << colCount << ": " << cell << " (" << e.what() << ")");
                m_throughputMatrix.clear(); // Clear partial data
                return false;
            }
        }
        m_throughputMatrix.push_back(rowData);
        rowCount++;
    }

    file.close();

    if (m_throughputMatrix.empty() || m_throughputMatrix[0].empty())
    {
        NS_LOG_ERROR("CSV file is empty or malformed: " << filePath);
        m_throughputMatrix.clear();
        return false;
    }


    NS_LOG_INFO("Successfully loaded throughput matrix from " << filePath << ". Dimensions: "
                                                              << m_throughputMatrix.size() << " rows x "
                                                              << m_throughputMatrix[0].size() << " columns.");
    return true;
}


// --- Private method implementation for position-based DataRate update ---

void
Simple5gNetDevice::DoUpdateDataRateBasedOnPosition()
{
    NS_LOG_FUNCTION(this);
    
    // Ensure that the device is installed on a node and a mobility model is installed
    if (!m_node)
    {
        NS_LOG_WARN("Device " << GetAddress() << " not attached to a node. Cannot update DataRate based on position. Stopping updates.");
        m_dataRateUpdateEvent.Cancel(); // Stop following updates
        return;
    }

    Ptr<MobilityModel> mobility = m_node->GetObject<MobilityModel>();
    if (!mobility)
    {
        NS_LOG_WARN("No MobilityModel found on node " << m_node->GetId() << " for device " << GetAddress() << ". Cannot update DataRate based on position. Stopping updates.");
        m_dataRateUpdateEvent.Cancel(); // Stop following updates
        return;
    }

    
    if (m_throughputMatrix.empty() || (m_throughputMatrix.size() > 0 && m_throughputMatrix[0].empty()))
    {
        
        // Set a default data rate of 1b/s if PRB are not assigned or if the CSV loading failed.
        DataRate newRate = DataRate("1b/s"); //Valore arbitrariamente basso perché 0 significherebbe infinito
        if (newRate != m_bps)
        {
            m_bps = newRate;
            NS_LOG_WARN("At time " << Simulator::Now().GetSeconds() << "s, DataRate for device "
                                        << GetAddress() << " set to " << m_bps
                                        << " because throughput matrix is empty (no PRBs assigned or CSV not loaded).");
        }
        else
        {
             NS_LOG_DEBUG("At time " << Simulator::Now().GetSeconds() << "s, DataRate for device "
                                         << GetAddress() << " remains " << m_bps
                                         << " (throughput matrix empty).");
        }
        //cancella la prox riga

        //m_bps = DataRate("8.192Mbps");
        // Plan a new check based on the specified interval
        m_dataRateUpdateEvent = Simulator::Schedule(Seconds(m_checkInterval),
                                                    &Simple5gNetDevice::DoUpdateDataRateBasedOnPosition,
                                                    this);
        return; 
    }


    Vector currentPos = mobility->GetPosition();

    // 1. Calcola l'indice centrale della matrice (la Base Station è al centro).
    // La divisione intera per 2 fornisce il floor per numeri positivi (es. 201/2 = 100).
    size_t matrixRows = m_throughputMatrix.size();
    size_t matrixCols = m_throughputMatrix[0].size();
    int centerX = static_cast<int>(matrixCols) / 2;
    int centerY = static_cast<int>(matrixRows) / 2;

    // 2. Calcola la posizione relativa alla BS e la converte in un offset della griglia.
    double relativeX = currentPos.x - m_xbs;
    double relativeY = currentPos.y - m_ybs;
    
    // 3. Calcola l'indice finale: (Indice Centrale) + floor((Posizione Relativa) / Risoluzione).
    int gridX = centerX + static_cast<int>(std::floor(relativeX / m_gridResolution));
    int gridY = centerY + static_cast<int>(std::floor(relativeY / m_gridResolution)); 
 
    DataRate newRate = m_bps; // Initialization with current data rate
    
    // Controlla i limiti della matrice. Gli indici validi vanno da 0 a matrixSize-1.
    bool x_in_bounds = (gridX >= 0) && (gridX < (int)matrixCols);
    bool y_in_bounds = (gridY >= 0) && (gridY < (int)matrixRows);

    if (x_in_bounds && y_in_bounds)
    {
        
        newRate = m_throughputMatrix[gridY][gridX]; // Accedi prima alla riga e poi alla colonna (Y, X)
        if(newRate==DataRate("0bps"))
        {
            newRate=DataRate("1bps");
        }
        NS_LOG_DEBUG("Found DataRate " << newRate << " for grid cell (" << gridX << "," << gridY << ").");
    }
    else
    {
        // Out of matrix boundaries (too large OR negative index) - set 0kbps.
        newRate = DataRate("1bps"); 
        
        std::string reason;
        if (gridX < 0 || gridY < 0) {
            // Caso 1: Indice Negativo. La UE è "troppo lontana" in direzione negativa rispetto al centro BS.
            reason = "Position is too far in the negative direction relative to the BS center (negative grid index).";
        } else {
            // Caso 2: Indice Troppo Grande. La UE è "troppo lontana" in direzione positiva rispetto al centro BS.
            reason = "Position is beyond the defined matrix boundaries (too large index).";
        }

        NS_LOG_WARN("Position (" << currentPos.x << ", " << currentPos.y << ") is outside defined matrix area. Grid cell (" << gridX << "," << gridY << "). Setting DataRate to " << newRate << ". Reason: " << reason);
    }

    // Update data rate only if the new data rate is different from the previous to avoid unnecessary reconfigurations
    if (newRate != m_bps)
    {
        m_bps = newRate; 
        NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s, DataRate for device "
                                     << GetAddress() << " updated to " << m_bps
                                     << " based on position (" << currentPos.x << ", " << currentPos.y << ") in grid cell (" << gridX << ", " << gridY << ").");
    }
    else
    {
        NS_LOG_DEBUG("At time " << Simulator::Now().GetSeconds() << "s, DataRate for device "
                                         << GetAddress() << " remains " << m_bps
                                         << " (position (" << currentPos.x << ", " << currentPos.y << ") in grid cell (" << gridX << ", " << gridY << ")).");
    }
    //CANCELLA
    //IL DATARATE IN QUELL ISTANTE E' LETTERALMENTE QUANTO INVIA E QUINDI QUANTO ARRIVA
    //m_bps = DataRate("700Mbps");  
  // NS_LOG_UNCOND("m_bps: " << m_bps); 
    // Plan new check based on the specified interval
    m_dataRateUpdateEvent = Simulator::Schedule(Seconds(m_checkInterval),
                                                &Simple5gNetDevice::DoUpdateDataRateBasedOnPosition,
                                                this);
}

uint32_t
Simple5gNetDevice::GetAssignedPrbs() const
{
    NS_LOG_FUNCTION(this);
    return m_assignedPrbs;
}

void
Simple5gNetDevice::SetAssignedPrbs(uint32_t prbs)
{
    NS_LOG_FUNCTION(this << prbs);
    m_assignedPrbs = prbs;
}

void
Simple5gNetDevice::CalculateAndDistributePrbs()
{
   
    NS_LOG_FUNCTION(this);
    if (!m_isBaseStation)
    {
        NS_LOG_INFO("The specified device is not a base station. Aborting operation");
        return;
    }
    if (m_channel == nullptr)
    {
        NS_LOG_ERROR("Channel is null, cannot calculate PRBs.");
        return;
    }

    uint32_t totalPrbs = 100;
    uint32_t connectedUeCount = m_connectedUeAddresses.size();
    uint32_t prbsPerUe = (connectedUeCount > 0) ? totalPrbs / connectedUeCount : 0;

    NS_LOG_INFO("Number of connected UEs: " << connectedUeCount << ". Calculated PRB for each UE: " << prbsPerUe);

    // Step 1: Set all PRBs to 0.
    for (uint32_t i = 0; i < m_channel->GetNDevices(); ++i)
    {
        Ptr<Simple5gNetDevice> device = DynamicCast<Simple5gNetDevice>(m_channel->GetDevice(i));
        if (device != nullptr && !device->IsBaseStation())
        {
            device->SetAssignedPrbs(0);
        }
    }
  

    // Step 2: Distribute new PRB only to currently connected devices.
    for (const auto& ueAddr : m_connectedUeAddresses)
    {
        // Find corresponding device on channel
        for (uint32_t i = 0; i < m_channel->GetNDevices(); ++i)
        {
            
            Ptr<Simple5gNetDevice> device = DynamicCast<Simple5gNetDevice>(m_channel->GetDevice(i));
            
            Mac48Address deviceAddr = Mac48Address::ConvertFrom(device->GetAddress());
                         
            if (device != nullptr && deviceAddr == ueAddr)
            {
                device->SetAssignedPrbs(prbsPerUe);
                NS_LOG_INFO("Assigned " << prbsPerUe << " PRBs to UE " << device->GetAddress());
                 
                 if (!device->GetCsvBasePath().empty() && prbsPerUe > 0)
                {
                    std::string filePath = device->GetCsvBasePath() + "/" + std::to_string(prbsPerUe) + ".csv";
                    if (device->LoadThroughputMatrixFromCsv(filePath))
                    {
                        NS_LOG_INFO("Throughput matrix successfully loaded from " << filePath << " for UE " << device->GetAddress());
                    }
                    else
                    {
                        NS_LOG_ERROR("Throughput matrix loading failed from " << filePath << " for UE " << device->GetAddress());
                    }
                }else{
                   
                }

                break; // Go to next UE
            }
            
        }
    }
    //NS_LOG_INFO("--- FINE CALCOLO PRB ---");
}
} // namespace ns3
