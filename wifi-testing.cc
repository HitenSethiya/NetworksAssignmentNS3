/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015, IMDEA Networks Institute
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Arkadeep Das <arkadeep2k17@hotmail.com>
.*
 * This is a simple example to test TCP over 802.11n (with MPDU aggregation enabled).
 *
 * Network topology:
 *
 *   Ap    STA
 *   *      *
 *   |      |
 *   n1     n2
 *
 * In this example, an HT station sends TCP packets to the access point.
 * We report the total throughput received during a window of 100ms.
 * The user can specify the application data rate and choose the variant
 * of TCP i.e. congestion control algorithm to use.
 */

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/tcp-westwood.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <ns3/flow-monitor-module.h>
#include <iomanip>
#include <vector>

NS_LOG_COMPONENT_DEFINE ("wifi-tcp");

using namespace ns3;

Ptr<PacketSink> sink;                         /* Pointer to the packet sink application */
uint64_t lastTotalRx = 0;                     /* The value of the last total received bytes */

void
CalculateThroughput ()
{
  Time now = Simulator::Now ();                                         /* Return the simulator's virtual time. */
  double cur = (sink->GetTotalRx () - lastTotalRx) * (double) 8 / 1e5;     /* Convert Application RX Packets to MBits. */
  std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  // std::cout << cur << "hiten       "<<lastTotalRx;
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
}


void ThroughputMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> flowMon)
  { 
    flowMon->CheckForLostPackets(); 
    std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
    Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin (); stats != flowStats.end (); ++stats)
    { 
      Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow (stats->first);
      std::cout<<"Flow ID     : " << stats->first <<" ; "<< fiveTuple.sourceAddress <<" -----> "<<fiveTuple.destinationAddress<<std::endl;
      std::cout<<"Tx Packets = " << stats->second.txPackets<<std::endl;
      std::cout<<"Rx Packets = " << stats->second.rxPackets<<std::endl;
      std::cout<<"Duration    : "<<stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds()<<std::endl;
      std::cout<<"Last Received Packet  : "<< stats->second.timeLastRxPacket.GetSeconds()<<" Seconds"<<std::endl;
      std::cout<<"Throughput: " << stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds())/1024/1024 << " Mbps"<<std::endl;
      std::cout<<"---------------------------------------------------------------------------"<<std::endl;
    } 

  flowMon->SerializeToXmlFile("lab-5.flowmon", true, true);
  Simulator::Schedule(MilliSeconds(100),&ThroughputMonitor, fmhelper, flowMon);

      
  }

