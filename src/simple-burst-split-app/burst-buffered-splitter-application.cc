#include "burst-buffered-splitter-application.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"

#include "ns3/simple-5g-net-device.h"
#include "ns3/simple-5g-net-device-helper.h"
#include "ns3/simple-sat-net-device-helper.h"
#include "ns3/simple-sat-net-device.h"
#include <random>
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BurstBufferedSplitterApplication");

NS_OBJECT_ENSURE_REGISTERED (BurstBufferedSplitterApplication);

TypeId
BurstBufferedSplitterApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BurstBufferedSplitterApplication")
    .SetParent<Application> ()
    .AddConstructor<BurstBufferedSplitterApplication> ()
    .AddAttribute ("PacketSize", "Amount of data to generate at each step (bytes)",
                   UintegerValue (100),
                   MakeUintegerAccessor (&BurstBufferedSplitterApplication::m_packetSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("GenerationInterval", "Time interval between data generation events",
                   TimeValue (Seconds (2.0)),
                   MakeTimeAccessor (&BurstBufferedSplitterApplication::m_genInterval),
                   MakeTimeChecker ())
    .AddAttribute ("BurstInterval", "Time interval between burst transmission events",
                   TimeValue (Seconds (4.0)),
                   MakeTimeAccessor (&BurstBufferedSplitterApplication::m_burstInterval),
                   MakeTimeChecker ())
    .AddAttribute ("MaxDelay", "Maximum acceptable delay for packets in the buffer",
                   TimeValue (Seconds (0.1)),
                   MakeTimeAccessor (&BurstBufferedSplitterApplication::m_maxDelay),
                   MakeTimeChecker ())
  ;
  return tid;
}

BurstBufferedSplitterApplication::BurstBufferedSplitterApplication ()
  : m_socketTN (0),
    m_socketNTN (0),
    m_peerPortTN (0),
    m_peerPortNTN (0),
    m_running (false),
    m_generatedBytes (0),
    m_sentBytesTN (0),
    m_sentBytesNTN (0),
    m_currentBufferBytes (0)
{
}

BurstBufferedSplitterApplication::~BurstBufferedSplitterApplication ()
{
  m_socketTN = 0;
  m_socketNTN = 0;
}

void
BurstBufferedSplitterApplication::Setup (Address destAddrTN, Address destAddrNTN, uint16_t portTN, uint16_t portNTN, uint32_t packetSize,
                                         Time genInterval,
                                         Time burstInterval, Time maxDelay,
                                         Ptr<NetDevice> deviceTN, Ptr<NetDevice> deviceNTN, Ipv4Address localAddrTN, Ipv4Address localAddrNTN)
                                        
{
  m_peerAddressTN = destAddrTN;
  m_peerAddressNTN = destAddrNTN;
  m_peerPortTN = portTN;
  m_peerPortNTN = portNTN;
  m_packetSize = packetSize;
  m_genInterval = genInterval;
  m_burstInterval = burstInterval;
  m_deviceTN = deviceTN;
  m_maxDelay = maxDelay;
  m_deviceNTN = deviceNTN;
  m_localAddressTN = localAddrTN;
  m_localAddressNTN = localAddrNTN;
}


void
BurstBufferedSplitterApplication::StartApplication (void)
{
  
  m_running = true;

  if (!m_socketTN)
    {
      m_socketTN = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());

      if (m_deviceTN)
        {
          m_socketTN->BindToNetDevice (m_deviceTN);
        }

      InetSocketAddress localSocketTN = InetSocketAddress (m_localAddressTN, 0);
      if (m_socketTN->Bind (localSocketTN) == -1)
        {
          NS_LOG_WARN ("Bind failed");
        }

      if (InetSocketAddress::IsMatchingType (m_peerAddressTN))
        {
          m_socketTN->Connect (m_peerAddressTN);
        }
      else
        {
          m_socketTN->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_peerAddressTN), m_peerPortTN));
        }
    }
  if (!m_socketNTN)
    {
      m_socketNTN = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());

      if (m_deviceNTN)
        {
          m_socketNTN->BindToNetDevice (m_deviceNTN);
        }

      InetSocketAddress localSocketNTN = InetSocketAddress (m_localAddressNTN, 0);
      if (m_socketNTN->Bind (localSocketNTN) == -1)
        {
          NS_LOG_WARN ("Bind failed");
        }

      if (InetSocketAddress::IsMatchingType (m_peerAddressNTN))
        {
          m_socketNTN->Connect (m_peerAddressNTN);
        }
      else
        {
          m_socketNTN->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_peerAddressNTN), m_peerPortNTN));
        }
    }
    InitializeController();
  m_genEvent = Simulator::Schedule (m_genInterval, &BurstBufferedSplitterApplication::GenerateStep, this);
  //il primo burst da problemi non arrivano i pacchetti, quindi inviamo un pacchetto di prova per sporcare la rete
  Simulator::Schedule(m_genInterval, &BurstBufferedSplitterApplication::WarmUpSend, this);
  m_burstEvent = Simulator::Schedule (m_burstInterval, &BurstBufferedSplitterApplication::BurstStep, this);
}



