#include "simple-sat-net-device.h"

#include "ns3/error-model.h"
#include "ns3/queue.h"
#include "simple-sat-channel.h"

#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/tag.h"
#include "ns3/trace-source-accessor.h"


#include <fstream>
#include <string>
#include <sstream>
#include <random>
#include <chrono>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SimpleSatNetDevice");

/**
 * @brief SimpleSatNetDevice tag to store source, destination and protocol of each packet.
 */
class SimpleSatTag : public Tag
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

NS_OBJECT_ENSURE_REGISTERED(SimpleSatTag);

TypeId
SimpleSatTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SimpleSatTag")
                            .SetParent<Tag>()
                            .SetGroupName("Network")
                            .AddConstructor<SimpleSatTag>();
    return tid;
}

TypeId
SimpleSatTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
SimpleSatTag::GetSerializedSize() const
{
    return 8 + 8 + 2;
}

void
SimpleSatTag::Serialize(TagBuffer i) const
{
    uint8_t mac[6];
    m_src.CopyTo(mac);
    i.Write(mac, 6);
    m_dst.CopyTo(mac);
    i.Write(mac, 6);
    i.WriteU16(m_protocolNumber);
}

void
SimpleSatTag::Deserialize(TagBuffer i)
{
    uint8_t mac[6];
    i.Read(mac, 6);
    m_src.CopyFrom(mac);
    i.Read(mac, 6);
    m_dst.CopyFrom(mac);
    m_protocolNumber = i.ReadU16();
}

void
SimpleSatTag::SetSrc(Mac48Address src)
{
    m_src = src;
}

Mac48Address
SimpleSatTag::GetSrc() const
{
    return m_src;
}

void
SimpleSatTag::SetDst(Mac48Address dst)
{
    m_dst = dst;
}

Mac48Address
SimpleSatTag::GetDst() const
{
    return m_dst;
}

void
SimpleSatTag::SetProto(uint16_t proto)
{
    m_protocolNumber = proto;
}

uint16_t
SimpleSatTag::GetProto() const
{
    return m_protocolNumber;
}

void
SimpleSatTag::Print(std::ostream& os) const
{
    os << "src=" << m_src << " dst=" << m_dst << " proto=" << m_protocolNumber;
}

NS_OBJECT_ENSURE_REGISTERED(SimpleSatNetDevice);

TypeId
SimpleSatNetDevice::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::SimpleSatNetDevice")
            .SetParent<NetDevice>()
            .SetGroupName("Network")
            .AddConstructor<SimpleSatNetDevice>()
            .AddAttribute("ReceiveErrorModel",
                          "The receiver error model used to simulate packet loss",
                          PointerValue(),
                          MakePointerAccessor(&SimpleSatNetDevice::m_receiveErrorModel),
                          MakePointerChecker<ErrorModel>())
            .AddAttribute("PointToPointMode",
                          "The device is configured in Point to Point mode",
                          BooleanValue(false),
                          MakeBooleanAccessor(&SimpleSatNetDevice::m_pointToPointMode),
                          MakeBooleanChecker())
            .AddAttribute("TxQueue",
                          "A queue to use as the transmit queue in the device.",
                          StringValue("ns3::DropTailQueue<Packet>"),
                          MakePointerAccessor(&SimpleSatNetDevice::m_queue),
                          MakePointerChecker<Queue<Packet>>())
            .AddAttribute("DataRate",
                          "The default data rate for point to point links. Zero means infinite",
                          DataRateValue(DataRate("4Mb/s")),
                          MakeDataRateAccessor(&SimpleSatNetDevice::m_bps),
                          MakeDataRateChecker())
            .AddTraceSource("PhyRxDrop",
                            "Trace source indicating a packet has been dropped "
                            "by the device during reception",
                            MakeTraceSourceAccessor(&SimpleSatNetDevice::m_phyRxDropTrace),
                            "ns3::Packet::TracedCallback");
    return tid;
}
//è IL DATARATE DEL DISPOSITIVO A NON FAR ARRIVARE TUTTI I BIT (LI PRENDE SEMPLICEMENTE DAL CSV)
SimpleSatNetDevice::SimpleSatNetDevice()
    : m_channel(nullptr),
      m_node(nullptr),
      m_mtu(0xffff),
      m_ifIndex(0),
      m_linkUp(false)
      {
    NS_LOG_FUNCTION(this);

    SetAddress(Mac48Address::Allocate()); //assegna un indirizzo MAC univoco (la variabile m_address)
    ScheduleDataRateUpdatesFromCsv(); //simula un throughput variabile nel tempo, basato su un file csv di input (posizioni e datarate associati)
}

