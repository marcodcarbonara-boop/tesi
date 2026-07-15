#ifndef BURST_BUFFERED_SPLITTER_APPLICATION_H
#define BURST_BUFFERED_SPLITTER_APPLICATION_H

#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/event-id.h"
#include "ns3/packet.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"

#include <queue>

namespace ns3 {

class BurstBufferedSplitterApplication : public Application
{
public:
  static TypeId GetTypeId (void);
  BurstBufferedSplitterApplication ();
  virtual ~BurstBufferedSplitterApplication ();
  void Setup (Address destAddrTN, Address destAddrNTN, uint16_t portTN, uint16_t portNTN,
              uint32_t packetSize, Time genInterval,
              Time burstInterval, Time maxDelay,
              Ptr<NetDevice> deviceTN = 0, Ptr<NetDevice> deviceNTN = 0,
              Ipv4Address localAddrTN = Ipv4Address::GetAny(), Ipv4Address localAddrNTN = Ipv4Address::GetAny());

  // Statistiche
  uint64_t GetGeneratedBytes(void) const { return m_generatedBytes; }
  uint64_t GetSentBytesTN(void) const { return m_sentBytesTN; }
  uint64_t GetSentBytesNTN(void) const { return m_sentBytesNTN; }
  uint64_t GetCurrentBufferBytes(void) const { return m_currentBufferBytes; }

    double GetSteeringCoefficient() const {
        double a = 0;
        for (double val : steeringValues) {
            a += val;
        }
        return steeringValues.empty() ? 0 : a / steeringValues.size();
    }
protected:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  
  void WarmUpSend(void);

private:
  void GenerateStep (void);
  void BurstStep (void);

  void InitializeController(void);
  void ComputeTxBuffer(void);
  uint32_t DecisionSteering(void);
  void SendViaTN (void);
  void SendViaNTN (void);

  Ptr<Socket>     m_socketTN;
  Ptr<Socket>     m_socketNTN;
  Address         m_peerAddressTN;
  Address         m_peerAddressNTN;
  uint16_t        m_peerPortTN;
  uint16_t        m_peerPortNTN;
  Ptr<NetDevice>  m_deviceTN;
  Ptr<NetDevice>  m_deviceNTN;
  Ipv4Address     m_localAddressTN;
  Ipv4Address     m_localAddressNTN;

  std::vector<double> m_coefficients;
  std::vector<double> m_qHistory;
  std::vector<double> m_txHistory;


  uint32_t        m_packetSize;
  Time            m_genInterval;
  Time            m_maxDelay;
  Time            m_burstInterval; //timeslot

  std::queue<Ptr<Packet>> q_buffer;
    std::queue<Ptr<Packet>> tx_buffer;
  std::queue<Ptr<Packet>> tx_bufferTN;
  std::queue<Ptr<Packet>> tx_bufferNTN;

  bool            m_running;
  EventId         m_genEvent;
  EventId         m_burstEvent;

  uint64_t        m_generatedBytes;
  uint64_t        m_sentBytesTN;
  uint64_t        m_sentBytesNTN;
  uint64_t        m_currentBufferBytes;

std::vector<double> steeringValues; //vettore che memorizza i valori di a (steering ratio) calcolati ad ogni burst, per monitoraggio e debug

};

} // namespace ns3

#endif