void
BurstBufferedSplitterApplication::StopApplication (void)
{
  m_running = false;
  if (m_socketTN)
    {
      m_socketTN->Close ();
    }
  if (m_socketNTN)
    {
      m_socketNTN->Close ();
    }
  Simulator::Cancel (m_genEvent);
  Simulator::Cancel (m_burstEvent);
}

void BurstBufferedSplitterApplication::WarmUpSend ()
{
  Ptr<Packet> p = Create<Packet>(m_packetSize);
  m_socketTN->Send(p);
    m_socketNTN->Send(p);

}



void BurstBufferedSplitterApplication::InitializeController(void)
{
  //B grande sistema più reattivo ai cambiamenti di q, B piccolo sistema conservativo più smoothing
  double B = 0.5; //quanto il ritardo medio di accodamento vale rispetto il ritardo massimo
  uint32_t M = m_maxDelay.GetSeconds()/ m_burstInterval.GetSeconds(); //numero di coefficienti bi
  M = std::max(M, 2U); //deve essere almeno 2 per avere un filtro di smoothing, se M=1 il filtro non ha memoria e o(k) = q(k-1) sempre
  //vettore dei coefficienti
  m_coefficients.resize(M);
  //VETTORI PER MEMORIZZARE LA STORIA DI q(k) E o(k)
  m_qHistory.assign(M, 0.0);
  m_txHistory.assign(M, 0.0);

  // b0 = 1
  m_coefficients[0] = 1.0;
  double sum = 0.0;
  // parametro di decadimento. coefficienti bi del tipo alpha^j alpha <1  per dare più peso ai valori recenti
  double alpha = 0.8;
  for (uint32_t j = 1; j < M; ++j)
    {
      sum+= std::pow(alpha, j);
    }

  // normalizzazione:
  // sum(bj) = B*M - 1
  double K = (B*M - 1.0) / sum;
  for(uint32_t j = 1; j < M; ++j)
    {
      m_coefficients[j] = K * std::pow(alpha, j);
    }
}

void BurstBufferedSplitterApplication::ComputeTxBuffer(void){
  /*concettualmente all'istante k in cui parte questa funzione
  abbiamo q(k-1) e d(k), dobbiamo calcolare o(k) e solo dopo averlo trasmesso si ottiene q(k)*/
  uint32_t M = m_qHistory.size();   
  double q = q_buffer.size()*m_packetSize*8; //q(k-1) in bit. 
  //ricordiamo che q(k) = q(k-1) + d(k) - o(k)
  
  //formula da implementare o(k) = q(k-1) - sum(bi [Dq(k-j) + o(k-j)])

  //1 burst, le storie sono tutte vuote
  if (tx_buffer.empty())
  {
    // pongo tutti a q(k) così da avere Dq(k-1) = q(k-1) - q(k-2) = 0 al primo burst, in modo che o(k) = q(k-1) al primo burst
    std::fill(m_qHistory.begin(), m_qHistory.end(), q);
  }
  //calcolo effettivo o(k)
  // M = memoria del filtro = size-1

  // q(k-1) è m_qHistory[0]
  double tx = m_qHistory[0]; //tx è o(k) che inizialmente è q(k-1) e poi viene corretto dagli altri termini della formula

  // Somma ricorsiva della formula (14) la formula deve arrivare a (q(k-M+1)-q(k-M)) e o(k-M) che sono m_qHistory[M-1] e m_txHistory[M-1]
  for (uint32_t j = 1; j < M; ++j)
    {
      double deltaQ = m_qHistory[j-1] - m_qHistory[j]; // q(k-j) - q(k-j-1)
      tx -= m_coefficients[j] * (deltaQ + m_txHistory[j-1]);
    }
  //aggiorno storia
  for (uint32_t j = M-1; j > 0; --j)
    {
        m_qHistory[j] = m_qHistory[j-1]; // q(k-j) diventa q(k-j-1), implementato è al contrario cioè m[0] è q(k-1) e m[1] è q(k-2) e così via, quindi m[M-1] = q[k - M] ; m[j] = q(k-j-1)  
        m_txHistory[j] = m_txHistory[j-1];
    }

    m_qHistory[0] = q; //salvi q(k-1); 
    m_txHistory[0] = tx;  // o(k) in bit che verrà usato nel burst successivo come o(k-1)
  //svuota q_buffer in tx_buffer
  int numPacketsToTx = static_cast<int>(std::round(tx / (m_packetSize * 8))); // numero di pacchetti da trasmettere in questo burst
  while (numPacketsToTx > 0 && !q_buffer.empty())
  {
    Ptr<Packet> p = q_buffer.front();
    q_buffer.pop();
    tx_buffer.push(p);
    numPacketsToTx--;
  }
}