void
SimpleSatNetDevice::Receive(Ptr<Packet> packet, uint16_t protocol, Mac48Address to, Mac48Address from)
{
    NS_LOG_FUNCTION(this << packet << protocol << to << from);
    NetDevice::PacketType packetType;

    //non viene mai richiamata sta funzione
    if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt(packet))
    {
   // NS_LOG_UNCOND("Packet dropped ... at t=" << Simulator::Now().GetSeconds());       
 m_phyRxDropTrace(packet);
        return;
    }

    if (to == m_address) //il destinatario è l'indirizzo stesso di questa classe (oggetto)
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
        //entra sempre qua
        m_rxCallback(this, packet, protocol, from);
    }
    else
    {
        //NS_LOG_UNCOND("Packet OTHERHOST DROPPED at " << GetAddress() << " (destined to " << to << ") size=" << packet->GetSize());
        m_phyRxDropTrace(packet);
    }

    if (!m_promiscCallback.IsNull())
    {
        m_promiscCallback(this, packet, protocol, from, to, packetType);
    }
}

void
SimpleSatNetDevice::SetChannel(Ptr<SimpleSatChannel> channel)
{
    NS_LOG_FUNCTION(this << channel);
    m_channel = channel;
    m_channel->Add(this);
    m_linkUp = true;
    m_linkChangeCallbacks();
}

Ptr<Queue<Packet>>
SimpleSatNetDevice::GetQueue() const
{
    NS_LOG_FUNCTION(this);
    return m_queue;
}

void
SimpleSatNetDevice::SetQueue(Ptr<Queue<Packet>> q)
{
    NS_LOG_FUNCTION(this << q);
    m_queue = q;
}

void
SimpleSatNetDevice::SetReceiveErrorModel(Ptr<ErrorModel> em)
{
    NS_LOG_FUNCTION(this << em);
    m_receiveErrorModel = em;
}

void
SimpleSatNetDevice::SetIfIndex(const uint32_t index)
{
    NS_LOG_FUNCTION(this << index);
    m_ifIndex = index;
}

DataRate SimpleSatNetDevice::GetDataRate() const
{
    NS_LOG_FUNCTION(this);
    return m_bps;
}

uint32_t
SimpleSatNetDevice::GetIfIndex() const
{
    NS_LOG_FUNCTION(this);
    return m_ifIndex;
}

Ptr<Channel>
SimpleSatNetDevice::GetChannel() const
{
    NS_LOG_FUNCTION(this);
    return m_channel;
}

void
SimpleSatNetDevice::SetAddress(Address address)
{
    NS_LOG_FUNCTION(this << address);
    m_address = Mac48Address::ConvertFrom(address);
}

Address
SimpleSatNetDevice::GetAddress() const
{
    //
    // Implicit conversion from Mac48Address to Address
    //
    NS_LOG_FUNCTION(this);
    return m_address;
}

bool
SimpleSatNetDevice::SetMtu(const uint16_t mtu)
{
    NS_LOG_FUNCTION(this << mtu);
    m_mtu = mtu;
    return true;
}

uint16_t
SimpleSatNetDevice::GetMtu() const
{
    NS_LOG_FUNCTION(this);
    return m_mtu;
}

bool
SimpleSatNetDevice::IsLinkUp() const
{
    NS_LOG_FUNCTION(this);
    return m_linkUp;
}

void
SimpleSatNetDevice::AddLinkChangeCallback(Callback<void> callback)
{
    NS_LOG_FUNCTION(this << &callback);
    m_linkChangeCallbacks.ConnectWithoutContext(callback);
}

bool
SimpleSatNetDevice::IsBroadcast() const
{
    NS_LOG_FUNCTION(this);
    return !m_pointToPointMode;
}

Address
SimpleSatNetDevice::GetBroadcast() const
{
    NS_LOG_FUNCTION(this);
    return Mac48Address::GetBroadcast();
}

bool
SimpleSatNetDevice::IsMulticast() const
{
    NS_LOG_FUNCTION(this);
    return !m_pointToPointMode;
}

Address
SimpleSatNetDevice::GetMulticast(Ipv4Address multicastGroup) const
{
    NS_LOG_FUNCTION(this << multicastGroup);
    return Mac48Address::GetMulticast(multicastGroup);
}

