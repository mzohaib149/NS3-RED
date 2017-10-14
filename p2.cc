
#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/random-variable-stream.h"
#include "ns3/point-to-point-dumbbell.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("p2");

int main (int argc, char *argv[])
{
	SeedManager::SetSeed (11223344);
   	uint32_t maxBytes = 100000000;
 	uint32_t queueSize;//ideally twice the delay bandwidth product ~5000 bytes
  	uint32_t segSize;
  	uint32_t windowSize;//ideally equal to delay bandwidth product ~2500 bytes
  	uint16_t nFlows=1;
  	std::string TCPFlavor;	
  	uint32_t tcpType;
  	uint32_t queueType;
  	std::string queuingDiscipline;
  	std::string rtt="10";
  	uint32_t minTh;
  	uint32_t maxTh;
  	std::string udpRate="500";
  	uint32_t noUdp=1;	
  	uint32_t bulkSend=1;

  	CommandLine cmd;
  	cmd.AddValue ("nFlows", "Are there 1 or 10 flows", nFlows);	
  	cmd.AddValue ("queueSize", "Maximum Bytes allowed in queue", queueSize);
  	cmd.AddValue ("segSize", "Maximum TCP segment size", segSize);//changing TCP segment size creates burstiness along with high rtt
  	cmd.AddValue ("windowSize", "Maximum receiver advertised window size", windowSize);
  	cmd.AddValue ("tcpType", "TCP type", tcpType);//Tahoe or Reno
  	cmd.AddValue ("queueType", "RED or Droptail", queueType);//0 for droptail 1 for Red
  	cmd.AddValue ("rtt", "Round Trip Time", rtt); 
  	cmd.AddValue ("udpRate", "UDP Rate", udpRate);
  	cmd.AddValue ("noUdp", "no UDP connection", noUdp);	
 	cmd.AddValue ("bulkSend", "BulkSend or OnOff", bulkSend);//1 for bulksend 0 for on off

  	cmd.Parse (argc, argv);
  
  	Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (segSize)); 	
  	Config::SetDefault("ns3::TcpSocketBase::MaxWindowSize", UintegerValue (windowSize));

  	minTh = 0.25 * queueSize;
  	maxTh = 0.75 * queueSize;

  	if (tcpType==0)
		TCPFlavor="Tahoe";
  	else if (tcpType==1)
		TCPFlavor="Reno";

	if (queueType==0)
	{
		queuingDiscipline="ns3::DropTailQueue";
		Config::SetDefault ("ns3::DropTailQueue::Mode", StringValue ("QUEUE_MODE_BYTES"));
		Config::SetDefault ("ns3::DropTailQueue::MaxBytes", UintegerValue (queueSize));
	}

	else if (queueType==1)
	{
		queuingDiscipline="ns3::RedQueue";
		Config::SetDefault ("ns3::RedQueue::Mode", StringValue ("QUEUE_MODE_BYTES"));
		Config::SetDefault ("ns3::RedQueue::QueueLimit", UintegerValue (queueSize));
		Config::SetDefault ("ns3::RedQueue::MinTh", DoubleValue (minTh));
		Config::SetDefault ("ns3::RedQueue::MaxTh", DoubleValue (maxTh));
		Config::SetDefault ("ns3::RedQueue::LInterm", DoubleValue (50));

	}


	std::string temp = "ns3::Tcp"+TCPFlavor;
	Config::SetDefault("ns3::TcpL4Protocol::SocketType",StringValue(temp));	
	Config::SetDefault ("ns3::TcpSocketBase::WindowScaling", BooleanValue(false));

 	PointToPointHelper pointToPointRouter;
 	pointToPointRouter.SetDeviceAttribute  ("DataRate", StringValue ("1Mbps"));
 	pointToPointRouter.SetChannelAttribute ("Delay", StringValue (rtt+"ms"));

	if (queueType==0)
	{
		pointToPointRouter.SetQueue ("ns3::DropTailQueue","MaxBytes",UintegerValue(queueSize));
	}
	else if (queueType==1)
	{
		pointToPointRouter.SetQueue ("ns3::RedQueue","MinTh",DoubleValue(minTh),"MaxTh",DoubleValue(maxTh),"QueueLimit",UintegerValue(queueSize),"LInterm", DoubleValue (50));
	}
 	
	PointToPointHelper pointToPointLeaf;
 	pointToPointLeaf.SetDeviceAttribute("DataRate", StringValue ("10Mbps"));

 	pointToPointLeaf.SetChannelAttribute("Delay", StringValue ("1ms"));
	  
 	PointToPointDumbbellHelper d (nFlows, pointToPointLeaf, nFlows, pointToPointLeaf, pointToPointRouter);
   
      	// Install Stack
      	InternetStackHelper stack;
      	d.InstallStack (stack);
    
      	// Assign IP Addresses
      	d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                             Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                             Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));
    

 	
 	NS_LOG_INFO("Populate Routing Tables"); 
  	Ipv4GlobalRoutingHelper::PopulateRoutingTables(); 	

  	NS_LOG_INFO ("Create Applications.");


 

  	uint16_t port = 9;  // well-known echo port number
	std::vector<Ptr<PacketSink> > sinks;
	std::vector<double> starttime;

	if (noUdp==0)
	{ 
 		for (uint16_t i=0; i<nFlows; i++)
 		{
 
  			if (i<nFlows-1) //on all flows except the last set TCP
  			{
  
  				if (bulkSend == 1)
    				{

   					BulkSendHelper source ("ns3::TcpSocketFactory",InetSocketAddress (d.GetRightIpv4Address(i), port+i));

   					source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
   					source.SetAttribute ("SendSize", UintegerValue (segSize));
   					ApplicationContainer sourceApps;
   					sourceApps.Add(source.Install (d.GetLeft (i)));
					sourceApps.Start (Seconds (0.0));
					sourceApps.Stop (Seconds (100.0));
   				}

				else if (bulkSend == 0)
  				{
	 				OnOffHelper source ("ns3::TcpSocketFactory",
                         		InetSocketAddress (d.GetRightIpv4Address(i), port+i));

   					source.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.0,Max=1.0]"));
    					source.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.0,Max=1.0]"));
	 				ApplicationContainer sourceApps;
   					sourceApps.Add(source.Install (d.GetLeft (i)));
					sourceApps.Start (Seconds (0.0));
					sourceApps.Stop (Seconds (100.0));
				 }  		

				starttime.push_back(0.0);

  				PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         	InetSocketAddress (Ipv4Address::GetAny (), port+i));
				AddressValue addr (InetSocketAddress(d.GetRightIpv4Address(i), i+port));
    				sink.SetAttribute("Local", addr);
  				ApplicationContainer sinkApps;
  				sinkApps.Add(sink.Install(d.GetRight(i)));
  				sinkApps.Start (Seconds (0.0));
  				sinkApps.Stop (Seconds (100.0));
				sinks.push_back(DynamicCast<PacketSink> (sinkApps.Get (0)));
			}

			else //for last flow set udp
			{
   				OnOffHelper udpsource ("ns3::UdpSocketFactory",InetSocketAddress (d.GetRightIpv4Address(i), port+i));
  				std::string rate = udpRate+"kb/s";
  				udpsource.SetConstantRate (DataRate(rate));
  				udpsource.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); 
  				udpsource.SetAttribute("OffTime",StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));	
  				ApplicationContainer sourceApps;
  				sourceApps.Add(udpsource.Install (d.GetLeft (i)));

 				sourceApps.Start (Seconds (0.0));
				sourceApps.Stop (Seconds (100.0));
				starttime.push_back(0.0);

  				PacketSinkHelper udpsink ("ns3::UdpSocketFactory",InetSocketAddress (Ipv4Address::GetAny (), port+i));
				AddressValue addr (InetSocketAddress(d.GetRightIpv4Address(i), i+port));
    				udpsink.SetAttribute("Local", addr);

  				ApplicationContainer sinkApps;
  				sinkApps.Add(udpsink.Install(d.GetRight(i)));
  				sinkApps.Start (Seconds (0.0));
  				sinkApps.Stop (Seconds (100.0));
				sinks.push_back(DynamicCast<PacketSink> (sinkApps.Get (0)));
			}

 		}

	}

	else //no udp
	{
  		for (uint16_t i=0; i<nFlows; i++)
  		{
	
    			if (bulkSend == 1)
    			{

   				BulkSendHelper source ("ns3::TcpSocketFactory",InetSocketAddress (d.GetRightIpv4Address(i), port+i));

   				source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
   				source.SetAttribute ("SendSize", UintegerValue (segSize));
    				ApplicationContainer sourceApps;
   				sourceApps.Add(source.Install (d.GetLeft (i)));
				sourceApps.Start (Seconds (0.0));
				sourceApps.Stop (Seconds (100.0));
			 }

  			else if (bulkSend == 0)
  			{
	 			OnOffHelper source ("ns3::TcpSocketFactory",InetSocketAddress (d.GetRightIpv4Address(i), port+i));

   				source.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.0,Max=1.0]"));
    				source.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.0,Max=1.0]"));
	 			ApplicationContainer sourceApps;
   				sourceApps.Add(source.Install (d.GetLeft (i)));
				sourceApps.Start (Seconds (0.0));
				sourceApps.Stop (Seconds (100.0));
  			}  		
  
 			starttime.push_back(0.0);
  			PacketSinkHelper sink ("ns3::TcpSocketFactory",
                        InetSocketAddress (Ipv4Address::GetAny (), port+i));
   			AddressValue addr (InetSocketAddress(d.GetRightIpv4Address(i), port+i));
    			sink.SetAttribute("Local", addr);
  			ApplicationContainer sinkApps;
  			sinkApps.Add(sink.Install(d.GetRight(i)));
  			sinkApps.Start (Seconds (0.0));
  			sinkApps.Stop (Seconds (100.0));
  			sinks.push_back(DynamicCast<PacketSink> (sinkApps.Get (0)));
 		}

	}

  	NS_LOG_INFO ("Run Simulation.");
  	Simulator::Stop (Seconds (100.0));
  	Simulator::Run ();

	double total =0;
	double average =0;
	for (uint16_t j=0; j<nFlows; j++)
	{
        
		std::cout<<"tcp,"<<tcpType; 
		std::cout<<",flow,"<<j<<",";
		std::cout<<"windowSize,"<<windowSize<<","<<"queueSize,"<<queueSize<<","<<"segSize,"<<segSize<<",";
		Ptr<PacketSink> sink1 = sinks.at(j);
		std::cout<<"goodput in kbps,"<<(sink1->GetTotalRx()*8.0)/((100.0-starttime.at(j))*1000.0)<<std::endl;
		total = total + (sink1->GetTotalRx()*8.0)/((100.0-starttime.at(j))*1000.0);
	}

	average= total/nFlows;
	std::cout<<"average goodput in kbps,"<<average<<std::endl;
  	Simulator::Destroy ();
  	NS_LOG_INFO ("Done.");

}
