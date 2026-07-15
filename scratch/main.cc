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
#include <random>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("OneBsOneSatScenario");


// --- Variabili Globali per il Tracing ---
std::ofstream g_throughputTrace; 
std::string g_outputDir;         

// Memoria per il calcolo del delta (Rx al Sink)
uint64_t g_lastRxBytesBs = 0;    
uint64_t g_lastRxBytesSat = 0;   

// Memoria per il calcolo del delta (Tx dall'App - Generazione/Invio)
uint64_t g_lastSentBytesBs = 0;
uint64_t g_lastSentBytesSat = 0;


void LogNodePositionAndThroughput(Ptr<Node> node, std::string nodeName);
void LoadWaypointsFromCsv(const std::string& filename, Ptr<WaypointMobilityModel> mobilityModel, Time timeOffset);
void LogTrafficStats(Ptr<BurstBufferedApplication> appBs,
                     Ptr<BurstBufferedApplication> appSat,
                     Ptr<PacketSink> sinkBs,   
                     Ptr<PacketSink> sinkSat); 
void SplitterLogTrafficStats(Ptr<BurstBufferedSplitterApplication> app,
                     Ptr<PacketSink> sinkBs,   
                     Ptr<PacketSink> sinkSat);

void MonitorThroughput(Ptr<BurstBufferedApplication> appBs, Ptr<BurstBufferedApplication> appSat, 
                       Ptr<PacketSink> sinkBs, Ptr<PacketSink> sinkSat, 
                       double interval);
void SplitterMonitorThroughput(Ptr<BurstBufferedSplitterApplication> app, 
                             Ptr<PacketSink> sinkBs, Ptr<PacketSink> sinkSat, 
                             double interval);
void GenerateGnuplotScript();
void SetupSimulationDirectory();

double genInterval = 0.001; // Seconds
double burstInterval = 0.5; // Seconds
int pktSize = 1024; // Bytes
double maxDelay = burstInterval*3; // Seconds, massimo ritardo accettabile per i pacchetti nel buffer prima di essere scartati (per evitare staleness)
//puo essere solo un multiplo di timeslot , vale almeno una durata intera di timeslot il pacchetto


