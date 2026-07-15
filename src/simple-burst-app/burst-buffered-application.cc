#include "burst-buffered-application.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BurstBufferedApplication");

NS_OBJECT_ENSURE_REGISTERED (BurstBufferedApplication);

TypeId
BurstBufferedApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BurstBufferedApplication")
    .SetParent<Application> ()
    .AddConstructor<BurstBufferedApplication> ()
    .AddAttribute ("PacketSize", "Amount of data to generate at each step (bytes)",
                   UintegerValue (100),
                   MakeUintegerAccessor (&BurstBufferedApplication::m_packetSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("GenerationInterval", "Time interval between data generation events",
                   TimeValue (Seconds (2.0)),
                   MakeTimeAccessor (&BurstBufferedApplication::m_genInterval),
                   MakeTimeChecker ())
    .AddAttribute ("BurstInterval", "Time interval between burst transmission events",
                   TimeValue (Seconds (4.0)),
                   MakeTimeAccessor (&BurstBufferedApplication::m_burstInterval),
                   MakeTimeChecker ())
  ;
  return tid;
}

BurstBufferedApplication::BurstBufferedApplication ()
  : m_socket (0),
    m_peerPort (0),
    m_running (false),
    m_generatedBytes (0),
    m_sentBytes (0),
    m_currentBufferBytes (0)
{
}

BurstBufferedApplication::~BurstBufferedApplication ()
{
  m_socket = 0;
}

void
BurstBufferedApplication::Setup (Address destAddr, uint16_t port,
                                 uint32_t packetSize, Time genInterval,
                                 Time burstInterval,
                                 Ptr<NetDevice> device, Ipv4Address localAddr)
{
  m_peerAddress = destAddr;
  m_peerPort = port;
  m_packetSize = packetSize;
  m_genInterval = genInterval;
  m_burstInterval = burstInterval;
  m_device = device;
  m_localAddress = localAddr;
}

void
BurstBufferedApplication::StartApplication (void)
{
  m_running = true;

  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());

      // 1. Bind al Device (opzionale)
      if (m_device)
        {
          m_socket->BindToNetDevice (m_device);
        //  NS_LOG_UNCOND("Bound to device with address: " << m_device->GetAddress());
        }

      // 2. Bind all'indirizzo locale
      uint16_t port = rand() % 65535;
      InetSocketAddress localSocket = InetSocketAddress (m_localAddress, port);
    //NS_LOG_UNCOND("IP: " << localSocket.GetIpv4() << " Port: " << localSocket.GetPort());
      if (m_socket->Bind (localSocket) == -1)
        {
          NS_LOG_WARN ("Bind failed");
        }
        // 3. Connect (gestione smart Address vs InetSocketAddress)
      if (InetSocketAddress::IsMatchingType (m_peerAddress))
        {
          m_socket->Connect (m_peerAddress);
        }
      else
        {
          m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_peerAddress), m_peerPort));
        }
    }

  // Avvia i due timer indipendenti
  // Timer Generazione
  m_genEvent = Simulator::Schedule (m_genInterval, &BurstBufferedApplication::GenerateStep, this);
  
  // Timer Burst (Invio) - parte dopo un intervallo
    Simulator::Schedule(m_genInterval, &BurstBufferedApplication::WarmUpSend, this);
  m_burstEvent = Simulator::Schedule (m_burstInterval, &BurstBufferedApplication::BurstStep, this);
}

void
BurstBufferedApplication::StopApplication (void)
{
  m_running = false;
  if (m_socket)
    {
      m_socket->Close ();
    }
  Simulator::Cancel (m_genEvent);
  Simulator::Cancel (m_burstEvent);
}

// sporca la rete inviando un pacchetto prima del primo burst, altrimenti l'intero primo burst verrebbe perso per il "slow start" del socket
void
BurstBufferedApplication::WarmUpSend (void)
{
  Ptr<Packet> p = Create<Packet>(m_packetSize);
  m_socket->Send(p);
}
void
BurstBufferedApplication::GenerateStep (void)
{
  if (!m_running) return;

  // Crea pacchetto e accumula
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_buffer.push (packet);
  
  m_generatedBytes += m_packetSize;
  m_currentBufferBytes += m_packetSize;

  NS_LOG_INFO ("Time " << Simulator::Now ().GetSeconds () 
               << "s: GENERATED " << m_packetSize << " bytes. Buffer size: " << m_buffer.size ());

  // Rischedula prossima generazione
  m_genEvent = Simulator::Schedule (m_genInterval, &BurstBufferedApplication::GenerateStep, this);
}

void
BurstBufferedApplication::BurstStep (void)
{
  

  if (!m_running) return;

  NS_LOG_INFO ("Time " << Simulator::Now ().GetSeconds () 
               << "s: BURST START. Flushing " << m_buffer.size () << " packets.");

  // LOOP DI INVIO ISTANTANEO
  // Svuota tutto il buffer in questo istante di simulazione

  

  while (!m_buffer.empty ())
    {
      Ptr<Packet> p = m_buffer.front ();
      uint32_t pSize = p->GetSize();

    

      // Tenta l'invio
      if (m_socket->Send (p) >= 0)
        {
           
          m_sentBytes += pSize;
          m_buffer.pop (); // Rimuovi solo se accettato dal socket
          m_currentBufferBytes -= pSize;
        }
      else 
        {  NS_LOG_UNCOND("DROP...");


          // Socket pieno (coda fisica piena)
          NS_LOG_WARN ("Physical Queue Full! Dropping packet from application buffer.");
         // NS_LOG_UNCOND("Pacchetto perso dal bufferedapp");
          m_buffer.pop (); 
          m_currentBufferBytes -= pSize;
        }
    }

  // Rischedula prossimo burst
  m_burstEvent = Simulator::Schedule (m_burstInterval, &BurstBufferedApplication::BurstStep, this);
}

} // namespace ns3