Address
SimpleSatNetDevice::GetMulticast(Ipv6Address addr) const
{
    NS_LOG_FUNCTION(this << addr);
    return Mac48Address::GetMulticast(addr);
}

bool
SimpleSatNetDevice::IsPointToPoint() const
{
    NS_LOG_FUNCTION(this);
    return m_pointToPointMode;
}

bool
SimpleSatNetDevice::IsBridge() const
{
    NS_LOG_FUNCTION(this);
    return false;
}

//NON VIENE MAI RICHIAMATA E DI CONSEGUENZA TUTTE LE ALTRE NON PARTONO
bool
SimpleSatNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
    //NS_LOG_UNCOND("send sat");
    NS_LOG_FUNCTION(this << packet << dest << protocolNumber);

    return SendFrom(packet, m_address, dest, protocolNumber);
}

bool
SimpleSatNetDevice::SendFrom(Ptr<Packet> p,
                          const Address& source,
                          const Address& dest,
                          uint16_t protocolNumber)
{

    NS_LOG_FUNCTION(this << p << source << dest << protocolNumber);
    if (p->GetSize() > GetMtu())
    {
       // NS_LOG_UNCOND("Packet size " << p->GetSize() << " exceeds MTU of " << GetMtu() << " bytes, dropping packet");
        return false;
    }

    Mac48Address to = Mac48Address::ConvertFrom(dest);
    Mac48Address from = Mac48Address::ConvertFrom(source);

    SimpleSatTag tag;
    tag.SetSrc(from);
    tag.SetDst(to);
    tag.SetProto(protocolNumber);

    p->AddPacketTag(tag);

    if (m_queue->Enqueue(p)) //prova ad aggiungere il pacchetto
    {
        if (m_queue->GetNPackets() == 1 && !FinishTransmissionEvent.IsPending())
        {
            StartTransmission();
        }
        return true;
    }
  //  NS_LOG_UNCOND("Packet dropped by queue at transmission");
    return false;
}

void
SimpleSatNetDevice::StartTransmission()
{

    if (m_queue->GetNPackets() == 0)
    {
        return;
    }
    NS_ASSERT_MSG(!FinishTransmissionEvent.IsPending(),
                  "Tried to transmit a packet while another transmission was in progress");
    Ptr<Packet> packet = m_queue->Dequeue();

    /**
     * SimpleSatChannel will deliver the packet to the far end(s) of the link as soon as Send is called
     * (or after its fixed delay, if one is configured). So we have to handle the rate of the link
     * here, which we do by scheduling FinishTransmission (packetSize / linkRate) time in the
     * future. While that event is running, the transmit path of this NetDevice is busy, so we can't
     * send other packets.
     *
     * SimpleSatChannel doesn't have a locking mechanism, and doesn't check for collisions, so there's
     * nothing we need to do with the channel until the transmission has "completed" from the
     * perspective of this NetDevice.
     */
    Time txTime = Time(0);
    if (m_bps > DataRate(0))
    {
        //IL DATA RATE VARIA NEL TEMPO, VIENE IN OGNI ISTANTE PRESO SEMPLICEMENTE DAL FILE CSV, E QUINDI IN BASE A QUANTO E' GRANDE IL PACCHETTO
        //CALCOLA IL TEMPO PER INVIARLO (Datarate = 8192 bps, packetSize = 1024 bytes =  8192 bit -> txTime = 1s)
        //per tutta la durata dell'invio di un pacchetto non viene considerato il cambiamento di datarate, ma solo quello che c'è al momento dell'inizio dell'invio (quindi se durante l'invio di un pacchetto da 1024 byte la datarate cambia da 8192 bps a 16384 bps, il tempo di invio rimane 1s e non diventa 0.5s)
        txTime = m_bps.CalculateBytesTxTime(packet->GetSize());
    }

    FinishTransmissionEvent =
        Simulator::Schedule(txTime, &SimpleSatNetDevice::FinishTransmission, this, packet);
}

void
SimpleSatNetDevice::FinishTransmission(Ptr<Packet> packet)
{
      //  NS_LOG_UNCOND("Device " << m_address << " transmitted packet " << packet->GetSize() << " bytes");

    NS_LOG_FUNCTION(this);
    SimpleSatTag tag;
    packet->RemovePacketTag(tag);

    Mac48Address src = tag.GetSrc();
    Mac48Address dst = tag.GetDst();
    uint16_t proto = tag.GetProto();

    m_channel->Send(packet, proto, dst, src, this); //invio effettivo del pacchetto (era implementato nel canale farà semplicemente dst.receive(packet, proto, dst, src) dopo un certo delay)

    StartTransmission();
}