int main(int argc, char* argv[])
{
LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);

   // LogComponentEnable("OneBsOneSatScenario", LOG_LEVEL_INFO);
    //LogComponentEnable("SimpleSatNetDevice", LOG_LEVEL_INFO);
    //LogComponentEnable("Simple5gNetDevice", LOG_LEVEL_INFO);
   // LogComponentEnable("BurstBufferedApplication", LOG_LEVEL_INFO);

    
    SetupSimulationDirectory();
    
    CommandLine cmd;
    cmd.AddValue("pktSize", "Packet size of traffic to generate", pktSize);
    cmd.AddValue("genInterval", "Generation interval of traffic", genInterval);
    cmd.AddValue("burstInterval", "Interval of bursts", burstInterval);
    cmd.AddValue("maxDelay", "Maximum acceptable delay for packets in the buffer before being dropped (to avoid staleness)", maxDelay);
    cmd.Parse(argc, argv);
    // esempio utilizzo: ./ns3 run scratch/[nomefile] --pktSize=2048 --genInterval=0.002 --burstInterval=1.0 --maxDelay=3.0


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
*/

    //ASSEGNAZIONE POSIZIONE WAYPOINT A UE
    vector<int> validFiles = {18, 34, 45};
    mt19937 rng;
    uint64_t timeSeed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    seed_seq seed_sequence{uint32_t(timeSeed), uint32_t(timeSeed >> 32)};
    rng.seed(seed_sequence);

    uniform_int_distribution<int> distrib(0, validFiles.size() - 1);
    int selectedFile = validFiles[distrib(rng)];

    //CAMBIARE SE LA POSIZIONE DEI FILE È DIVERSA
    string basePath = "/home/ubuntu/ns-3-dev/scratch/tn-ntn-ns3/moduli/resources/positions1km/";

    stringstream ss;
    ss << basePath << selectedFile << ".csv";

    string filename = ss.str();
    LoadWaypointsFromCsv(filename, ueMobility, Seconds(0.0));

    // 5G devices
    Simple5gNetDeviceHelper bsHelper;
    Simple5gNetDeviceHelper ueHelper;
    Ptr<Simple5gChannel> channel = CreateObject<Simple5gChannel>();

    bsHelper.SetIsBaseStation(true);
    NetDeviceContainer bsDevice = bsHelper.Install(bsNode, channel);
    Ptr<Simple5gNetDevice> bsPtrDevice = DynamicCast<Simple5gNetDevice>(bsDevice.Get(0));

    ueHelper.SetIsBaseStation(false);
    NetDeviceContainer ueDevice = ueHelper.Install(ueNode, channel);
    Ptr<Simple5gNetDevice> uePtrDevice = DynamicCast<Simple5gNetDevice>(ueDevice.Get(0));

    // Satellite device (connect SAT <-> UE)
    SimpleSatNetDeviceHelper satHelper;
    //satHelper.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    NetDeviceContainer satDevices = satHelper.Install(NodeContainer(satNode.Get(0), ueNode.Get(0)));
    // satDevices: index 0 = SAT device, index 1 = UE device (per l'ordine passato)

    // Connect UE <-> BS logical association
    bsPtrDevice->AddConnectedUeAddress(Mac48Address::ConvertFrom(uePtrDevice->GetAddress()));
    uePtrDevice->SetConnectedBsAddress(Mac48Address::ConvertFrom(bsPtrDevice->GetAddress()), bsPtrDevice);

    // Position-based datarate updates for UE 5G
    uePtrDevice->StartPositionBasedDataRateUpdates(1.0, 5.0);

    // P2P links
    PointToPointHelper p2pBsSink;
    p2pBsSink.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2pBsSink.SetChannelAttribute("Delay", StringValue("0ms"));
    NetDeviceContainer bsSinkDevices = p2pBsSink.Install(NodeContainer(bsNode.Get(0), sinkNode.Get(0)));

    PointToPointHelper p2pSatSink;
    p2pSatSink.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2pSatSink.SetChannelAttribute("Delay", StringValue("0ms"));
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

     // Definiamo due porte distinte per distinguere i flussi al ricevitore
    uint16_t portBs = 9;
    uint16_t portSat = 10;

    // 1. Installiamo DUE Sink sul nodo destinazione
    
    // Sink per BS (Porta 9)
    PacketSinkHelper sinkHelperBs("ns3::UdpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), portBs));
    ApplicationContainer sinkAppsBs = sinkHelperBs.Install(sinkNode);
    sinkAppsBs.Start(Seconds(0.0));
    sinkAppsBs.Stop(Seconds(35.0)); 

    // Sink per SAT (Porta 10)
    PacketSinkHelper sinkHelperSat("ns3::UdpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), portSat));
    ApplicationContainer sinkAppsSat = sinkHelperSat.Install(sinkNode);
    sinkAppsSat.Start(Seconds(0.0));
    sinkAppsSat.Stop(Seconds(35.0));

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

    // ---------------------------------------------------------
    // APP 1: Terrestrial Path (Target: Porta 9)
    // ---------------------------------------------------------
    NS_LOG_INFO("Configuring Burst App 1 (Terrestrial 5G)");
    
    Ptr<BurstBufferedApplication> appBs = CreateObject<BurstBufferedApplication>();
    
    appBs->SetStartTime(Seconds(0.0));
    appBs->SetStopTime(Seconds(10.0));
    // Argomenti: DestAddress, Port, PacketSize, GenInterval, BurstInterval, Device, LocalIP
    appBs->Setup(InetSocketAddress(sinkIpViaBs, portBs), 
                 portBs,                                 
                 pktSize,              // Packet Size
                 Seconds(genInterval),    // Gen Interval
                 Seconds(burstInterval),      // Burst Interval
                 ue5gDevice);           // Bind IP
        //APPLICAZIONE TRAFFICO SOLO SU RETE TN, decommentare per visualizzare scenario con traffico separato TN e NTN                        
   //ue->AddApplication(appBs);

    // ---------------------------------------------------------
    // APP 2: Satellite Path (Target: Porta 10)
    // ---------------------------------------------------------
    NS_LOG_INFO("Configuring Burst App 2 (Satellite)");
    Ptr<BurstBufferedApplication> appSat = CreateObject<BurstBufferedApplication>();
    appSat->SetStartTime(Seconds(0.0)); 
    appSat->SetStopTime(Seconds(10.0));
    appSat->Setup(InetSocketAddress(sinkIpViaSat, portSat), 
        portSat, 
        pktSize,             // Packet Size
        Seconds(genInterval),    // Gen Interval
        Seconds(burstInterval),    // Burst Interval
        ueSatDevice);          // Bind IP
            
    //APPLICAZIONE TRAFFICO SOLO SU RETE NTN, decommentare per visualizzare scenario con traffico separato TN e NTN
   // ue->AddApplication(appSat);

    // ---------------------------------------------------------
    // APP 3: Terrestrial + Satellite (Target: Porta 9 e Porta 10)
    // ---------------------------------------------------------
    NS_LOG_INFO("Configuring Burst App 3 (Splitter TN + NTN)");
    Ptr<BurstBufferedSplitterApplication> appSplitter = CreateObject<BurstBufferedSplitterApplication>();
    appSplitter->Setup(
        sinkIpViaBs,
        sinkIpViaSat,
        portBs,
        portSat,
        pktSize,
        Seconds(genInterval), 
        Seconds(burstInterval), 
        Seconds(maxDelay),
        ue5gDevice,
        ueSatDevice
                );
     appSplitter->SetStartTime(Seconds(0.0));
    appSplitter->SetStopTime(Seconds(10.0));     
    //APPLICAZIONE SPLITTER TRAFFICO SU TN E NTN, decommentare per visualizzare scenario con splitter TN e NTN                                                  
    ue->AddApplication(appSplitter);

    

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
    Ptr<PacketSink> sinkPtrBs = DynamicCast<PacketSink>(sinkAppsBs.Get(0));
    Ptr<PacketSink> sinkPtrSat = DynamicCast<PacketSink>(sinkAppsSat.Get(0));

    // 3. Scheduliamo il monitoraggio ogni 0.1 secondi

    //DECOMMENTA QUESTO SE VUOI VEDERE SCENARI SEPARATI TN E NTN
   //Simulator::Schedule(Seconds(0.0), &MonitorThroughput, appBs, appSat, sinkPtrBs, sinkPtrSat, 0.1);

    //DECOMMENTA QUESTO SE VUOI VEDERE LO SPLITTER
    Simulator::Schedule(Seconds(0.0), &SplitterMonitorThroughput, appSplitter, sinkPtrBs, sinkPtrSat, 0.1);

    // ---------------------------------------------------------
    // Run & Log
    // ---------------------------------------------------------
    
    // Log posizione (commentato se non serve sempre)
    //LogNodePositionAndThroughput(ue, "UE");

    Simulator::Stop(Seconds(15.0)); 
    Simulator::Run();

    // Log Statistiche Finali
    //DECOMMENTA QUESTO SE VUOI VEDERE STATISTICHE SEPARATE TN E NTN
    //LogTrafficStats(appBs, appSat, sinkPtrBs, sinkPtrSat);

    //DECOMMENTA QUESTO SE VUOI VEDERE STATISTICHE SPLITTER TN E NTN
    SplitterLogTrafficStats(appSplitter, sinkPtrBs, sinkPtrSat);

    // Chiudiamo il file e generiamo lo script plot
    g_throughputTrace.close();
    GenerateGnuplotScript();

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


void LogTrafficStats(Ptr<BurstBufferedApplication> appBs,
                     Ptr<BurstBufferedApplication> appSat,
                     Ptr<PacketSink> sinkBs,   // <--- Sink dedicato alla porta BS (es. 9)
                     Ptr<PacketSink> sinkSat)  // <--- Sink dedicato alla porta SAT (es. 10)
{
    //double toMB = 1.0 / (1024.0 * 1024.0);
    double toMB = 1.0 / (1e6);
    
    // ==========================================
    // 1. ANALISI FLUSSO TERRESTRE (BS)
    // ==========================================
    double genBsMB  = appBs->GetGeneratedBytes() * toMB;
    double sentBsMB = appBs->GetSentBytes() * toMB;
    double rxBsMB   = (sinkBs->GetTotalRx() - pktSize) * toMB; //SOTTRARRE PACCHETTO DI WARMUP USATO PER SPORCARE LA RETE
    
    double backlogBsMB = genBsMB - sentBsMB; // Rimasti nel buffer app
    double lossBsMB    = sentBsMB - rxBsMB;  // Persi nella rete

    // Percentuale di perdita di rete (Network Packet Loss Rate)
    double lossPercBs = (sentBsMB > 0) ? (lossBsMB / sentBsMB) * 100.0 : 0.0;


    // ==========================================
    // 2. ANALISI FLUSSO SATELLITARE (SAT)
    // ==========================================
    double genSatMB  = appSat->GetGeneratedBytes() * toMB;
    double sentSatMB = appSat->GetSentBytes() * toMB;
    double rxSatMB   = (sinkSat->GetTotalRx() - pktSize) * toMB;

    double backlogSatMB = genSatMB - sentSatMB; // Rimasti nel buffer app
    double lossSatMB    = sentSatMB - rxSatMB;  // Persi nella rete

    // Percentuale di perdita di rete
    double lossPercSat = (sentSatMB > 0) ? (lossSatMB / sentSatMB) * 100.0 : 0.0;


    // ==========================================
    // 3. STAMPA REPORT
    // ==========================================
    std::cout << "\n========= DETAILED TRAFFIC REPORT =========\n";

    // --- Report BS ---
    std::cout << "[BS - Terrestrial Path]\n";
    std::cout << "  Generated by App:      " << genBsMB << " MB\n";
    std::cout << "  Buffered (Not Sent):   " << (backlogBsMB > 0 ? backlogBsMB : 0) << " MB\n";
    std::cout << "  Sent to Network:       " << sentBsMB << " MB\n";
    std::cout << "  Received at Sink:      " << rxBsMB << " MB\n";
    std::cout << "  NETWORK LOSS:          " << (lossBsMB > 0 ? lossBsMB : 0) << " MB";
    std::cout << " (" << lossPercBs << "%)\n";

    // --- Report SAT ---
    std::cout << "\n[SAT - Satellite Path]\n";
    std::cout << "  Generated by App:      " << genSatMB << " MB\n";
    std::cout << "  Buffered (Not Sent):   " << (backlogSatMB > 0 ? backlogSatMB : 0) << " MB\n";
    std::cout << "  Sent to Network:       " << sentSatMB << " MB\n";
    std::cout << "  Received at Sink:      " << rxSatMB << " MB\n";
    std::cout << "  NETWORK LOSS:          " << (lossSatMB > 0 ? lossSatMB : 0) << " MB";
    std::cout << " (" << lossPercSat << "%)\n";

    // --- Totali ---
    std::cout << "-------------------------------------------\n";
    std::cout << "TOTAL Network Loss:      " << (lossBsMB + lossSatMB) << " MB\n";
    std::cout << "===========================================\n";
}

//IDENTICO AL METODO LogTrafficStats MA RIFERITO ALL'APP SPLITTER, CHE HA STATISTICHE AGGREGATE PER TN E NTN, NON SEPARATE
void SplitterLogTrafficStats(Ptr<BurstBufferedSplitterApplication> app,
                     Ptr<PacketSink> sinkBs,   // <--- Sink dedicato alla porta BS (es. 9)
                     Ptr<PacketSink> sinkSat)  // <--- Sink dedicato alla porta SAT (es. 10)
{
    //double toMB = 1.0 / (1024.0 * 1024.0);
    double toMB = 1.0 / (1e6);

  
    double genMB  = app->GetGeneratedBytes() * toMB; // Totale generato dall'app (da dividere tra BS e SAT)

    double sentBsMB = app->GetSentBytesTN() * toMB; //Totale inviato sulla parte terrestre (TN)
    double sentSatMB = app->GetSentBytesNTN() * toMB; //Totale inviato sulla parte satellitare (NTN)

    double rxBsMB   = (sinkBs->GetTotalRx() - pktSize) * toMB; // Totale ricevuto al sink dalla parte terrestre (BS)
    double rxSatMB   = (sinkSat->GetTotalRx() - pktSize) * toMB; // Totale ricevuto al sink dalla parte satellitare (SAT)

    double backlogMB = genMB - sentBsMB - sentSatMB; // Rimasti nel buffer app
    double lossBsMB    = sentBsMB - rxBsMB;  // Persi nella rete sulla parte terrestre (BS)
    double lossSatMB    = sentSatMB - rxSatMB;  // Persi nella rete sulla parte satellitare (SAT)

    // Percentuale di perdita di rete (Network Packet Loss Rate) - per la parte terrestre (BS)
    double lossPercBs = (sentBsMB > 0) ? (lossBsMB / sentBsMB) * 100.0 : 0.0;

    // Percentuale di perdita di rete (Network Packet Loss Rate) - per la parte satellitare (SAT)
    double lossPercSat = (sentSatMB > 0) ? (lossSatMB / sentSatMB) * 100.0 : 0.0;

    double coefficientSteering = app->GetSteeringCoefficient();
    // ==========================================
    // 3. STAMPA REPORT
    // ==========================================
    std::cout << "\n========= DETAILED TRAFFIC REPORT =========\n";

    std::cout << "  Generated by App:      " << genMB << " MB\n";
    std::cout << "  Buffered (Not Sent):   " << (backlogMB > 0 ? backlogMB : 0) << " MB\n";
    std::cout << " Average of Steering Coefficient:  " << coefficientSteering << "\n";
        // --- Report BS ---

    std::cout << "[BS - Terrestrial Path]\n";
    std::cout << "  Sent to Network:       " << sentBsMB << " MB\n";
    std::cout << "  Received at Sink:      " << rxBsMB << " MB\n";
    std::cout << "  NETWORK LOSS:          " << (lossBsMB > 0 ? lossBsMB : 0) << " MB";
    std::cout << " (" << lossPercBs << "%)\n";

    // --- Report SAT ---
    std::cout << "\n[SAT - Satellite Path]\n";
    std::cout << "  Sent to Network:       " << sentSatMB << " MB\n";
    std::cout << "  Received at Sink:      " << rxSatMB << " MB\n";
    std::cout << "  NETWORK LOSS:          " << (lossSatMB > 0 ? lossSatMB : 0) << " MB";
    std::cout << " (" << lossPercSat << "%)\n";

    // --- Totali ---
    std::cout << "-------------------------------------------\n";
    std::cout << "TOTAL Network Loss:      " << (lossBsMB + lossSatMB) << " MB\n";
    std::cout << "===========================================\n";
}
// --- Monitoraggio Throughput ---
void MonitorThroughput(Ptr<BurstBufferedApplication> appBs, Ptr<BurstBufferedApplication> appSat, 
                       Ptr<PacketSink> sinkBs, Ptr<PacketSink> sinkSat, 
                       double interval)
{
    double now = Simulator::Now().GetSeconds();
    double bufferBsMbits = 0;
    double bufferSatMbits = 0;
    double thRxBs = 0;
    double thRxSat = 0;
    double thSentBs = 0;
    double thSentSat = 0;
    // 1. Dati LATO SINK (Ricezione)
    uint64_t currentRxBs = sinkBs->GetTotalRx();
    uint64_t currentRxSat = sinkSat->GetTotalRx();

    // 2. Dati LATO APP (Invio effettivo del Burst)
    uint64_t currentSentBs = appBs->GetSentBytes();
    uint64_t currentSentSat = appSat->GetSentBytes();
    if(currentSentBs != 0 || currentSentSat != 0) {
        // 3. Dati BUFFER (Riempimento attuale)
        // Convertiamo i Bytes in Mbits per poterli confrontare graficamente con Mbps
        bufferBsMbits = (appBs->GetCurrentBufferBytes() * 8.0) / 1e6;
        bufferSatMbits = (appSat->GetCurrentBufferBytes() * 8.0) / 1e6;

        // 3. Calcolo Delta
        uint64_t deltaRxBs = currentRxBs - g_lastRxBytesBs;
        uint64_t deltaRxSat = currentRxSat - g_lastRxBytesSat;
        uint64_t deltaSentBs = currentSentBs - g_lastSentBytesBs;
        uint64_t deltaSentSat = currentSentSat - g_lastSentBytesSat;

        // 4. Conversione in Mbps
        thRxBs = (deltaRxBs * 8.0) / (1e6);
        thRxSat = (deltaRxSat * 8.0) / (1e6);
        thSentBs = (deltaSentBs * 8.0) / (1e6);
        thSentSat = (deltaSentSat * 8.0) / (1e6);
    }
    // 6. Scrittura su file: 
    // Col 1: Time
    // Col 2: Tx_5G (Mbps) | Col 3: Rx_5G (Mbps) 
    // Col 4: Tx_Sat (Mbps)| Col 5: Rx_Sat (Mbps)
    // Col 6: Buffer_5G (Mbits) | Col 7: Buffer_Sat (Mbits)
    g_throughputTrace << now << " " 
                      << thSentBs << " " << thRxBs << " " 
                      << thSentSat << " " << thRxSat << " "
                      << bufferBsMbits << " " << bufferSatMbits << std::endl;

    // 7. Aggiornamento Memoria
    g_lastRxBytesBs = currentRxBs;
    g_lastRxBytesSat = currentRxSat;
    g_lastSentBytesBs = currentSentBs;
    g_lastSentBytesSat = currentSentSat;

    // Rischedula
    Simulator::Schedule(Seconds(interval), &MonitorThroughput, appBs, appSat, sinkBs, sinkSat, interval);
}
void SplitterMonitorThroughput(Ptr<BurstBufferedSplitterApplication> app, 
                       Ptr<PacketSink> sinkBs, Ptr<PacketSink> sinkSat, 
                       double interval)
{
    double now = Simulator::Now().GetSeconds();

    
    // 1. Dati LATO SINK (Ricezione)
    uint64_t currentRxBs = sinkBs->GetTotalRx(); 
    uint64_t currentRxSat = sinkSat->GetTotalRx();

    // 2. Dati LATO APP (Invio effettivo del Burst)
    uint64_t currentSentBs = app->GetSentBytesTN();
    uint64_t currentSentSat = app->GetSentBytesNTN();

    double thRxBs = 0;
    double thRxSat =  0;
    double thSentBs = 0;
    double thSentSat = 0;
    double bufferMbits = 0;

    if(currentSentBs != 0 || currentSentSat != 0) {
      
     // 3. Dati BUFFER (Riempimento attuale)
    // Convertiamo i Bytes in Mbits per poterli confrontare graficamente con Mbps

     bufferMbits = (app->GetCurrentBufferBytes() * 8.0) / 1e6;
    // 3. Calcolo Delta
    uint64_t deltaRxBs = currentRxBs - g_lastRxBytesBs;
    uint64_t deltaRxSat = currentRxSat - g_lastRxBytesSat;
    uint64_t deltaSentBs = currentSentBs - g_lastSentBytesBs;
    uint64_t deltaSentSat = currentSentSat - g_lastSentBytesSat;

    // 4. Conversione in Mbps
     thRxBs = (deltaRxBs * 8.0) / (1e6);
     thRxSat = (deltaRxSat * 8.0) / (1e6);
     thSentBs = (deltaSentBs * 8.0) / (1e6);
     thSentSat = (deltaSentSat * 8.0) / (1e6);
    }
    // 6. Scrittura su file: 
    // Col 1: Time
    // Col 2: Tx_5G (Mbps) | Col 3: Rx_5G (Mbps) 
    // Col 4: Tx_Sat (Mbps)| Col 5: Rx_Sat (Mbps)
    // Col 6: Buffer_5G (Mbits) | Col 7: Buffer_Sat (Mbits)
    /*g_throughputTrace << now << " " 
                      << thSentBs << " " << thRxBs << " " 
                      << thSentSat << " " << thRxSat << " "
                      << bufferBsMbits << " " << bufferSatMbits << std::endl;
*/
    g_throughputTrace << now << " " 
                      << thSentBs << " " << thRxBs << " " 
                      << thSentSat << " " << thRxSat << " "
                      << bufferMbits << " " << std::endl;
    // 7. Aggiornamento Memoria
    g_lastRxBytesBs = currentRxBs;
    g_lastRxBytesSat = currentRxSat;
    g_lastSentBytesBs = currentSentBs;
    g_lastSentBytesSat = currentSentSat;

    // Rischedula
    Simulator::Schedule(Seconds(interval), &SplitterMonitorThroughput, app, sinkBs, sinkSat, interval);
}

// --- Generazione Script Gnuplot ---
void GenerateGnuplotScript()
{
    std::string pltPath = g_outputDir + "/plot_throughput.plt";
    std::string datPath = g_outputDir + "/throughput_trace.dat";
    std::string png5g = g_outputDir + "/throughput_5g.png";
    std::string pngSat = g_outputDir + "/throughput_sat.png";
    std::string pngBuffer = g_outputDir + "/buffer_occupancy.png"; 

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
    plotFile << "set title '5G Channel: Burst (Tx) vs Sink Received (Rx)'" << std::endl;
    plotFile << "set ylabel 'Throughput (Mbps)' textcolor rgb 'black'" << std::endl;
    
    // Disabilita asse Y destro per pulizia
    plotFile << "unset y2tics" << std::endl;
    plotFile << "unset y2label" << std::endl;
    plotFile << "set ytics mirror" << std::endl;

    plotFile << "plot '" << datPath << "' using 1:2 title 'Burst Tx Rate' lw 2 lc rgb 'blue', \\" << std::endl;
    plotFile << "     '" << datPath << "' using 1:3 title 'Sink Rx Rate' lw 2 lc rgb 'green'" << std::endl;

    // --- GRAFICO 2: Satellite (Solo Throughput) ---
    plotFile << "set output '" << pngSat << "'" << std::endl;
    plotFile << "set title 'Satellite Channel: Burst (Tx) vs Sink Received (Rx)'" << std::endl;

    plotFile << "plot '" << datPath << "' using 1:4 title 'Burst Tx Rate' lw 2 lc rgb 'red', \\" << std::endl;
    plotFile << "     '" << datPath << "' using 1:5 title 'Sink Rx Rate' lw 2 lc rgb 'orange'" << std::endl;

    // --- GRAFICO 3: Buffer Occupancy Comparison ---
    plotFile << "set output '" << pngBuffer << "'" << std::endl;
    plotFile << "set title 'Buffer Occupancy'" << std::endl;
    plotFile << "set ylabel 'Buffer Occupancy (Mbits)' textcolor rgb 'black'" << std::endl;

    plotFile << "plot '" << datPath << "' using 1:6 title 'App Buffer' lw 2 lc rgb 'blue', \\" << std::endl;

    plotFile.close();

  NS_LOG_INFO("Gnuplot script generated at: " << pltPath);
    std::string gnuplotCmd = "gnuplot " + pltPath;
int ret = system(gnuplotCmd.c_str());
    system(("feh " + png5g + " " + pngSat + " " + pngBuffer + " &").c_str());
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