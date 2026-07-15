#ifndef BURST_BUFFERED_APPLICATION_H
#define BURST_BUFFERED_APPLICATION_H

#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/event-id.h"
#include "ns3/packet.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"

#include <queue>

namespace ns3 {

class BurstBufferedApplication : public Application
{
public:
  static TypeId GetTypeId (void);
  BurstBufferedApplication ();
  virtual ~BurstBufferedApplication ();

  /**
   * @param destAddr Indirizzo destinazione (IP + Porta o solo IP)
   * @param port Porta destinazione (se non inclusa in destAddr)
   * @param packetSize Quantità di dati da generare ogni volta (bytes)
   * @param genInterval Ogni quanto generare i dati
   * @param burstInterval Ogni quanto svuotare la coda
   * @param device (Opzionale) NetDevice specifico su cui fare il bind
   * @param localAddr (Opzionale) Indirizzo IP locale per il bind
   */
  void Setup (Address destAddr, uint16_t port, 
              uint32_t packetSize, Time genInterval, 
              Time burstInterval, 
              Ptr<NetDevice> device = 0, Ipv4Address localAddr = Ipv4Address::GetAny());

  // Statistiche
  uint64_t GetGeneratedBytes(void) const { return m_generatedBytes; }
  uint64_t GetSentBytes(void) const { return m_sentBytes; }
  uint64_t GetCurrentBufferBytes(void) const { return m_currentBufferBytes; }

protected:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

private:
  void WarmUpSend(void);
  // Funzioni logiche
  void GenerateStep (void); // Crea pacchetto
  void BurstStep (void);    // Invia tutto

  // Socket e Rete
  Ptr<Socket>     m_socket;
  Address         m_peerAddress;
  uint16_t        m_peerPort;
  Ptr<NetDevice>  m_device;
  Ipv4Address     m_localAddress;

  // Parametri di configurazione
  uint32_t        m_packetSize;    // Quanti dati generare (es. 100 byte)
  Time            m_genInterval;   // Ogni quanto generare (es. 2 sec)
  Time            m_burstInterval; // Ogni quanto inviare (es. 4 sec)

  // Stato
  std::queue<Ptr<Packet>> m_buffer;
  bool            m_running;
  EventId         m_genEvent;
  EventId         m_burstEvent;

  // Contatori
  uint64_t        m_generatedBytes;
  uint64_t        m_sentBytes;
  uint64_t        m_currentBufferBytes;
};

} // namespace ns3

#endif /* BURST_BUFFERED_APPLICATION_H */