#include <iostream>  
#include <fstream>  
#include <string>
#include <cassert>  
      
#include "ns3/core-module.h"  
#include "ns3/network-module.h"  
#include "ns3/internet-module.h"  
#include "ns3/point-to-point-module.h"  
#include "ns3/applications-module.h"  
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/traffic-control-module.h"
      
using namespace ns3;
using namespace std;
      
NS_LOG_COMPONENT_DEFINE ("TcpImplementationHybla");  
      
int  
main (int argc, char *argv[])  
{

  bool flow_monitor = true;  
  Time::SetResolution (Time::NS);  
  //LogComponentEnable ("BottleNeckTcpScriptExample", LOG_LEVEL_INFO);  
  //LogComponentEnable ("TcpL4Protocol", LOG_LEVEL_INFO);  
  //LogComponentEnable ("TcpSocketImpl", LOG_LEVEL_ALL);  
  //LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  std::string redLinkDataRate = "100Mbps";
  std::string redLinkDelaySender= "2ms";
  std::string redLinkDelayReceiver= "1ms";

  CommandLine cmd;  
  cmd.Parse (argc,argv);
      
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpHybla::GetTypeId ()));
  Config::SetDefault ("ns3::TcpHybla::RRTT", TimeValue (MilliSeconds (10)));
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1024));  
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("100Mbps"));

  NS_LOG_INFO ("Set RED params");
  Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_DISC_MODE_PACKETS"));
  Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (5));
  Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (15));
      
  NodeContainer nodes;  
  nodes.Create (4);
      
  vector<NodeContainer> nodeAdjacencyList(3);  
  nodeAdjacencyList[0]=NodeContainer(nodes.Get(0),nodes.Get(3));  
  nodeAdjacencyList[1]=NodeContainer(nodes.Get(1),nodes.Get(3));  
  nodeAdjacencyList[2]=NodeContainer(nodes.Get(2),nodes.Get(3));   
      
  vector<PointToPointHelper> pointToPoint(3);  
  pointToPoint[0].SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPoint[0].SetChannelAttribute ("Delay", StringValue ("2ms"));
      
  pointToPoint[1].SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPoint[1].SetChannelAttribute ("Delay", StringValue ("2ms"));
      
  pointToPoint[2].SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPoint[2].SetChannelAttribute ("Delay", StringValue ("1ms"));

  InternetStackHelper stack;
  stack.Install (nodes);

  TrafficControlHelper tchRed, tchRedR;
  tchRed.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", StringValue (redLinkDataRate),
                           "LinkDelay", StringValue (redLinkDelaySender));
  tchRedR.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", StringValue (redLinkDataRate),
                           "LinkDelay", StringValue (redLinkDelayReceiver));

  vector<NetDeviceContainer> devices (3); 
  vector<QueueDiscContainer> queueDiscs (3);
  PointToPointHelper p2p;

  for(uint32_t i=0; i<3; i++)  
    {  
      devices[i] = pointToPoint[i].Install (nodeAdjacencyList[i]);
    }

  p2p.SetDeviceAttribute ("DataRate", StringValue (redLinkDataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (redLinkDelaySender));
  devices[0] = p2p.Install (nodeAdjacencyList[0]);
  queueDiscs[0] = tchRed.Install (devices[0]);

  p2p.SetDeviceAttribute ("DataRate", StringValue (redLinkDataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (redLinkDelaySender));
  devices[1] = p2p.Install (nodeAdjacencyList[1]);
  queueDiscs[1] = tchRed.Install (devices[1]);

  p2p.SetDeviceAttribute ("DataRate", StringValue (redLinkDataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (redLinkDelayReceiver));
  devices[2] = p2p.Install (nodeAdjacencyList[2]);
  queueDiscs[2] = tchRedR.Install (devices[2]);
      
  Ipv4AddressHelper address;  
  vector<Ipv4InterfaceContainer> interfaces(3);
  for(uint32_t i=0; i<3; i++)  
    {  
      ostringstream subset;  
      subset << "10.1." << i+1 << ".0";  
      address.SetBase(subset.str().c_str (),"255.255.255.0");
      interfaces[i]=address.Assign(devices[i]);
    }  
       
  uint16_t port = 50000;  
  ApplicationContainer sinkApp;  
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));  
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);  
  sinkApp.Add(sinkHelper.Install(nodeAdjacencyList[2].Get(0)));  
  sinkApp.Start (Seconds (0.0));  
  sinkApp.Stop (Seconds (2.0));  
      
  OnOffHelper onOffHelper ("ns3::TcpSocketFactory", Address ());  
  onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));  
  onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));  
      
  ApplicationContainer clientApps;

  AddressValue remoteAddress (InetSocketAddress (interfaces[2].GetAddress (0), port));  
  onOffHelper.SetAttribute("Remote",remoteAddress);  
  clientApps.Add(onOffHelper.Install(nodeAdjacencyList[0].Get(0)));  
      
  remoteAddress=AddressValue(InetSocketAddress (interfaces[2].GetAddress (0), port));  
  onOffHelper.SetAttribute("Remote",remoteAddress);  
  clientApps.Add(onOffHelper.Install(nodeAdjacencyList[1].Get(0)));  

  clientApps.Start (Seconds (0.5));  
  clientApps.Stop (Seconds (1.5));  
      
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();  

  FlowMonitorHelper flowHelper;
  if (flow_monitor)
    {
      flowHelper.InstallAll ();
    }

  Simulator::Stop (Seconds (3.0));  
  Simulator::Run ();  

  if (flow_monitor)
    {
      flowHelper.SerializeToXmlFile ("tcp-implementation-hybla.flowmonitor", true, true);
    }

  Simulator::Destroy ();  
  return 0;  
}  
