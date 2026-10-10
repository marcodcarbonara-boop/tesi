#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/simple-5g-net-device.h"
#include "ns3/simple-5g-net-device-helper.h"
#include "ns3/simple-sat-net-device-helper.h"
#include "ns3/simple-sat-net-device.h"

#include "ns3/burst-buffered-application.h" 
#include "ns3/burst-buffered-splitter-application.h" 
#include "ns3/flow-monitor-module.h"
#include <random>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("OneBsOneSatScenario");


// --- Variabili Globali per il Tracing ---
std::ofstream g_throughputTrace; 
std::string g_outputDir;         


struct MonitorState {
    uint64_t rtTxTn = 0, rtRxTn = 0, rtTxNtn = 0, rtRxNtn = 0;
    uint64_t nrtTxTn = 0, nrtTxNtn = 0, nrtRxTn = 0, nrtRxNtn = 0;
};
MonitorState g_mon;

static double DeltaMbps(uint64_t current, uint64_t& last, double interval)
{
    double mbps = ((current - last) * 8.0) / (interval * 1e6);
    last = current;
    return mbps;
}

void LogNodePositionAndThroughput(Ptr<Node> node, std::string nodeName);
void LoadWaypointsFromCsv(const std::string& filename, Ptr<WaypointMobilityModel> mobilityModel, Time timeOffset);
void LogMixedTrafficStats(Ptr<BurstBufferedSplitterApplication> appRt, 
                          Ptr<PacketSink> sinkRtBs, Ptr<PacketSink> sinkRtSat, 
                          Ptr<PacketSink> sinkNrtBs, Ptr<PacketSink> sinkNrtSat,
                          Ptr<BurstBufferedApplication> appNrtBs, 
                          Ptr<BurstBufferedApplication> appNrtSat);
void MixedMonitorThroughput(Ptr<BurstBufferedSplitterApplication> appRt,
                            Ptr<PacketSink> sinkRtBs, Ptr<PacketSink> sinkRtSat,
                            Ptr<PacketSink> sinkNrtBs, Ptr<PacketSink> sinkNrtSat,
                            Ptr<BurstBufferedApplication> appNrtBs,
                            Ptr<BurstBufferedApplication> appNrtSat,
                            Ptr<Simple5gNetDevice> ue5g,
                            double interval);
void GenerateGnuplotScript();
void SetupSimulationDirectory();

double genInterval = 0.001; // Seconds
double burstInterval = 0.5; // Seconds
int pktSize = 1024; // Bytes
double maxDelay = -1; // Seconds, massimo ritardo accettabile per i pacchetti nel buffer prima di essere scartati (per evitare staleness)
//puo essere solo un multiplo di timeslot , vale almeno una durata intera di timeslot il pacchetto


int main(int argc, char* argv[])
{
LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);

   // LogComponentEnable("OneBsOneSatScenario", LOG_LEVEL_INFO);
    //LogComponentEnable("SimpleSatNetDevice", LOG_LEVEL_INFO);
    //LogComponentEnable("Simple5gNetDevice", LOG_LEVEL_INFO);
   // LogComponentEnable("BurstBufferedApplication", LOG_LEVEL_INFO);

    
    SetupSimulationDirectory();
    // --- Parametri di Canale Fisico (Valori di default: Scenario Urbano) --- 
    std::string tnDelay = "8ms"; // Ritardo di propagazione Terrestre (8ms Urbano / 12ms Rurale) 
    std::string satDelay = "25ms"; // Ritardo di propagazione Satellitare LEO (25ms Urbano / 28ms Rurale) 
    double satDataRate = 10.0; // Capacità nominale Satellite in Mbps (10Mbps Urbano / 12Mbps Rurale) 
    double tnDataRate = 20.0; // Capacità nominale terrestre in Mbps (20Mbps Urbano / 10.5Mbps Rurale) 
    double satErrorRate = 0.01; // Tasso di perdita pacchetti sul link satellitare (1% di Packet Error Rate)
    double tnErrorRate = 0.005; // Tasso di perdita pacchetti sul link terrestre (0.5% di Packet Error Rate)
    // `./ns3 run scratch/main --tnDelay=12ms --satDelay=28ms --satDataRate = 12.0 --tnDataRate = 10.5` per testare lo scenario rurale

    
    CommandLine cmd;
    cmd.AddValue("pktSize", "Packet size of traffic to generate", pktSize);
    cmd.AddValue("genInterval", "Generation interval of traffic", genInterval);
    cmd.AddValue("burstInterval", "Interval of bursts", burstInterval);
    cmd.AddValue("maxDelay", "Maximum acceptable delay for packets in the buffer before being dropped (to avoid staleness)", maxDelay);
    // esempio utilizzo: ./ns3 run scratch/[nomefile] --pktSize=2048 --genInterval=0.002 --burstInterval=1.0 --maxDelay=3.0

    // NUOVI PARAMETRI DI CANALE
    cmd.AddValue("tnDelay", "Propagation delay for Terrestrial link", tnDelay);
    cmd.AddValue("satDelay", "Propagation delay for LEO Satellite link", satDelay);
    cmd.AddValue("satRate", "Data rate for Satellite link in Mbps", satDataRate); 
    cmd.AddValue("tnRate", "Data rate for Terrestrial link in Mbps", tnDataRate);
    cmd.AddValue("satErrorRate", "Packet error rate on satellite link", satErrorRate);
    cmd.AddValue("tnErrorRate", "Packet error rate on 5G terrestrial link", tnErrorRate);


    cmd.Parse(argc, argv);
    if (maxDelay < 0) maxDelay = burstInterval * 3;

    // Setup nodi
    NodeContainer bsNode; bsNode.Create(1);
    NodeContainer ueNode; ueNode.Create(1);
    NodeContainer satNode; satNode.Create(1);
    NodeContainer sinkNode; sinkNode.Create(1);

    // Mobility: BS fixed
    MobilityHelper constantMobility;
    constantMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    constantMobility.Install(bsNode);
    Ptr<ConstantPositionMobilityModel> bsMobility = bsNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    bsMobility->SetPosition(Vector(0, 0.0, 0.0));

    // UE waypoint
    MobilityHelper waypointMobility;
    waypointMobility.SetMobilityModel("ns3::WaypointMobilityModel");
    waypointMobility.Install(ueNode);
    Ptr<WaypointMobilityModel> ueMobility = DynamicCast<WaypointMobilityModel>(ueNode.Get(0)->GetObject<MobilityModel>());
    if (!ueMobility) {
        NS_FATAL_ERROR("Impossibile ottenere WaypointMobilityModel per UE1.");
    }