Ptr<Node>
SimpleSatNetDevice::GetNode() const
{
    NS_LOG_FUNCTION(this);
    return m_node;
}

void
SimpleSatNetDevice::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    m_node = node;
}

bool
SimpleSatNetDevice::NeedsArp() const
{
    NS_LOG_FUNCTION(this);
    return !m_pointToPointMode;
}

void
SimpleSatNetDevice::SetReceiveCallback(NetDevice::ReceiveCallback cb)
{
    NS_LOG_FUNCTION(this << &cb);
    m_rxCallback = cb;
}

void
SimpleSatNetDevice::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_channel = nullptr;
    m_node = nullptr;
    m_receiveErrorModel = nullptr;
    m_queue->Dispose();
    if (FinishTransmissionEvent.IsPending())
    {
        FinishTransmissionEvent.Cancel();
    }
    NetDevice::DoDispose();
}

void
SimpleSatNetDevice::SetPromiscReceiveCallback(PromiscReceiveCallback cb)
{
    NS_LOG_FUNCTION(this << &cb);
    m_promiscCallback = cb;
}

bool
SimpleSatNetDevice::SupportsSendFrom() const
{
    NS_LOG_FUNCTION(this);
    return true;
}


void
SimpleSatNetDevice::ScheduleDataRateUpdate(DataRate newDataRate, Time updateTime)
{
    NS_LOG_FUNCTION(this << newDataRate << updateTime);
    // Plan an event to call the update method at the specified time.
    Simulator::Schedule(updateTime, &SimpleSatNetDevice::DoUpdateDataRate, this, newDataRate);

    /*NS_LOG_INFO("Scheduled DataRate update for device " << GetAddress()
                                                        << " to " << newDataRate
                                                        << " at simulation time " << updateTime);*/
}

// Performing data rate update
void
SimpleSatNetDevice::DoUpdateDataRate(DataRate newDataRate)
{
    NS_LOG_FUNCTION(this << newDataRate);
    m_bps = newDataRate; 
   //CANCELLA o cascella se preferisci
       // m_bps = DataRate("10.192Mbps");   

    NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s, DataRate for device "
                           << GetAddress() << " was set to " << newDataRate);
}


void
SimpleSatNetDevice::ScheduleDataRateUpdatesFromCsv()
{
    // Base path for the CSV files
    const std::string basePath = "/home/ubuntu/ns-3-dev/scratch/tn-ntn-ns3/moduli/resources/sat_tput_30min/";
    const int maxFileNumber = 315;
    // Random number selection for random trace file
    // Generate a random number between 1 and maxFileNumber
    std::mt19937_64 rng;
    // seed with a high-resolution clock
    uint64_t timeSeed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::seed_seq seed_sequence{uint32_t(timeSeed & 0xffffffff), uint32_t(timeSeed>>32)};
    rng.seed(seed_sequence);
    std::uniform_int_distribution<int> distrib(1, maxFileNumber);
    int randomNumber = distrib(rng);

    // Construct the full filename
    std::stringstream ss;
    ss << basePath << randomNumber << ".csv";
    const std::string filename = ss.str();

    NS_LOG_FUNCTION(this << "Selected filename: " << filename);
    
    std::ifstream file(filename);
    if (!file.is_open())
    {
        NS_LOG_ERROR("Failed to open CSV file: " << filename);
        return;
    }

    std::string line;
    // Skip the header line
    std::getline(file, line);

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string timestamp_str, datarate_str;

        if (std::getline(ss, timestamp_str, ',') && std::getline(ss, datarate_str, ','))
        {
            try
            {
                double timestamp = std::stod(timestamp_str);
                double datarate_bps = std::stod(datarate_str);
                
                Time updateTime = Seconds(timestamp);
                DataRate newDataRate = DataRate(datarate_bps);
                
                // Schedule the update
                this->ScheduleDataRateUpdate(newDataRate, updateTime);
            
            }
            catch (const std::exception& e)
            {
                NS_LOG_WARN("Skipping malformed line in CSV: " << line << " - " << e.what());
            }
        }
    }
    NS_LOG_INFO("Selected filename: " << filename << " for device " << GetAddress());

    file.close();
}


} // namespace ns3