int
main (int argc, char *argv[])
{
  uint32_t payloadSize = 1472;                       /* Transport layer payload size in bytes. */
  std::string dataRate = "100Mbps";                  /* Application layer datarate. */
  std::string tcpVariant = "TcpHybla";             /* TCP variant type. */
  std::string phyRate = "HtMcs7";                    /* Physical layer bitrate. */
  double simulationTime = 10;
  bool enableRts = 0;
  bool verifyResults = 0; //used for regression
  bool pcapTracing = false;                          /* PCAP Tracing is enabled or not. */

  /* Command line argument parser setup. */
  CommandLine cmd;
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("dataRate", "Application data ate", dataRate);
  cmd.AddValue ("tcpVariant", "Transport protocol to use: TcpNewReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ", tcpVariant);
  cmd.AddValue ("phyRate", "Physical layer bitrate", phyRate);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("pcap", "Enable/disable PCAP Tracing", pcapTracing);
  cmd.AddValue ("enableRts", "Enable or disable RTS/CTS", enableRts);
  cmd.AddValue ("verifyResults", "Enable/disable results verification at the end of the simulation", verifyResults);
  cmd.Parse (argc, argv);
  tcpVariant = std::string ("ns3::") + tcpVariant;

  std::cout << "\nTCPVariant: " << tcpVariant << " Mbit/s" << std::endl;

  /* No fragmentation and no RTS/CTS */
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("999999"));

  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", enableRts ? StringValue ("0") : StringValue ("999999"));
  // Select TCP variant
  if (tcpVariant.compare ("ns3::TcpWestwoodPlus") == 0)
    { 
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (tcpVariant, &tcpTid), "TypeId " << tcpVariant << " not found");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (tcpVariant)));
    }

  /* Configure TCP Options */
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));

  WifiMacHelper wifiMac;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

  /* Set up Legacy Channel */
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (5e9));

  /* Setup Physical Layer */
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.Set ("TxPowerStart", DoubleValue (10.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (10.0));
  wifiPhy.Set ("TxPowerLevels", UintegerValue (1));
  wifiPhy.Set ("TxGain", DoubleValue (0));
  wifiPhy.Set ("RxGain", DoubleValue (0));
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (10));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-79));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-79 + 3));
  wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");
  wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue (phyRate),
                                      "ControlMode", StringValue ("HtMcs0"));

  NodeContainer networkNodes;
  networkNodes.Create (3);
  Ptr<Node> staWifiTransNode0 = networkNodes.Get (0);
  Ptr<Node> staWifiTransNode2 = networkNodes.Get (2);
  Ptr<Node> staWifiRecvNode1 = networkNodes.Get (1);

  
  Ssid ssid = Ssid("network");
  
  /* Configure AP */
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer recvDevice1;
  recvDevice1 = wifiHelper.Install (wifiPhy, wifiMac, staWifiRecvNode1);

  /* Configure STA */
  
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer senders0,senders2;
  senders2 = wifiHelper.Install (wifiPhy, wifiMac, staWifiTransNode2);
  senders0 = wifiHelper.Install (wifiPhy, wifiMac, staWifiTransNode0);


  /* Mobility model */
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (0.25, 0.0, 0.0));
  positionAlloc->Add (Vector (0.50, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (networkNodes.Get (0));
  mobility.Install (networkNodes.Get (1));
  mobility.Install (networkNodes.Get (2));

  /* Internet stack */
  InternetStackHelper stack;
  stack.Install (networkNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface0,staInterface1,staInterface2;
  staInterface1 = address.Assign (recvDevice1);
  staInterface0 = address.Assign (senders0);
  staInterface2 = address.Assign (senders2);
  std::cout<<"Addresses : "<<staInterface0.GetAddress (0) <<"   "<<staInterface1.GetAddress (0) <<"  "<<staInterface2.GetAddress (0)<<"\n";

  /* Populate routing table */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* Install TCP Receiver on the access point */
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
  ApplicationContainer sinkApp = sinkHelper.Install (staWifiRecvNode1);
  sink = StaticCast<PacketSink> (sinkApp.Get(0)); 

  /* Install TCP/UDP Transmitter on the station */
  OnOffHelper server0 ("ns3::TcpSocketFactory", (InetSocketAddress (staInterface1.GetAddress (0), 9)));
  server0.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  server0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  server0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  server0.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));

  /* Install TCP/UDP Transmitter on the station */
  OnOffHelper server2 ("ns3::TcpSocketFactory", (InetSocketAddress (staInterface1.GetAddress (0), 9)));
  server2.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  server2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  server2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  server2.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));

  ApplicationContainer serverApp0 = server0.Install (staWifiTransNode0);
  ApplicationContainer serverApp2 = server2.Install (staWifiTransNode2);
  /* Start Applications */
  sinkApp.Start (Seconds (0.0));
  serverApp0.Start (Seconds (1.0));
  serverApp2.Start(Seconds (2.0));
  // Simulator::Schedule (Seconds (2.1), &CalculateThroughput);

//------Throughput Monitor--------------------------------------------------------------------------------------
  FlowMonitorHelper fmHelper;
  Ptr<FlowMonitor> allMon = fmHelper.Install(networkNodes);


  allMon->SetAttribute("DelayBinWidth", DoubleValue(0.001));
        allMon->SetAttribute("JitterBinWidth", DoubleValue(0.001));
        allMon->SetAttribute("PacketSizeBinWidth", DoubleValue(20)); 


  /* Enable Traces */
  if (pcapTracing)
    {
      wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      wifiPhy.EnablePcap ("AccessPoint", recvDevice1);
      wifiPhy.EnablePcap ("Station0", senders0);
      wifiPhy.EnablePcap ("Station2", senders2);
    }

  /* Start Simulation */
  Simulator::Stop (Seconds (simulationTime + 2));
  ThroughputMonitor(&fmHelper ,allMon);
  Simulator::Run ();
  Simulator::Destroy ();  

  // double averageThroughput = ((sink->GetTotalRx () * 8) / (1e6  * simulationTime));
  // if (averageThroughput < 50)
  //   {
  //     NS_LOG_ERROR ("Obtained throughput is not in the expected boundaries!");
  //     exit (1);
  //   }
  // std::cout << "\nAverage throughput: " << averageThroughput << " Mbit/s" << std::endl;
  return 0;
}