uint32_t BurstBufferedSplitterApplication::DecisionSteering(void)
{
  Ptr<Simple5gNetDevice> devTN = DynamicCast<Simple5gNetDevice>(m_deviceTN);
  Ptr<SimpleSatNetDevice> devNTN = DynamicCast<SimpleSatNetDevice>(m_deviceNTN);

  static std::random_device rd;
  static std::mt19937 gen(rd());

  //RITARDI DI PROPAGAZIONE SIMULATI CON DISTRIBUZIONE GAUSSIANA
  // Media dei ritardi
  double meanTN = 0.001;
  double meanNTN = 0.02;
  // Deviazione standard = 15% della media
  double stdTN = meanTN * 0.15;
  double stdNTN = meanNTN * 0.15;
  std::normal_distribution<double> distTN(meanTN, stdTN);
  std::normal_distribution<double> distNTN(meanNTN, stdNTN);

  double tTN = distTN(gen);
  double tNTN = distNTN(gen);
  //EVITI RITARDI NEGATIVI
  tTN = std::max(0.0, tTN);
  tNTN = std::max(0.0, tNTN);



  uint32_t n = tx_buffer.size();
  double total = n*m_packetSize*8;  //bit per ora è q(k) deve diventare o(k)
  double rTN = devTN->GetDataRate().GetBitRate(); //datarate TN in bps
  double rNTN = devNTN->GetDataRate().GetBitRate(); //datarate NTN in bps

    /*total è la quantità di dati da trasmettere in questo burst, è o(k) in bit, 
    ma se l'intervallo di generazione è alto (vengono generati pochi dati) significa che total/rNTN e total/rTN tendono a 0 per cui a tende a infinito e si prende 1*/
    //quindi solo con intervalli di generazione molto piccoli si splitta in modo più bilanciato (dipende anche dal timeslot, cioè tempo di burst)
  double a = ((total/rNTN) -tTN + tNTN)/(total/rTN + total/rNTN);
  a = std::min(1.0,a);
  uint32_t tnTarget = static_cast<uint32_t>(n * a); // numero di pacchetti da inviare via TN
  steeringValues.push_back(a); 
  return tnTarget;
} 

void
BurstBufferedSplitterApplication::GenerateStep (void)
{
  if (!m_running) return;

  // Crea pacchetto e accumula
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  q_buffer.push (packet);
  
  m_generatedBytes += m_packetSize;
  m_currentBufferBytes += m_packetSize;

  NS_LOG_INFO ("Time " << Simulator::Now ().GetSeconds () 
               << "s: GENERATED " << m_packetSize << " bytes. Buffer size: " << q_buffer.size ());

  // Rischedula prossima generazione
  m_genEvent = Simulator::Schedule (m_genInterval, &BurstBufferedSplitterApplication::GenerateStep, this);
}


void BurstBufferedSplitterApplication::BurstStep ()
{
  if (!m_running) return;
  ComputeTxBuffer();
  uint32_t tnTarget = DecisionSteering();
  uint32_t ntnTarget = tx_buffer.size() - tnTarget;

  // svuota buffer globale → split
  while (!tx_buffer.empty())
  {
    Ptr<Packet> p = tx_buffer.front();
    tx_buffer.pop();

    if (tnTarget > 0)
    {
      tx_bufferTN.push(p);
      tnTarget--;
    }
    else
    {
      tx_bufferNTN.push(p);
      ntnTarget--;
    }
  }

  Simulator::ScheduleNow(
    &BurstBufferedSplitterApplication::SendViaTN, this);

Simulator::ScheduleNow(
    &BurstBufferedSplitterApplication::SendViaNTN, this);

  // 4. reschedule steering
  m_burstEvent  = Simulator::Schedule(m_burstInterval, &BurstBufferedSplitterApplication::BurstStep, this);
}


void
BurstBufferedSplitterApplication::SendViaTN()
{
    if (!m_running || tx_bufferTN.empty())
        return;

    Ptr<Packet> p = tx_bufferTN.front();
    uint32_t pSize = p->GetSize();

    if (m_socketTN->Send(p) >= 0)
    {
        m_sentBytesTN += pSize;
    }
     else
    {
        NS_LOG_WARN("Physical Queue Full! Dropping packet from application buffer.");
    }

    tx_bufferTN.pop();
    m_currentBufferBytes -= pSize;

    if (!tx_bufferTN.empty())
    {
        Simulator::ScheduleNow(
            &BurstBufferedSplitterApplication::SendViaTN, this);
    }
}

void
BurstBufferedSplitterApplication::SendViaNTN()
{
    if (!m_running || tx_bufferNTN.empty())
        return;

    Ptr<Packet> p = tx_bufferNTN.front();
    uint32_t pSize = p->GetSize();

    if (m_socketNTN->Send(p) >= 0)
    {
        m_sentBytesNTN += pSize;
    }
    else
    {
        NS_LOG_WARN("Physical Queue Full! Dropping packet from application buffer.");
    }

    tx_bufferNTN.pop();
    m_currentBufferBytes -= pSize;

    if (!tx_bufferNTN.empty())
    {
        Simulator::ScheduleNow(
            &BurstBufferedSplitterApplication::SendViaNTN, this);
    }
}
} // namespace ns3