//file dove il datarate 5g non è sempre 0
/*
file 18 rete 5g molto stabile
file 34 congestione moderata 5g
file 45 rete 5g completamente congestionata, c'è bisogno di intervento dalla NTN
file 400 rete 5g con perdirta di los, c'è bisogno di intervento dalla NTN

*/

    //ASSEGNAZIONE POSIZIONE WAYPOINT A UE
    vector<int> validFiles = {18, 34, 45, 400};
    mt19937 rng;
    uint64_t timeSeed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    seed_seq seed_sequence{uint32_t(timeSeed), uint32_t(timeSeed >> 32)};
    rng.seed(seed_sequence);

    uniform_int_distribution<int> distrib(0, validFiles.size() - 1);
    int selectedFile = validFiles[distrib(rng)];

    //CAMBIARE SE LA POSIZIONE DEI FILE È DIVERSA
    string basePath = "/home/pasta1/ns-allinone-3.38/ns-3.38/scratch/moduli/resources/positions1km/";

    stringstream ss;
    ss << basePath << selectedFile << ".csv";

    string filename = ss.str();
    LoadWaypointsFromCsv(filename, ueMobility, Seconds(0.0));

    // 5G devices
    Simple5gNetDeviceHelper bsHelper;
    Simple5gNetDeviceHelper ueHelper;
    Ptr<Simple5gChannel> channel = CreateObject<Simple5gChannel>();

    // Impostare la velocità nominale del link terrestre
    std::stringstream ssTnRate;
    ssTnRate << tnDataRate << "Mbps"; 
    
    // Applichiamo la banda nominale agli helper 5G
    bsHelper.SetDeviceAttribute("DataRate", DataRateValue(DataRate(ssTnRate.str())));
    ueHelper.SetDeviceAttribute("DataRate", DataRateValue(DataRate(ssTnRate.str())));

    bsHelper.SetIsBaseStation(true);
    NetDeviceContainer bsDevice = bsHelper.Install(bsNode, channel);
    Ptr<Simple5gNetDevice> bsPtrDevice = DynamicCast<Simple5gNetDevice>(bsDevice.Get(0));

    ueHelper.SetIsBaseStation(false);
    NetDeviceContainer ueDevice = ueHelper.Install(ueNode, channel);
    Ptr<Simple5gNetDevice> uePtrDevice = DynamicCast<Simple5gNetDevice>(ueDevice.Get(0));

    // Satellite device (connect SAT <-> UE)
    SimpleSatNetDeviceHelper satHelper;

    // Impostare la velocità nominale del link satellitare
    std::stringstream ssRate;
    ssRate << satDataRate << "Mbps"; 
    satHelper.SetDeviceAttribute("DataRate", DataRateValue(DataRate(ssRate.str())));

    //satHelper.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    NetDeviceContainer satDevices = satHelper.Install(NodeContainer(satNode.Get(0), ueNode.Get(0)));
    // satDevices: index 0 = SAT device, index 1 = UE device (per l'ordine passato)

    // Modello di errore per simulare il Packet Error Rate (PER) sul canale satellitare
    Ptr<RateErrorModel> emSat = CreateObject<RateErrorModel>();
    emSat -> SetAttribute("ErrorRate", DoubleValue(satErrorRate));
    emSat -> SetUnit(RateErrorModel::ERROR_UNIT_PACKET);

    // Modello di errore per simulare il Packet Error Rate (PER) sul canale terrestre
    Ptr<RateErrorModel> emTn = CreateObject<RateErrorModel>();
    emTn -> SetAttribute("ErrorRate", DoubleValue(tnErrorRate));
    emTn -> SetUnit(RateErrorModel::ERROR_UNIT_PACKET);

    

    // Applichiamo il modello di errore al dispositivo radio satellitare lato UE (indice 1)

    satDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(emSat));
    bsDevice.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(emTn));


    // Connect UE <-> BS logical association
    bsPtrDevice->AddConnectedUeAddress(Mac48Address::ConvertFrom(uePtrDevice->GetAddress()));
    uePtrDevice->SetConnectedBsAddress(Mac48Address::ConvertFrom(bsPtrDevice->GetAddress()), bsPtrDevice);

    // Position-based datarate updates for UE 5G
    uePtrDevice->StartPositionBasedDataRateUpdates(1.0, 5.0);

    // P2P links
    PointToPointHelper p2pBsSink;
    p2pBsSink.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2pBsSink.SetChannelAttribute("Delay", StringValue(tnDelay));
    NetDeviceContainer bsSinkDevices = p2pBsSink.Install(NodeContainer(bsNode.Get(0), sinkNode.Get(0)));

    PointToPointHelper p2pSatSink;
    p2pSatSink.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2pSatSink.SetChannelAttribute("Delay", StringValue(satDelay));
    NetDeviceContainer satSinkDevices = p2pSatSink.Install(NodeContainer(satNode.Get(0), sinkNode.Get(0)));

    // Internet stack
    InternetStackHelper stack;
    stack.Install(bsNode);
    stack.Install(ueNode);
    stack.Install(satNode);
    stack.Install(sinkNode);

    // IP addressing
    Ipv4AddressHelper address;

    // UE-BS network
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueBsInterfaces = address.Assign(NetDeviceContainer(ueDevice.Get(0), bsDevice.Get(0)));
    // ueBsInterfaces: index 0 = UE iface, index 1 = BS iface

    // UE-SAT network
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ueSatInterfaces = address.Assign(satDevices);
    // ueSatInterfaces: index 0 = SAT iface, index 1 = UE iface

    // BS-Sink (P2P)
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer bsSinkInterfaces = address.Assign(bsSinkDevices);
    // bsSinkInterfaces: index 0 = BS iface, index 1 = Sink iface

    // SAT-Sink (P2P)
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer satSinkInterfaces = address.Assign(satSinkDevices);
    // satSinkInterfaces: index 0 = SAT iface, index 1 = Sink iface

    // Populate routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();


    
    // ---------------------------------------------------------
    // --- Applications ---
    // ---------------------------------------------------------

    // Definiamo due porte distinte per distinguere i flussi real time al ricevitore 
    uint16_t portBsRt = 9;
    uint16_t portSatRt = 10;

    // Definiamo due porte distinte per distinguere i flussi non real time al ricevitore 
    uint16_t portBsNrt = 11;
    uint16_t portSatNrt = 12;

    // 1. Installiamo DUE Sink sul nodo destinazione
    
    // Sink per BS RT (Porta 9)
    PacketSinkHelper sinkRtBs("ns3::UdpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), portBsRt));
    ApplicationContainer sinkAppsRtBs = sinkRtBs.Install(sinkNode);
    sinkAppsRtBs.Start(Seconds(0.0));
    sinkAppsRtBs.Stop(Seconds(35.0)); 

    // Sink per SAT RT (Porta 10)
    PacketSinkHelper sinkRtSat("ns3::UdpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), portSatRt));
    ApplicationContainer sinkAppsRtSat = sinkRtSat.Install(sinkNode);
    sinkAppsRtSat.Start(Seconds(0.0));
    sinkAppsRtSat.Stop(Seconds(35.0));

    // Sink per BS NRT (Porta 11)
    PacketSinkHelper sinkNrtBs("ns3::UdpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), portBsNrt));
    ApplicationContainer sinkAppsNrtBs = sinkNrtBs.Install(sinkNode);
    sinkAppsNrtBs.Start(Seconds(0.0));
    sinkAppsNrtBs.Stop(Seconds(35.0)); 

    // Sink per SAT NRT (Porta 12)
    PacketSinkHelper sinkNrtSat("ns3::UdpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), portSatNrt));
    ApplicationContainer sinkAppsNrtSat = sinkNrtSat.Install(sinkNode);
    sinkAppsNrtSat.Start(Seconds(0.0));
    sinkAppsNrtSat.Stop(Seconds(35.0));


    // Recuperiamo i riferimenti necessari
    Ptr<Node> ue = ueNode.Get(0);
    
    // Device specifici per il binding (Multihoming)
    Ptr<NetDevice> ue5gDevice = ueDevice.Get(0); 
    Ptr<NetDevice> ueSatDevice = satDevices.Get(1); 

    // Indirizzi IP sorgenti (della UE) per il binding
    Ipv4Address ueIp5g  = ueBsInterfaces.GetAddress(0);
    Ipv4Address ueIpSat = ueSatInterfaces.GetAddress(1);

    // Indirizzi IP Destinazione (Sink)
    // Usiamo l'indirizzo dell'interfaccia del Sink connessa alla rispettiva rete
    // per aiutare il routing a scegliere il percorso giusto anche se usiamo BindToNetDevice
    Ipv4Address sinkIpViaBs  = bsSinkInterfaces.GetAddress(1);
    Ipv4Address sinkIpViaSat = satSinkInterfaces.GetAddress(1);

    // Terrestrial + Satellite (Target: Porta 9 e Porta 10)
    // ---------------------------------------------------------
    NS_LOG_INFO("Configuring Burst App 3 (Splitter TN + NTN)");
    Ptr<BurstBufferedSplitterApplication> appRtSplitter = CreateObject<BurstBufferedSplitterApplication>();
    appRtSplitter->Setup(
        sinkIpViaBs,
        sinkIpViaSat,
        portBsRt,
        portSatRt,
        pktSize,
        Seconds(genInterval), 
        Seconds(burstInterval), 
        Seconds(maxDelay),
        ue5gDevice,
        ueSatDevice
                );
    appRtSplitter->SetStartTime(Seconds(0.0));
    appRtSplitter->SetStopTime(Seconds(15.0));     
    //APPLICAZIONE SPLITTER TRAFFICO SU TN E NTN, decommentare per visualizzare scenario con splitter TN e NTN                                                  
    ue->AddApplication(appRtSplitter);

    //  APPLICAZIONE NON-REAL-TIME SULLA RETE TERRESTRE  (porta 11)
    Ptr<BurstBufferedApplication> appNrtBs = CreateObject<BurstBufferedApplication>();
    appNrtBs->Setup(
        InetSocketAddress(sinkIpViaBs, portBsNrt),
        portBsNrt,
        pktSize,
        Seconds(genInterval *2),
        Seconds(burstInterval),
        ue5gDevice
                );
    appNrtBs->SetStartTime(Seconds(0.0));
    appNrtBs->SetStopTime(Seconds(15.0));     
    ue->AddApplication(appNrtBs);

    //  APPLICAZIONE NON-REAL-TIME SULLA RETE SATELLITARE (porta 12)
    Ptr<BurstBufferedApplication> appNrtSat = CreateObject<BurstBufferedApplication>();
    appNrtSat->Setup(
        InetSocketAddress(sinkIpViaSat, portSatNrt),
        portSatNrt,
        pktSize,
        Seconds(genInterval *4),
        Seconds(burstInterval),
        ueSatDevice
                );
    appNrtSat->SetStartTime(Seconds(0.0));
    appNrtSat->SetStopTime(Seconds(15.0));     
    ue->AddApplication(appNrtSat);

    

        // --- MONITORAGGIO GRAFICO ---
    
    // 1. Apriamo il file di trace nella cartella specifica
  
    std::string tracePath = g_outputDir + "/throughput_trace.dat";
    g_throughputTrace.open(tracePath);
    if (!g_throughputTrace.is_open()) {
        NS_LOG_WARN("Could not open " << tracePath << "!");
    } else {
        NS_LOG_INFO("Writing trace data to: " << tracePath);
    }

    // 2. Otteniamo i puntatori ai Sink PRIMA del Run per poterli passare al monitor
    //nessuna separazione Rt e NRT
    //Ptr<PacketSink> sinkPtrBs = DynamicCast<PacketSink>(sinkAppsBs.Get(0));
    //Ptr<PacketSink> sinkPtrSat = DynamicCast<PacketSink>(sinkAppsSat.Get(0));

    //Separazione RT e NRT
    Ptr<PacketSink> sinkPtrRtBs = DynamicCast<PacketSink>(sinkAppsRtBs.Get(0));
    Ptr<PacketSink> sinkPtrRtSat = DynamicCast<PacketSink>(sinkAppsRtSat.Get(0));
    Ptr<PacketSink> sinkPtrNrtBs = DynamicCast<PacketSink>(sinkAppsNrtBs.Get(0));
    Ptr<PacketSink> sinkPtrNrtSat = DynamicCast<PacketSink>(sinkAppsNrtSat.Get(0));


    // 3. Scheduliamo il monitoraggio ogni 0.1 secondi
    Simulator::Schedule(Seconds(0.0), [=]() {
    MixedMonitorThroughput(appRtSplitter, sinkPtrRtBs, sinkPtrRtSat,
                           sinkPtrNrtBs, sinkPtrNrtSat,
                           appNrtBs, appNrtSat, uePtrDevice, 0.1);
});
    
    // ---------------------------------------------------------
    // Run & Log
    // ---------------------------------------------------------
    
    // Log posizione (commentato se non serve sempre)
    //LogNodePositionAndThroughput(ue, "UE");
    FlowMonitorHelper fmHelper;
    Ptr<FlowMonitor> flowMon = fmHelper.InstallAll();

    Simulator::Stop(Seconds(17.0)); 
    Simulator::Run();


    Ptr<Ipv4FlowClassifier> cls = DynamicCast<Ipv4FlowClassifier>(fmHelper.GetClassifier());

    //split con RT e NRT

    // REPORT FINALE (per Traffico Misto RT + NRT)
    LogMixedTrafficStats(appRtSplitter, sinkPtrRtBs, sinkPtrRtSat, sinkPtrNrtBs, sinkPtrNrtSat, appNrtBs, appNrtSat);


    // Chiudiamo il file e generiamo lo script plot
    g_throughputTrace.close();
    GenerateGnuplotScript();

    std::cout << "\n===== QoS PER FLUSSO =====\n";
    for (const auto& [id, st] : flowMon->GetFlowStats())
    {
    auto t = cls->FindFlow(id);
    if (t.destinationPort < 9 || t.destinationPort > 12) continue;
    double rx = st.rxPackets;
    double delayMs  = rx > 0 ? st.delaySum.GetSeconds() * 1000.0 / rx : 0.0;
    double jitterMs = rx > 1 ? st.jitterSum.GetSeconds() * 1000.0 / (rx - 1) : 0.0;
    double lossPerc = st.txPackets > 0 ? 100.0 * (st.txPackets - st.rxPackets) / st.txPackets : 0.0;
    std::cout << "porta " << t.destinationPort << ": tx " << st.txPackets << " rx " << st.rxPackets
        << " loss " << lossPerc << "% | ritardo " << delayMs << " ms | jitter " << jitterMs << " ms\n";
    }

    Simulator::Destroy();
    return 0;
}



// --- helper functions  ---

void
LoadWaypointsFromCsv(const std::string& filename, Ptr<WaypointMobilityModel> mobilityModel, Time timeOffset)
{
    std::ifstream file(filename.c_str());
    if (!file.is_open())
    {
        NS_FATAL_ERROR("Impossibile aprire il file CSV: " << filename);
    }

    std::string line;

    // Salta l'intestazione e la prima riga di dati - in alcuni file le prime righe sono a 0
    if (std::getline(file, line)) { }
    if (std::getline(file, line)) { }

    // Leggi la seconda riga di dati per impostare la posizione iniziale del device
    if (std::getline(file, line))
    {
        std::istringstream ss(line);
        std::string token;
        double time, x, y;
        std::getline(ss, token, ','); time = std::stod(token);
        std::getline(ss, token, ','); x = std::stod(token);
        std::getline(ss, token, ','); y = std::stod(token);
        mobilityModel->SetPosition(Vector(x, y, 0.0));
        NS_LOG_INFO("Posizione iniziale impostata a (" << x << ", " << y << ")");
        mobilityModel->AddWaypoint(Waypoint(timeOffset + Seconds(time), Vector(x, y, 0.0)));
    }

    while (std::getline(file, line))
    {
        std::istringstream ss(line);
        std::string token;
        double time, x, y;
        std::getline(ss, token, ','); time = std::stod(token);
        std::getline(ss, token, ','); x = std::stod(token);
        std::getline(ss, token, ','); y = std::stod(token);
        mobilityModel->AddWaypoint(Waypoint(timeOffset + Seconds(time), Vector(x, y, 0.0)));
    }
}

void
LogNodePositionAndThroughput(Ptr<Node> node, std::string nodeName)
{
    Ptr<WaypointMobilityModel> mobility = node->GetObject<WaypointMobilityModel>();
    Ptr<Simple5gNetDevice> device = DynamicCast<Simple5gNetDevice>(node->GetDevice(0));

    if (mobility && device)
    {
        Vector position = mobility->GetPosition();
        DataRate currentDataRate;
        DataRateValue dataRateValue;
        device->GetAttribute("DataRate", dataRateValue);
        currentDataRate = dataRateValue.Get();

        NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() << "s | "
                             << nodeName << " Position: x=" << position.x
                             << ", y=" << position.y << ", z=" << position.z
                             << " | Throughput: " << currentDataRate << " | Assigned PRBs: "
                             << device->GetAssignedPrbs()
                             << " ue connessa " << device->GetConnectedBsAddress());
    }
    else
    {
        NS_LOG_WARN("Node " << nodeName << " does not have a MobilityModel or Simple5gNetDevice. Cannot log position/throughput.");
        return;
    }
    Simulator::Schedule(Seconds(1.0), &LogNodePositionAndThroughput, node, nodeName);
}



void LogMixedTrafficStats(Ptr<BurstBufferedSplitterApplication> appRt, 
                          Ptr<PacketSink> sinkRtBs, Ptr<PacketSink> sinkRtSat, 
                          Ptr<PacketSink> sinkNrtBs, Ptr<PacketSink> sinkNrtSat,
                          Ptr<BurstBufferedApplication> appNrtBs, 
                          Ptr<BurstBufferedApplication> appNrtSat )
{
    double toMB = 1.0 / (1e6); // Conversione da Byte a Megabyte (MB)

    // 1\. Dati Real-Time (RT)
    double genRtMB = appRt->GetGeneratedBytes() * toMB;
    double sentRtBsMB = appRt->GetSentBytesTN() * toMB;
    double sentRtSatMB = appRt->GetSentBytesNTN() * toMB;
    double rxRtBsMB = sinkRtBs->GetTotalRx() * toMB;
    double rxRtSatMB = sinkRtSat->GetTotalRx() * toMB; 
    double totalRtRxMB = rxRtBsMB + rxRtSatMB; 

    // Calcolo Backlog (pacchetti rimasti nel buffer dell'app) e Perdite di Rete

    double backlogRtMB = genRtMB - sentRtBsMB - sentRtSatMB; 
    double lossRtBsMB    = sentRtBsMB - rxRtBsMB;  
    double lossRtSatMB    = sentRtSatMB - rxRtSatMB;  
    double lossPercRtBs = (sentRtBsMB > 0) ? (lossRtBsMB / sentRtBsMB) * 100.0 : 0.0;
    double lossPercRtSat = (sentRtSatMB > 0) ? (lossRtSatMB / sentRtSatMB) * 100.0 : 0.0;


    // 2\. Dati Non-Real-Time (NRT / Background) 
    double rxNrtBsMB = sinkNrtBs->GetTotalRx() * toMB;
    double rxNrtSatMB = sinkNrtSat->GetTotalRx() * toMB; 

    double residualRtMB  = appRt->GetCurrentBufferBytes() * toMB;
    double discardedRtMB = backlogRtMB - residualRtMB;     // scartati per maxDelay (se negativo, l'ipotesi è falsa)

    double genNrtBsMB   = appNrtBs->GetGeneratedBytes() * toMB;
    double sentNrtBsMB  = appNrtBs->GetSentBytes() * toMB;
    double genNrtSatMB  = appNrtSat->GetGeneratedBytes() * toMB;
    double sentNrtSatMB = appNrtSat->GetSentBytes() * toMB;

    double lossNrtBsMB  = sentNrtBsMB - rxNrtBsMB;
    double lossNrtSatMB = sentNrtSatMB - rxNrtSatMB;
    double lossPercNrtBs  = (sentNrtBsMB  > 0) ? lossNrtBsMB  / sentNrtBsMB  * 100.0 : 0.0;
    double lossPercNrtSat = (sentNrtSatMB > 0) ? lossNrtSatMB / sentNrtSatMB * 100.0 : 0.0;
    double totalNrtRxMB = rxNrtBsMB + rxNrtSatMB;

    // 3\. TOTALI COMPLESSIVI TERRESTRE (TN) E SATELLITARE (SAT)
    double totalSentTN = sentRtBsMB + sentNrtBsMB;
    double totalRxTN = rxRtBsMB + rxNrtBsMB;
    double totalLossTN = totalSentTN - totalRxTN;
    double lossPercTotalTN = (totalSentTN > 0) ? (totalLossTN / totalSentTN) * 100.0 : 0.0;

    double totalSentSAT = sentRtSatMB + sentNrtSatMB;
    double totalRxSAT = rxRtSatMB + rxNrtSatMB;
    double totalLossSAT = totalSentSAT - totalRxSAT;
    double lossPercTotalSAT = (totalSentSAT > 0) ? (totalLossSAT / totalSentSAT)* 100.0 : 0.0;

    // 4\. TOTALE COMPLESSIVO GLOBALE (TN + SAT)
    double totalSentGlobal = totalSentTN + totalSentSAT;
    double totalRxGlobal = totalRxTN + totalRxSAT;
    double totalLossGlobal = totalSentGlobal - totalRxGlobal;
    double globalLossPerc = (totalSentGlobal > 0) ? (totalLossGlobal / totalSentGlobal) * 100.0 : 0.0;
    double totalGenGlobal = genRtMB + genNrtBsMB + genNrtSatMB;
    double deliveryRatio  = (totalGenGlobal > 0) ? totalRxGlobal / totalGenGlobal * 100.0 : 0.0;
        
    double steeringCoeff = appRt -> GetSteeringCoefficient();


    // 5\. Stampa del Report Finale
    std::cout << "\n========= DETAILED TRAFFIC REPORT =========\n\n";

    std::cout << "[1] TRAFFICO REAL-TIME \n"; 
    std::cout << " Totale Generato dalla Sorgente : " << genRtMB <<  " MB\n";
    std::cout << " Inviato su 5G (TN) : " << sentRtBsMB <<  " MB\n";
    std::cout << " Inviato su Satellite (NTN) : " << sentRtSatMB <<  " MB\n";
    std::cout << " Scartati per maxDelay : " << discardedRtMB <<  " MB\n";
    std::cout << " Residuo a fine sim : " << residualRtMB <<  " MB\n";


    std::cout << " ----------------------------\n ";
    std::cout << "Ricevuto al sink 5G (Porta 9) : " << rxRtBsMB<< " MB (" << lossPercRtBs << "%"<<" perdita di rete)\n";
    std::cout << "Ricevuto al sink Sat (Porta 10) : " << rxRtSatMB<< " MB (" << lossPercRtSat << "%"<< " perdita di rete)\n";
    std::cout << "-> TOTALE RT CONSEGNATO : "<< totalRtRxMB <<  " MB\n";    

    std::cout << " ----------------------------\n ";
    std::cout << "[2] TRAFFICO NON-REAL-TIME \n";
    std::cout << "5G (Porta 11): generato " << genNrtBsMB << " MB\n";
    std::cout << "inviati :"<< sentNrtBsMB<< " MB\n"; 
    std::cout << "ricevuti :" << rxNrtBsMB << " MB (" << lossPercNrtBs << "%" <<" perdita di rete)\n";
    std::cout << "Sat (Porta 12): generato " << genNrtSatMB << " MB\n";
    std::cout << "inviati :" << sentNrtSatMB<< " MB\n";
    std::cout << "ricevuti :" << rxNrtSatMB << " MB (" << lossPercNrtSat << "%" <<" perdita di rete)\n";
    std::cout << "-> TOTALE NRT CONSEGNATO : " << totalNrtRxMB << " MB\n";
    std::cout << " ----------------------------\n ";

    std::cout << "[3] TOTALI COMPLESSIVI PER CANALE DI RETE \n"; 
    std::cout << "CANALE TERRESTRE (TN 5G) :\n"; 
    std::cout << "Totale Inviato : "<< totalSentTN <<  " MB\n";
    std::cout << "Totale Ricevuto : "<< totalRxTN <<  " MB\n";
    std::cout << "Network Loss TN "<< totalLossTN <<  " MB (" << lossPercTotalTN << "% )\n";

    std::cout << "CANALE SATELLITARE (NTN LEO) :\n";
    std::cout << "Totale Inviato : "<< totalSentSAT <<  " MB\n";
    std::cout << "Totale Ricevuto : "<< totalRxSAT <<  " MB\n";
    std::cout << "Network Loss NTN : "<< totalLossSAT <<  " MB (" << lossPercTotalSAT << "% )\n";
 
    std::cout << "======================================================\n";
    std::cout << "[4] RIEPILOGO GLOBALE DELLA SIMULAZIONE:\n";
    std::cout << "Totale Traffico Inviato : " << totalSentGlobal << " MB\n";
    std::cout << "Totale Traffico Ricevuto : " << totalRxGlobal << " MB\n";
    std::cout << "Totale Pacchetti Persi in Rete: " << totalLossGlobal << " MB\n";
    std::cout << "Network Loss PERCENTUALE GLOBALE: " << globalLossPerc << " %\n";
    std::cout << "Consegnato/Generato globale: " << deliveryRatio << " %\n";

    std::cout << "Steering Coeff alpha(k) Finale : "<< steeringCoeff <<  "\n";

}

void MixedMonitorThroughput(Ptr<BurstBufferedSplitterApplication> appRt,
                            Ptr<PacketSink> sinkRtBs, Ptr<PacketSink> sinkRtSat,
                            Ptr<PacketSink> sinkNrtBs, Ptr<PacketSink> sinkNrtSat,
                            Ptr<BurstBufferedApplication> appNrtBs,
                            Ptr<BurstBufferedApplication> appNrtSat,
                            Ptr<Simple5gNetDevice> ue5g,
                            double interval)
{
    double now = Simulator::Now().GetSeconds();

    double rtTxTn   = DeltaMbps(appRt->GetSentBytesTN(),  g_mon.rtTxTn,  interval);
    double rtRxTn   = DeltaMbps(sinkRtBs->GetTotalRx(),   g_mon.rtRxTn,  interval);
    double rtTxNtn  = DeltaMbps(appRt->GetSentBytesNTN(), g_mon.rtTxNtn, interval);
    double rtRxNtn  = DeltaMbps(sinkRtSat->GetTotalRx(),  g_mon.rtRxNtn, interval);

    double nrtTxTn  = DeltaMbps(appNrtBs->GetSentBytes(),  g_mon.nrtTxTn,  interval);
    double nrtRxTn  = DeltaMbps(sinkNrtBs->GetTotalRx(),   g_mon.nrtRxTn,  interval);
    double nrtTxNtn = DeltaMbps(appNrtSat->GetSentBytes(), g_mon.nrtTxNtn, interval);
    double nrtRxNtn = DeltaMbps(sinkNrtSat->GetTotalRx(),  g_mon.nrtRxNtn, interval);

    double alpha         = appRt->GetSteeringCoefficient();
    double nrtBufBsMbit  = (appNrtBs->GetCurrentBufferBytes()  * 8.0) / 1e6;
    double nrtBufSatMbit = (appNrtSat->GetCurrentBufferBytes() * 8.0) / 1e6;
    double bufferMbits = (appRt->GetCurrentBufferBytes() * 8.0) / 1e6;

    DataRateValue drv;
    ue5g->GetAttribute("DataRate", drv);       
    double cap5gMbps     = drv.Get().GetBitRate() / 1e6;

    // Col: 1 time | 2 RT Tx TN | 3 RT Rx TN | 4 RT Tx NTN | 5 RT Rx NTN | 6 buffer
    //      7 NRT Tx TN | 8 NRT Rx TN | 9 NRT Tx NTN | 10 NRT Rx NTN | 11 alpha | 
    //      12 buffer NRT 5G | 13 buffer NRT sat | 14 capacità 5G
    g_throughputTrace << now << " "
                      << rtTxTn << " " << rtRxTn << " " << rtTxNtn << " " << rtRxNtn << " "
                      << bufferMbits << " "
                      << nrtTxTn << " " << nrtRxTn << " " << nrtTxNtn << " " << nrtRxNtn
                      << " " << alpha << " " << nrtBufBsMbit << " " << nrtBufSatMbit << " " << cap5gMbps
                      << std::endl;

    Simulator::Schedule(Seconds(interval), [=]() {
    MixedMonitorThroughput(appRt, sinkRtBs, sinkRtSat,
                           sinkNrtBs, sinkNrtSat,
                           appNrtBs, appNrtSat, ue5g, interval);
});
}

// --- Generazione Script Gnuplot ---
void GenerateGnuplotScript()
{
    std::string pltPath = g_outputDir + "/plot_throughput.plt";
    std::string datPath = g_outputDir + "/throughput_trace.dat";
    std::string png5g = g_outputDir + "/throughput_5g.png";
    std::string pngSat = g_outputDir + "/throughput_sat.png";
    std::string pngBuffer = g_outputDir + "/buffer_occupancy.png";
    std::string pngNrt5g = g_outputDir + "/throughput_5g_nrt.png";
    std::string pngNrtSat = g_outputDir + "/throughput_sat_nrt.png";
    std::string pngAlpha = g_outputDir + "/alpha.png";
    std::string pngNrtBuf = g_outputDir + "/buffer_nrt.png";
    std::string pngCap = g_outputDir + "/capacity_5g.png";

    std::ofstream plotFile;
    plotFile.open(pltPath);

    // Configurazione Comune
    plotFile << "set terminal png size 1200,800 enhanced font 'Verdana,18'" << std::endl;
    plotFile << "set grid" << std::endl;
    plotFile << "set style data lines" << std::endl;
    plotFile << "set key top left" << std::endl;
    plotFile << "set xlabel 'Simulation Time (s)'" << std::endl;

    // --- GRAFICO 1: 5G Terrestre (Solo Throughput) ---
    plotFile << "set output '" << png5g << "'" << std::endl;
    plotFile << "set title '5G Channel - RT: Tx vs Rx'" << std::endl;
    plotFile << "set ylabel 'Throughput (Mbps)' textcolor rgb 'black'" << std::endl;

    // Disabilita asse Y destro per pulizia
    plotFile << "unset y2tics" << std::endl;
    plotFile << "unset y2label" << std::endl;
    plotFile << "set ytics mirror" << std::endl;

    plotFile << "plot '" << datPath << "' using 1:2 title 'RT Tx' lw 2 lc rgb 'blue', \\" << std::endl;
    plotFile << "     '" << datPath << "' using 1:3 title 'RT Rx' lw 2 lc rgb 'green'" << std::endl;

    // --- GRAFICO 2: Satellite (Solo Throughput) ---
    plotFile << "set output '" << pngSat << "'" << std::endl;
    plotFile << "set title 'Satellite Channel - RT: Tx vs Rx'" << std::endl;

    plotFile << "plot '" << datPath << "' using 1:4 title 'RT Tx' lw 2 lc rgb 'red', \\" << std::endl;
    plotFile << "     '" << datPath << "' using 1:5 title 'RT Rx' lw 2 lc rgb 'orange'" << std::endl;

    // --- GRAFICO 3: Buffer Occupancy Comparison ---
    plotFile << "set output '" << pngBuffer << "'" << std::endl;
    plotFile << "set title 'RT Buffer Occupancy (splitter)'" << std::endl;
    plotFile << "set ylabel 'RT App Buffer' textcolor rgb 'black'" << std::endl;

    plotFile << "plot '" << datPath << "' using 1:6 title 'App Buffer' lw 2 lc rgb 'blue'" << std::endl;

    // --- GRAFICO 4: 5G Terrestre, traffico NRT (colonne 7-8) ---
    plotFile << "set output '" << pngNrt5g << "'" << std::endl;
    plotFile << "set title 'NRT 5G Channel: Tx vs Rx'" << std::endl;
    plotFile << "set ylabel 'Throughput (Mbps)' textcolor rgb 'black'" << std::endl;

    plotFile << "plot '" << datPath << "' using 1:7 title 'NRT Tx' lw 2 lc rgb 'purple', \\" << std::endl;
    plotFile << "     '" << datPath << "' using 1:8 title 'NRT Rx' lw 2 lc rgb '#008B8B'" << std::endl;

    // --- GRAFICO 5: Satellite, traffico NRT (colonne 9-10) ---
    plotFile << "set output '" << pngNrtSat << "'" << std::endl;
    plotFile << "set title 'NRT Satellite Channel: Tx vs Rx'" << std::endl;

    plotFile << "plot '" << datPath << "' using 1:9 title 'NRT Tx' lw 2 lc rgb 'brown', \\" << std::endl;
    plotFile << "     '" << datPath << "' using 1:10 title 'NRT Rx' lw 2 lc rgb '#DAA520'" << std::endl;

    plotFile << "set output '" << pngAlpha << "'" << std::endl;
    plotFile << "set title 'Steering coefficient alpha(k)'" << std::endl;
    plotFile << "set ylabel 'alpha'" << std::endl;
    plotFile << "plot '" << datPath << "' using 1:11 title 'alpha(k)' lw 2 lc rgb 'black'" << std::endl;

    plotFile << "set output '" << pngNrtBuf << "'" << std::endl;
    plotFile << "set title 'NRT Buffer Occupancy'" << std::endl;
    plotFile << "set ylabel 'Buffer Occupancy (Mbits)'" << std::endl;
    plotFile << "plot '" << datPath << "' using 1:12 title 'NRT buffer 5G' lw 2 lc rgb 'purple', \\" << std::endl;
    plotFile << "     '" << datPath << "' using 1:13 title 'NRT buffer Sat' lw 2 lc rgb 'brown'" << std::endl;

    plotFile << "set output '" << pngCap << "'" << std::endl;
    plotFile << "set title '5G: capacita vs Rx RT+NRT'" << std::endl;
    plotFile << "set ylabel 'Throughput (Mbps)'" << std::endl;
    plotFile << "plot '" << datPath << "' using 1:14 title 'Capacita 5G' lw 2 lc rgb 'black', \\" << std::endl;
    plotFile << "     '" << datPath << "' using 1:($3+$8) title 'Rx RT+NRT' lw 2 lc rgb 'green'" << std::endl;

    plotFile.close();

    NS_LOG_INFO("Gnuplot script generated at: " << pltPath);
    std::string gnuplotCmd = "gnuplot " + pltPath;
    int ret = system(gnuplotCmd.c_str());
    if (ret != 0)
    {
        std::cerr << "gnuplot failed: " << gnuplotCmd << std::endl;
        return;
    }
    if (system(("feh " + png5g + " " + pngSat + " " + pngBuffer + " " +
                pngNrt5g + " " + pngNrtSat + " " + pngAlpha + " " + pngNrtBuf + " " + pngCap + " &").c_str()) != 0)
    {
        std::cerr << "feh failed" << std::endl;
    }
}


// Funzione Helper per creare la cartella univoca
void SetupSimulationDirectory()
{
    // 1. Ottieni il percorso Desktop
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        NS_LOG_WARN("Cannot find HOME directory. Using current directory.");
        g_outputDir = ".";
        return;
    }

    std::string baseDir = std::string(homeDir) + "/Desktop/exec_data";

    // 2. Crea il timestamp
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[80];
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, 80, "%Y-%m-%d_%H-%M-%S", timeinfo);
    std::string timestamp(buffer);

    // 3. Definisci il percorso completo per questa run
    g_outputDir = baseDir + "/run_" + timestamp;

    // 4. Crea la cartella usando system command (mkdir -p crea anche le cartelle intermedie se mancano)
    std::string cmd = "mkdir -p " + g_outputDir;
    int status = system(cmd.c_str());

    if (status == 0) {
        NS_LOG_INFO("Output directory created: " << g_outputDir);
    } else {
        NS_LOG_WARN("Failed to create directory: " << g_outputDir << ". Using current directory.");
        g_outputDir = ".";
    }
}