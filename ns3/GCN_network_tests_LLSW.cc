/*
 * Group Centric Networking
 *
 * Copyright (C) 2015 Massachusetts Institute of Technology
 * All rights reserved.
 *
 * Authors:
 *           Patricia Deutsch         <patricia.deutsch@ll.mit.edu>
 *           Gregory Kuperman         <gkuperman@ll.mit.edu>
 *           Leonid Veytser           <veytser@ll.mit.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/olsr-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/dce-module.h"
#include "ns3/simple-wireless-module.h"


// This file supports an N nodes scenario in which the
// nodes are placed in a circle of outer radius X.
// The user can choose to use simple wireless or WiFi for the radio model
// and NetPatterns, OLSR, or AODV for the routing.
//
// n0 is always the source node and is located at the origin of the circle
// n1-nx are possibly group nodes (i.e., receivers) if they are within the
// area specified by the user. This can be a circle of inner radius Y or
// a rectangle centered on the origin.
//
// The user also has the choice of constant position of random waypoint mobility

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("GCN_Test");

uint32_t count_sent = 0;
// These are only used by OLSR & AODV
uint32_t count_recv = 0;
uint32_t pkts_sent_data = 0;
uint32_t bytes_sent_data = 0;
uint32_t pkts_sent_cntl = 0;
uint32_t bytes_sent_cntl = 0;
uint32_t rteConvergeTime = 0;
#define ON_OFF_APP_PKT_SIZE  1400
#define ON_OFF_OTA_PKT_SIZE  ON_OFF_APP_PKT_SIZE + 28   // 28 bytes get added to packet when sent OTA

Timer m_PositionTimer;

// ******************************************************************
// This is used when running GCN on Simple Wireless
// ******************************************************************
static void TxBeginSW (Ptr<const Packet> p, Mac48Address from, Mac48Address to , uint16_t protocol)
{
  //std::cout << Simulator::Now ().GetSeconds () << " Node sending packet of " << p->GetSize() << " bytes to address " << to << std::endl;
  count_sent++;
}

// ******************************************************************
// This is used when running Simple Wireless (GCN, OLSR, AODV)
// ******************************************************************
static void QueueLatencyStats (Ptr<const Packet> p, Time latency)
{
  //std::cout << Simulator::Now ().GetSeconds () << " Packet latency: " << std::setprecision(6) << double_t(latency.GetMicroSeconds())/1000000.0 << " seconds"<< std::endl;
}

// ******************************************************************
// This function supports OLSR and AODV when running on Simple Wireless
// ******************************************************************
static void TransmitStatsSW (Ptr<const Packet> p, Mac48Address from, Mac48Address to , uint16_t protocol)
{
  // Figure out if this is OLSR or data
  if (to.IsBroadcast () || (p->GetSize() != ON_OFF_OTA_PKT_SIZE) )
  {
		pkts_sent_cntl++;
		bytes_sent_cntl += p->GetSize();
		//std::cout << Simulator::Now ().GetSeconds () << " Node sending CONTROL packet of " << p->GetSize() << " bytes to address " << to << std::endl;
  }
  else
  {
	  pkts_sent_data++;
	  bytes_sent_data += p->GetSize();
	  //std::cout << Simulator::Now ().GetSeconds () << " Node sending DATA packet of " << p->GetSize() << " bytes to address " << to << std::endl;
  }
}

// ******************************************************************
// These functions support OLSR and AODV and are related to the Applications
// ******************************************************************

static void SinkReceivedBytes (Ptr<const Packet> p, const Address & from)
{
  count_recv++;
  //std::cout << Simulator::Now ().GetSeconds () << " Node receiving packet of " << p->GetSize() << " bytes. count_recv is "<< count_recv << std::endl;
}

static void AppSendBytes (Ptr<const Packet> p)
{
   count_sent++;
   //std::cout << Simulator::Now ().GetSeconds () << " Node sending packet of " << p->GetSize() << " bytes. count_sent is "<< count_sent << std::endl;
}


// ******************************************************************
// This function supports OLSR
// ******************************************************************
static void checkRouting ()
{
	//std::cout << Simulator::Now ().GetSeconds () <<  " Calling checkRouting" << std::endl;
	int count = 0;
	uint32_t nodeCount = NodeList::GetNNodes ();
	std::vector<ns3::olsr::RoutingTableEntry> tempVector;
	Ptr<Ipv4RoutingProtocol> listProto;
	Ptr<olsr::RoutingProtocol> listOlsr;
	uint32_t numRoutes;
	int16_t priority;
	uint32_t numProtocols;
	
	for (uint32_t i = 0; i < nodeCount; i++)
	{
		// get the node and the node's IPv4 objects
		Ptr<Node> node = NodeList::GetNode (i);
		Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
		// Now get the ipv4 routing. We used a list below so cast the
		// ptr to a list
		Ptr<Ipv4RoutingProtocol> rpTemp = ipv4->GetRoutingProtocol ();
		Ptr<Ipv4ListRouting> list = DynamicCast<Ipv4ListRouting> (rpTemp);
		
		// Now go over all the protocols on the list and look for OLSR
		// As of now, there is only 1 protocol on the list (OLSR)
		numProtocols = list->GetNRoutingProtocols ();
		for (uint32_t i = 0; i < numProtocols; i++) 
		{
			listProto = list->GetRoutingProtocol (i, priority);
			listOlsr = DynamicCast<olsr::RoutingProtocol> (listProto);
			if (listOlsr)
			{
				// call the OLSR function to get the route table entries which
				// returns a vector of the route entries. We don't really need
				// the entries, just the size but OLSR does not have any function
				// that just gets the number of routes
				tempVector = listOlsr->GetRoutingTableEntries ();
				numRoutes = tempVector.size();
				//std::cout << "     Node " << i << " number of routes is "<< numRoutes << std::endl;
				if (numRoutes == (nodeCount - 1) )
				{
					// This node has all the routes so increment count of nodes
					// that have routes to all other nodes
					count++;
				}
				else
				{
					// So sad. This node does not have routes to all the other nodes.
					// Just quit since we are looking to have all nodes have all routes.
					break;
				}
			}
		}
	}
	
	if (count == nodeCount)
	{
		// WooHoo! All the nodes have routes to all the other nodes!
		// Routing has converged.
		//std::cout << Simulator::Now ().GetSeconds () << " Routing has converged" << std::endl;
		rteConvergeTime = Simulator::Now ().GetSeconds ();
	}
	else
	{
		// Routing has not converged so schedule the check again 1 sec from now
		Time rteInterval = ns3::Seconds (1.0);
		Simulator::Schedule (rteInterval, &checkRouting);
	}
}

// ******************************************************************
// This function supports printing position every 1 second
// ******************************************************************
static void PositionTimerExpire ()
{
	NodeContainer::Iterator it;
	NodeContainer const & n = NodeContainer::GetGlobal ();
	
	double currentTime = Simulator::Now ().GetSeconds () ;
	std::cout << currentTime;
	
	for (it = n.Begin (); it != n.End (); ++it) 
	{ 
		// get node ptr, position and node id
		Ptr<Node> node = *it; 
		Ptr<MobilityModel> mob = node->GetObject<MobilityModel> (); 
		Vector pos = mob->GetPosition (); 
		std::cout << ";(" << pos.x << "," << pos.y << ")";
	}
	std::cout << "\n";
	
	Simulator::Schedule (ns3::Seconds (1.0), &PositionTimerExpire);
}


int 
main (int argc, char *argv[])
{
	uint32_t i;
	NodeContainer::Iterator it;
	NodeContainer::Iterator it2;
	std::list<Ipv4Address> destAddresses; // used by OLSR
	std::set<int> groupNodes;  // list of node ids selected as group nodes
	std::set<int> sourceNodes; // list of node ids selected as source nodes
	std::vector<int> shuffle;  // vector used to random shuffle if needed
	int manyToOneSender;
	int sourceNodeId;
	Ipv4Address sourceNodeAddr;
	
	
	// ***********************************************************************
	// Initialize all value that are to be used in the scenario
	// ***********************************************************************
	// These value apply to all scenarios
	uint32_t nNodes = 400;
	double txRange = 100.0;
	int p_g = 10;
	double outerRadius = 500.0;
	std::string radioModel = "SW";
	std::string routingType = "GCN";
	bool collectPcap = false;
	// The following are used to determine the shape of the area in which the group nodes are selected
	// Choice can be either a circle or a rectangle. BOTH are centered about (0,0)
	int selectAreaShape = 0;    // 0 = circle of innerRadius   1 = rectangle of length x height
	double innerRadius = 250.0;
	double lengthRectX = 800;
	double heightRectY = 200;
	bool useMobility=false;
	double minSpeed=10.0;
	double maxSpeed=20.0;
	double minPause=0.0;
	double maxPause=0.0;
	int node1Type = 0; // Node 1 type is either 0 or 1. 
	                   // 0 means it is at origin and is source node
	                   // 1 means it is treated like any other node
	// Many to One is two digits
	// First digit indicates whether or not One to Many is active. That is, does the source send traffic?
	//       When One to Many is off, the source node sends ADVERTISE (but no DATA) 
	// Second digit how Many to One is configured. That is, do the group nodes send back to the source?
	//         0 = no, 1 = yes but just randomly selected group node, 2 = yes all group nodes
	int manyToOne = 10;	// 10 = one to many on + many to one off
								// 11 = one to many on + many to one on with just 1 group node sending
								// 12 = one to many on + many to one on with all group nodes sending
								// 01 = one to many off + many to one on with just 1 group node sending
								// 02 = one to many off + many to one on with all group nodes sending
	int uniResil = 0;  // 0 = low, 1 = med, 2  =high
	int writeStats = 0; // 0 = off. 1 = on. ON causes simulation to write positions every 1 second when mobility is enabled
	
	// These values apply if using the Simple Wireless model
	double dataRate = 100000000.0; // 100Mbps
	std::string errorModel = "CONSTANT";
	double errorRate = 0.0;
	
	// These values apply if using netpatterns
	uint32_t srcttl = 3;
	uint32_t robustMode = 0;
	uint32_t probrelay = 0;
	int32_t advtime = 20;  // -1 = no advertise; 0 = use advertise but override; >0 send ADVERTISE over the air
	uint32_t regenttl = 1; // 1= regen TTL at group nodes 0 = don't regen. Use 0 if simulating classic flood.
	uint32_t numPackets = 15; // the number of packets that the client node should send before stopping
	bool allSenders=false;

	// init counters to 0
	int count_group = 0;
	int count_inRange = 0;
	int count_outRange = 0;

	// ***********************************************************************
	// parse command line
	// ***********************************************************************
	CommandLine cmd;
	cmd.AddValue ("nNodes", "Number of GCN nodes", nNodes);
	cmd.AddValue ("txrange", "Transmission Range of wireless link in meters", txRange);
	cmd.AddValue ("p_g", "Probability of being a group node (0 - 100)", p_g);
	cmd.AddValue ("outerRadius", "Radius of the outer circle into which nodes are randomly placed.", outerRadius);
	cmd.AddValue ("radioModel", "Radio model to use for the simulation. Must be SW or SWDIR", radioModel);
	cmd.AddValue ("routingType", "Routing type to use for the simulation. Must be GCN, OLSR or AODV", routingType); 
	cmd.AddValue ("selectAreaShape", "The shape of the area in which group nodes are placed. 0 = circle 1= rectangle", selectAreaShape);
	cmd.AddValue ("innerRadius", "Radius of the inner circle in which group nodes are selected.", innerRadius);
	cmd.AddValue ("lengthRectX", "Total length along x-axis of the rectangle in which group nodes are selected.", lengthRectX);
	cmd.AddValue ("heightRectY", "Total height along y-axis of the rectangle in which group nodes are selected.", heightRectY);
	cmd.AddValue ("pcap", "Set to 1 to collect pcap traces", collectPcap);
	cmd.AddValue ("useMobility", "Set to 1 to have random waypoint mobility", useMobility);
	cmd.AddValue ("minSpeed", "Minimun speed to use for random mobility", minSpeed);
	cmd.AddValue ("maxSpeed", "Maximum speed to use for random mobility", maxSpeed);
	cmd.AddValue ("minPause", "Minimun pause to use for random mobility", minPause);
	cmd.AddValue ("maxPause", "Maximum pause to use for random mobility", maxPause);
	cmd.AddValue ("node1Type", "The way in which node 0 should be used. 0 = Node 0 at origin and is source node. 1=treat like any other node.", node1Type);
	cmd.AddValue ("datarate", "Data Rate of wireless link (e.g. 1Mb/s)", dataRate);
	cmd.AddValue ("errorModel", "Error model to use. Must be one of: CONSTANT, AI, AO, BI, BO, CI, CO, ZA, ZB, ZC, ZD, ZE, ZF, ZG, ZH, ZI, ZJ, ZK", errorModel);
	cmd.AddValue ("error", "Error rate if CONSTANT error model is used", errorRate);
	cmd.AddValue ("srcttl", "Source TTL", srcttl);
	cmd.AddValue ("robust", "Enable robust mode. Set to 1 to enable.", robustMode);
	cmd.AddValue ("probrelay", "Probability of Relay to use for ACKs", probrelay);
	cmd.AddValue ("advtime", "Interval for ADVERTISE messages", advtime);
	cmd.AddValue ("regenttl", "Don't regenerate TTL at group nodes", regenttl);
	cmd.AddValue ("numPackets", "Number of packets that the client node should send before stopping", numPackets);
	cmd.AddValue ("allSenders", "Set to 1 to have all group nodes also be senders in addition to listeners.", allSenders);
	cmd.AddValue ("manyToOne", "Set to 1 to have a random group node send unicast to the source node. Set to 2 to have all group nodes send unicast", manyToOne);
	cmd.AddValue ("uniResil", "Unicast resilience. 0=Low, 1=Medium, 2=High", uniResil);
	cmd.AddValue ("writeStats", "write the position stats. 1 = yes, 0 = no", writeStats);
	cmd.Parse (argc,argv);
	
	// Set up simtime
	double simtime;
	if ( (routingType.compare("OLSR") ==0) || (routingType.compare("AODV") ==0) )
	{
		simtime = 15.0 + numPackets + 10.0;
	}
	else if (advtime > 0)
	{
		simtime = 2.0 + 15.0 + numPackets + 10.0;
	}
	else
	{
		simtime = 2.0 + numPackets + 10.0;
	}

	// if using Directional network, can't use mobility because neighbor management
	// (adding/removing neighbors) is not implemented.
	if ( (radioModel.compare("SWDIR") ==0) && (useMobility != 0) )
	{
		std::cout << "****************************************************************************" << std::endl;
		std::cout << "WARNING: Mobility has been disabled because SWDIR radio model is being used." << std::endl;
		std::cout << "****************************************************************************" << std::endl;
		// reset mobility to 0
		useMobility = 0;
	}


	if (errorModel.compare("CONSTANT") ==0)
	{
		std::cout << "Running scenario for " << simtime << " seconds with " <<nNodes << " nodes\n     SrcTTL: " << srcttl << "\n     p_g: " << p_g << "\n     Tx Range: " << txRange << "\n     Error Model: CONSTANT\n     Error Rate: " << errorRate 
	          << "\n     Robust Mode: " << (robustMode ? "Yes" : "No") << "\n     ADVERTISE Interval: " << advtime << "\n     Prob of Relay: " << probrelay << "\n     Regenerate TTL: " << (regenttl ? "Yes" : "No") 
	          << "\n     Group Nodes are Senders: " << (allSenders ? "Yes" : "No")
	          << "\n     Many To One: " << manyToOne 
	          << "\n     Unicast Resilience: " << uniResil
	          << "\n     Num Packets: " << numPackets 
	          << "\n     Outer Radius: " << outerRadius << std::endl;
	}
	else
	{
		std::cout << "Running scenario for " << simtime << " seconds with " <<nNodes << " nodes\n     SrcTTL: " << srcttl << "\n     p_g: " << p_g << "\n     Tx Range: " << txRange << "\n     Error Model:  " << errorModel 
	          << "\n     Robust Mode: " << (robustMode ? "Yes" : "No") << "\n     ADVERTISE Interval: " << advtime << "\n     Prob of Relay: " << probrelay << "\n     Regenerate TTL: " << (regenttl ? "Yes" : "No") 
	          << "\n     Group Nodes are Senders: " << (allSenders ? "Yes" : "No")
	          << "\n     Many To One: " << manyToOne 
	          << "\n     Unicast Resilience: " << uniResil
	          << "\n     Num Packets: " << numPackets 
	          << "\n     Outer Radius: " << outerRadius << std::endl;
	}
	
	if (selectAreaShape == 0)
	{
		std::cout << "     Group Node Area Shape: circle with radius " << innerRadius << std::endl;
	}
	else
	{
		std::cout << "     Group Node Area Shape: rectangle with length " << lengthRectX << "  height " << heightRectY << std::endl;
	}
	
	if (useMobility == 0)
	{
		std::cout << "     Mobility: Disabled" << std::endl;
	}
	else
	{
		std::cout << "     Mobility: Enabled with speed [" << minSpeed << "," << maxSpeed << "] pause [" << minPause << "," << maxPause << "]" << std::endl;
	}
	
	// ***********************************************************************
	// Create all the nodes
	// ***********************************************************************
	NodeContainer NpNodes;
	// make # nodes 1 more than the actual number because we
	// create a node 0 which is NOT used. We only use 1-N so that
	// the node ids match GCN
	NpNodes.Create (nNodes + 1);
	NodeContainer const & n = NodeContainer::GetGlobal ();
	
	// Create container to hold devices
	NetDeviceContainer devices;

	// ***********************************************************************
	// Set up the physical/radio layer
	// ***********************************************************************
	// Set transmission range and queue properties
	Config::SetDefault ("ns3::SimpleWirelessChannel::MaxRange", DoubleValue (txRange));
	Config::SetDefault ("ns3::SimpleWirelessNetDevice::DataRate", DataRateValue (DataRate (dataRate)));
	
	// Create error model and set as default for the device
	// ALWAYS set the error rate to 0 here. The error is handled
	// by the channel model in the simple wireless
	Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
	em->SetAttribute ("ErrorRate", DoubleValue (0.0));
	em->SetAttribute ("ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
	Config::SetDefault ("ns3::SimpleWirelessNetDevice::ReceiveErrorModel", PointerValue(em));

	// create channel
	Ptr<SimpleWirelessChannel> phy = CreateObject<SimpleWirelessChannel> ();
	//std::cout << "channel is " << phy << " has " << phy->GetNDevices() << " devices\n";

	// Set up the error model on the channel
	if (errorModel.compare("CONSTANT") ==0)
	{
		phy->setErrorRate(errorRate);
		phy->setErrorModelType(CONSTANT);
	}
	else if (errorModel.compare("AI") ==0) // No interference Indoor
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.0);
		phy->addToPERmodel(10.0, 0.0);
		phy->addToPERmodel(20.0, 0.0);
		phy->addToPERmodel(30.0, 0.007);
		phy->addToPERmodel(40.0, 0.1);
		phy->addToPERmodel(50.0, 0.4);
		phy->addToPERmodel(60.0, 0.7);
		phy->addToPERmodel(70.0, 0.9);
		phy->addToPERmodel(80.0, 0.98);
		phy->addToPERmodel(90.0, 0.999);
		phy->addToPERmodel(100.0, 1.0);
	}
	else if (errorModel.compare("AO") ==0) // No interference Outdoor
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.0);
		phy->addToPERmodel(10.0, 0.0);
		phy->addToPERmodel(20.0, 0.0);
		phy->addToPERmodel(30.0, 0.0);
		phy->addToPERmodel(40.0, 0.0);
		phy->addToPERmodel(50.0, 0.01);
		phy->addToPERmodel(60.0, 0.04);
		phy->addToPERmodel(70.0, 0.1);
		phy->addToPERmodel(80.0, 0.19);
		phy->addToPERmodel(90.0, 0.3);
		phy->addToPERmodel(100.0, 0.4);
		phy->addToPERmodel(110.0, 0.53);
		phy->addToPERmodel(120.0, 0.64);
		phy->addToPERmodel(130.0, 0.72);
		phy->addToPERmodel(140.0, 0.79);
		phy->addToPERmodel(150.0, 0.85);
		phy->addToPERmodel(160.0, 0.89);
		phy->addToPERmodel(170.0, 0.93);
		phy->addToPERmodel(180.0, 0.95);
		phy->addToPERmodel(190.0, 0.96);
		phy->addToPERmodel(200.0, 0.97);
		phy->addToPERmodel(210.0, 1.0);
	}
	else if (errorModel.compare("BI") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.1);
		phy->addToPERmodel(10.0, 0.1);
		phy->addToPERmodel(20.0, 0.1);
		phy->addToPERmodel(30.0, 0.1063);
		phy->addToPERmodel(40.0, 0.19);
		phy->addToPERmodel(50.0, 0.46);
		phy->addToPERmodel(60.0, 0.73);
		phy->addToPERmodel(70.0, 0.91);
		phy->addToPERmodel(80.0, 0.982);
		phy->addToPERmodel(90.0, 0.9991);
		phy->addToPERmodel(100.0, 1.0);
	}
	else if (errorModel.compare("BO") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.1);
		phy->addToPERmodel(10.0, 0.1);
		phy->addToPERmodel(20.0, 0.1);
		phy->addToPERmodel(30.0, 0.1);
		phy->addToPERmodel(40.0, 0.1);
		phy->addToPERmodel(50.0, 0.109);
		phy->addToPERmodel(60.0, 0.136);
		phy->addToPERmodel(70.0, 0.19);
		phy->addToPERmodel(80.0, 0.271);
		phy->addToPERmodel(90.0, 0.37);
		phy->addToPERmodel(100.0, 0.46);
		phy->addToPERmodel(110.0, 0.577);
		phy->addToPERmodel(120.0, 0.676);
		phy->addToPERmodel(130.0, 0.748);
		phy->addToPERmodel(140.0, 0.811);
		phy->addToPERmodel(150.0, 0.865);
		phy->addToPERmodel(160.0, 0.901);
		phy->addToPERmodel(170.0, 0.937);
		phy->addToPERmodel(180.0, 0.955);
		phy->addToPERmodel(190.0, 0.964);
		phy->addToPERmodel(200.0, 0.973);
		phy->addToPERmodel(210.0, 1.0);
	}
	else if (errorModel.compare("CI") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.25);
		phy->addToPERmodel(10.0, 0.25);
		phy->addToPERmodel(20.0, 0.25);
		phy->addToPERmodel(30.0, 0.25525);
		phy->addToPERmodel(40.0, 0.335);
		phy->addToPERmodel(50.0, 0.55);
		phy->addToPERmodel(60.0, 0.775);
		phy->addToPERmodel(70.0, 0.925);
		phy->addToPERmodel(80.0, 0.985);
		phy->addToPERmodel(90.0, 0.99925);
		phy->addToPERmodel(100.0, 1.0);
	}
	else if (errorModel.compare("CO") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.25);
		phy->addToPERmodel(10.0, 0.25);
		phy->addToPERmodel(20.0, 0.25);
		phy->addToPERmodel(30.0, 0.25);
		phy->addToPERmodel(40.0, 0.25);
		phy->addToPERmodel(50.0, 0.2575);
		phy->addToPERmodel(60.0, 0.28);
		phy->addToPERmodel(70.0, 0.325);
		phy->addToPERmodel(80.0, 0.3925);
		phy->addToPERmodel(90.0, 0.475);
		phy->addToPERmodel(100.0, 0.55);
		phy->addToPERmodel(110.0, 0.6475);
		phy->addToPERmodel(120.0, 0.73);
		phy->addToPERmodel(130.0, 0.79);
		phy->addToPERmodel(140.0, 0.8425);
		phy->addToPERmodel(150.0, 0.8875);
		phy->addToPERmodel(160.0, 0.9175);
		phy->addToPERmodel(170.0, 0.9475);
		phy->addToPERmodel(180.0, 0.9625);
		phy->addToPERmodel(190.0, 0.97);
		phy->addToPERmodel(200.0, 0.9775);
	}
	else if (errorModel.compare("DI") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.5);
		phy->addToPERmodel(10.0, 0.5);
		phy->addToPERmodel(20.0, 0.5);
		phy->addToPERmodel(30.0, 0.5035);
		phy->addToPERmodel(40.0, 0.55);
		phy->addToPERmodel(50.0, 0.7);
		phy->addToPERmodel(60.0, 0.85);
		phy->addToPERmodel(70.0, 0.95);
		phy->addToPERmodel(80.0, 0.99);
		phy->addToPERmodel(90.0, 0.9995);
		phy->addToPERmodel(100.0, 1.0);
	}
	else if (errorModel.compare("DO") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.5);
		phy->addToPERmodel(10.0, 0.5);
		phy->addToPERmodel(20.0, 0.5);
		phy->addToPERmodel(30.0, 0.5);
		phy->addToPERmodel(40.0, 0.5);
		phy->addToPERmodel(50.0, 0.505);
		phy->addToPERmodel(60.0, 0.52);
		phy->addToPERmodel(70.0, 0.55);
		phy->addToPERmodel(80.0, 0.595);
		phy->addToPERmodel(90.0, 0.65);
		phy->addToPERmodel(100.0, 0.7);
		phy->addToPERmodel(110.0, 0.765);
		phy->addToPERmodel(120.0, 0.82);
		phy->addToPERmodel(130.0, 0.86);
		phy->addToPERmodel(140.0, 0.895);
		phy->addToPERmodel(150.0, 0.925);
		phy->addToPERmodel(160.0, 0.945);
		phy->addToPERmodel(170.0, 0.965);
		phy->addToPERmodel(180.0, 0.975);
		phy->addToPERmodel(190.0, 0.98);
		phy->addToPERmodel(200.0, 0.985);
		phy->addToPERmodel(210.0, 1.0);
	}
	else if (errorModel.compare("ZA") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.000);
		phy->addToPERmodel(10.0, 0.000);
		phy->addToPERmodel(20.0, 0.000);
		phy->addToPERmodel(30.0, 0.007);
		phy->addToPERmodel(40.0, 0.100);
		phy->addToPERmodel(50.0, 0.400);
		phy->addToPERmodel(60.0, 0.700);
		phy->addToPERmodel(70.0, 0.900);
		phy->addToPERmodel(80.0, 0.980);
		phy->addToPERmodel(90.0, 0.999);
		phy->addToPERmodel(100.0, 1.000);
	}
	else if (errorModel.compare("ZB") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.050);
		phy->addToPERmodel(10.0, 0.050);
		phy->addToPERmodel(20.0, 0.050);
		phy->addToPERmodel(30.0, 0.057);
		phy->addToPERmodel(40.0, 0.145);
		phy->addToPERmodel(50.0, 0.430);
		phy->addToPERmodel(60.0, 0.715);
		phy->addToPERmodel(70.0, 0.905);
		phy->addToPERmodel(80.0, 0.981);
		phy->addToPERmodel(90.0, 0.999);
		phy->addToPERmodel(100.0, 1.000);
	}
	else if (errorModel.compare("ZC") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.100);
		phy->addToPERmodel(10.0, 0.100);
		phy->addToPERmodel(20.0, 0.100);
		phy->addToPERmodel(30.0, 0.106);
		phy->addToPERmodel(40.0, 0.190);
		phy->addToPERmodel(50.0, 0.460);
		phy->addToPERmodel(60.0, 0.730);
		phy->addToPERmodel(70.0, 0.910);
		phy->addToPERmodel(80.0, 0.982);
		phy->addToPERmodel(90.0, 0.999);
		phy->addToPERmodel(100.0, 1.000);
	}
	else if (errorModel.compare("ZD") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.150);
		phy->addToPERmodel(10.0, 0.150);
		phy->addToPERmodel(20.0, 0.150);
		phy->addToPERmodel(30.0, 0.156);
		phy->addToPERmodel(40.0, 0.235);
		phy->addToPERmodel(50.0, 0.490);
		phy->addToPERmodel(60.0, 0.745);
		phy->addToPERmodel(70.0, 0.915);
		phy->addToPERmodel(80.0, 0.983);
		phy->addToPERmodel(90.0, 0.999);
		phy->addToPERmodel(100.0, 1.000);
	}
	else if (errorModel.compare("ZE") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.200);
		phy->addToPERmodel(10.0, 0.200);
		phy->addToPERmodel(20.0, 0.200);
		phy->addToPERmodel(30.0, 0.206);
		phy->addToPERmodel(40.0, 0.280);
		phy->addToPERmodel(50.0, 0.520);
		phy->addToPERmodel(60.0, 0.760);
		phy->addToPERmodel(70.0, 0.920);
		phy->addToPERmodel(80.0, 0.984);
		phy->addToPERmodel(90.0, 0.999);
		phy->addToPERmodel(100.0, 1.000);
	}
	else if (errorModel.compare("ZF") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.250);
		phy->addToPERmodel(10.0, 0.250);
		phy->addToPERmodel(20.0, 0.250);
		phy->addToPERmodel(30.0, 0.255);
		phy->addToPERmodel(40.0, 0.325);
		phy->addToPERmodel(50.0, 0.550);
		phy->addToPERmodel(60.0, 0.775);
		phy->addToPERmodel(70.0, 0.925);
		phy->addToPERmodel(80.0, 0.985);
		phy->addToPERmodel(90.0, 0.999);
		phy->addToPERmodel(100.0, 1.000);
	}
	else if (errorModel.compare("ZG") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.300);
		phy->addToPERmodel(10.0, 0.300);
		phy->addToPERmodel(20.0, 0.300);
		phy->addToPERmodel(30.0, 0.305);
		phy->addToPERmodel(40.0, 0.370);
		phy->addToPERmodel(50.0, 0.580);
		phy->addToPERmodel(60.0, 0.790);
		phy->addToPERmodel(70.0, 0.930);
		phy->addToPERmodel(80.0, 0.986);
		phy->addToPERmodel(90.0, 0.999);
		phy->addToPERmodel(100.0, 1.000);
	}
	else if (errorModel.compare("ZH") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.350);
		phy->addToPERmodel(10.0, 0.350);
		phy->addToPERmodel(20.0, 0.350);
		phy->addToPERmodel(30.0, 0.355);
		phy->addToPERmodel(40.0, 0.415);
		phy->addToPERmodel(50.0, 0.610);
		phy->addToPERmodel(60.0, 0.805);
		phy->addToPERmodel(70.0, 0.935);
		phy->addToPERmodel(80.0, 0.987);
		phy->addToPERmodel(90.0, 0.999);
		phy->addToPERmodel(100.0, 1.000);
	}
	else if (errorModel.compare("ZI") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.400);
		phy->addToPERmodel(10.0, 0.400);
		phy->addToPERmodel(20.0, 0.400);
		phy->addToPERmodel(30.0, 0.404);
		phy->addToPERmodel(40.0, 0.460);
		phy->addToPERmodel(50.0, 0.640);
		phy->addToPERmodel(60.0, 0.820);
		phy->addToPERmodel(70.0, 0.940);
		phy->addToPERmodel(80.0, 0.988);
		phy->addToPERmodel(90.0, 0.999);
		phy->addToPERmodel(100.0, 1.000);
	}
	else if (errorModel.compare("ZJ") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.450);
		phy->addToPERmodel(10.0, 0.450);
		phy->addToPERmodel(20.0, 0.450);
		phy->addToPERmodel(30.0, 0.454);
		phy->addToPERmodel(40.0, 0.505);
		phy->addToPERmodel(50.0, 0.670);
		phy->addToPERmodel(60.0, 0.835);
		phy->addToPERmodel(70.0, 0.945);
		phy->addToPERmodel(80.0, 0.989);
		phy->addToPERmodel(90.0, 0.999);
		phy->addToPERmodel(100.0, 1.000);
	}
	else if (errorModel.compare("ZK") ==0)
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.500);
		phy->addToPERmodel(10.0, 0.500);
		phy->addToPERmodel(20.0, 0.500);
		phy->addToPERmodel(30.0, 0.504);
		phy->addToPERmodel(40.0, 0.550);
		phy->addToPERmodel(50.0, 0.700);
		phy->addToPERmodel(60.0, 0.850);
		phy->addToPERmodel(70.0, 0.950);
		phy->addToPERmodel(80.0, 0.990);
		phy->addToPERmodel(90.0, 1.000);
		phy->addToPERmodel(100.0, 1.000);
	}		
	else
	{
		NS_FATAL_ERROR ("invalid error model specified: " << errorModel << "  Valid values are: CONSTANT, AI, AO, BI, BO, CI, CO, ZA, ZB, ZC, ZD, ZE, ZF, ZG, ZH, ZI, ZJ, ZK");
		return 0;
	}

	// create simple wireless device on each node
	std::string fileStr;
	for (it = n.Begin (); it != n.End (); ++it) 
	{ 
		Ptr<Node> node = *it; 
		
		uint32_t id = node->GetId();
		// Don't start anything on node 0
		if (id == 0)
			continue;
		
		// create device
		Ptr<SimpleWirelessNetDevice> simpleWireless = CreateObject<SimpleWirelessNetDevice> ();
		simpleWireless->SetChannel(phy);
		simpleWireless->SetNode(node);
		simpleWireless->SetAddress(Mac48Address::Allocate ());
		
		// NOTE: We are using SimpleWireless with no queue so
		// don't call anything to set the queue since default
		// is no queue unless configured
		
		node->AddDevice (simpleWireless);
		devices.Add (simpleWireless);
		
		// set up pcap capture
		if (collectPcap)
		{
			std::ostringstream stringStream;
			stringStream << "GCN_node_" << node->GetId() << ".pcap";
			fileStr = stringStream.str();
			simpleWireless->EnablePcapAll(fileStr);
		}
	}
	
	// set up call back for traces
	Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::SimpleWirelessNetDevice/QueueLatency", MakeCallback (&QueueLatencyStats));
	if ( (routingType.compare("OLSR") ==0) || (routingType.compare("AODV") ==0) )
		Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::SimpleWirelessNetDevice/PhyTxBegin", MakeCallback (&TransmitStatsSW));
	else
		Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::SimpleWirelessNetDevice/PhyTxBegin", MakeCallback (&TxBeginSW));
	
	// ***********************************************************************
	// Define position allocator as a circle with user specified outer radius
	// ***********************************************************************
	ObjectFactory pos;
	pos.SetTypeId ("ns3::UniformDiscPositionAllocator");
	pos.Set ("X", DoubleValue (0.0));
	pos.Set ("Y", DoubleValue (0.0));
	pos.Set ("rho", DoubleValue (outerRadius));
	Ptr<PositionAllocator> positionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
	
	// ***********************************************************************
	// Define and install random mobility. Here we are using the circle set up above
	// as the position allocator for mobility (i.e., the area they can move in)
	// ***********************************************************************
	MobilityHelper mobility;
	mobility.SetPositionAllocator (positionAlloc);
	if (useMobility)
	{
		// Create random generator for speed
		Ptr<UniformRandomVariable> mobilitySpeed = CreateObject<UniformRandomVariable> ();
		mobilitySpeed->SetAttribute ("Min", DoubleValue (minSpeed));
		mobilitySpeed->SetAttribute ("Max", DoubleValue (maxSpeed));
		
		// Create random generator for pause time
		Ptr<UniformRandomVariable> mobilityPause = CreateObject<UniformRandomVariable> ();
		mobilityPause->SetAttribute ("Min", DoubleValue (minPause));
		mobilityPause->SetAttribute ("Max", DoubleValue (maxPause));
		
		mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                  "Speed", PointerValue (mobilitySpeed),
                                  "Pause", PointerValue (mobilityPause),
                                  "PositionAllocator", PointerValue (positionAlloc));
	}
	else
	{
		mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	}
	mobility.Install (NpNodes);
	
	// Schedule function that prints position every 1 second if enabled
	if (writeStats)
		Simulator::Schedule (ns3::Seconds (1.0), &PositionTimerExpire);
	
	// ***********************************************************************
	// Select node positions and the group nodes. Apps are installed below
	// so we need to keep a list of the group nodes and source nodes.
	// ***********************************************************************
	// Set up random number generator for picking group nodes
	Ptr<UniformRandomVariable> grpNode = CreateObject<UniformRandomVariable> ();
	grpNode->SetAttribute ("Min", DoubleValue (0));
	grpNode->SetAttribute ("Max", DoubleValue (99));
	
	for (it = n.Begin (); it != n.End (); ++it) 
	{ 
		// get node ptr, position and node id
		Ptr<Node> node = *it; 
		Ptr<MobilityModel> mob = node->GetObject<MobilityModel> (); 
		int id =  node->GetId();
		
		// Put node 0 way out in the middle of no where
		if (id == 0)
		{
			mob->SetPosition (Vector (10.0*outerRadius,0.0,0.0));
			continue;
		}
		
		// Decide if group node
		if ( (id == 1) && (node1Type == 0) )
		{
			// User selected to have node 1 be "0" vs "random". That means that node 1 is
			// placed at the center and is the source node!
			mob->SetPosition (Vector (0.0,0.0,0.0));
			Vector pos = mob->GetPosition (); 
			std::cout << "Node " << id  << " is source node. Position (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n"; 
			
			// insert into list of sources
			sourceNodes.insert(id);
			
			// Set variable that holds source node id
			sourceNodeId = 1;
		}
		else
		{
			// Node is to be randomly placed and randomly selected as group node if inside the
			// group node selection area. If node 0 is "random" it falls here.
			Vector pos = mob->GetPosition (); 
			double_t location = sqrt( (pos.x)*(pos.x) + (pos.y)*(pos.y) );
			// See if this node could be a group node
			if ( ((selectAreaShape == 0) && (location < innerRadius)) ||  // 0 is circle; look for nodes that are inside the innerRadius
				  ((selectAreaShape == 1) && ((abs(pos.x) < lengthRectX/2.0) && (abs(pos.y) < heightRectY/2.0)))   // 1 is rectangle; look for nodes that are inside the length and height on either side of origin
				)
			{
				count_inRange++;
				if (grpNode->GetInteger () < p_g)
				{
					count_group++;
					std::cout << "Node " << id << " IS group node. Position (" << pos.x << ", " << pos.y << ", " << pos.z << ") Location "<< location <<"\n"; 
					
					// put id on list
					groupNodes.insert(id);
				}
				else
				{
					std::cout << "Node " << id << " NOT group node. Position (" << pos.x << ", " << pos.y << ", " << pos.z << ") Location "<< location <<" Lost coin toss. \n"; 
				}
			}
			else
			{
				count_outRange++;
				std::cout << "Node " << id << " NOT group node. Position (" << pos.x << ", " << pos.y << ", " << pos.z << ") Location "<< location <<" Out of range. \n"; 
			}
		}
	}
	
	// Now see if we have a source node and if not pick one
	// If node 1 is "random" then we may not have one yet and we have
	// to pick one from the list of group nodes and make it the source
	if ( (sourceNodes.size() == 0) && (groupNodes.size() > 0))
	{
		// randomly pick a node and add it to the list
		// first copy from the set (which is searchable with find) to
		// a vector which can be used for random shuffle
		std::copy(groupNodes.begin(), groupNodes.end(), std::back_inserter(shuffle));
		std::random_shuffle ( shuffle.begin(), shuffle.end() );
		auto sourceIt = shuffle.begin();
		std::cout << "Node " << *sourceIt << " is source node.\n"; 
		
		// insert into list of sources
		sourceNodes.insert(*sourceIt);
		
		// remove from group nodes list
		count_group--;
		auto dstIt = groupNodes.find(*sourceIt);
		groupNodes.erase(dstIt);
		
		// Set variable that holds source node id
		sourceNodeId = *sourceIt;
	}
	
	// If one group node is sending back to source randomly select it here
	if ( (manyToOne == 1) || (manyToOne == 11) )
	{
		// many to one is set up to randomly pick a group node
		std::copy(groupNodes.begin(), groupNodes.end(), std::back_inserter(shuffle));
		std::random_shuffle ( shuffle.begin(), shuffle.end() );
		auto senderIt = shuffle.begin();
		manyToOneSender = *senderIt;
		std::cout << "Node " << manyToOneSender << " is unicast source node." << std::endl; 
	}
	
	// ***********************************************************************
	// Set up routing which can be OLSR, AODV or GCN
	// ***********************************************************************
	if ( (routingType.compare("OLSR") ==0) || (routingType.compare("AODV") ==0) )
	{
		// install OLSR or AODV
		// don't have to use the Ipv4ListRoutingHelper but it prints
		// the routing table in a better format than directly installing olsr.
		InternetStackHelper stack;
		OlsrHelper olsr;
		AodvHelper aodv;
		Ipv4ListRoutingHelper list;
		
		// Add the routing to the route helper
		if ( routingType.compare("OLSR") ==0)
		{
			list.Add (olsr, 10);
			
			// print the olsr routing table to a file every 10 seconds
			//Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("400node_OLSR.routes", std::ios::out);
			//olsr.PrintRoutingTableAllEvery (ns3::Seconds (10), routingStream);
	
			// schedule function which checks if routing has converged
			Simulator::Schedule (ns3::Seconds (1.0), &checkRouting);
		}
		else
		{
			//Config::SetDefault ("ns3::aodv::RoutingProtocol::MaxQueueLen", UintegerValue (1000));
			//Config::SetDefault ("ns3::aodv::RoutingProtocol::ActiveRouteTimeout", TimeValue (ns3::Seconds (1000)));
			Config::SetDefault ("ns3::aodv::RoutingProtocol::DestinationOnly", BooleanValue (true));
			
			list.Add (aodv, 10);
			
			//Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("400node_AODV.routes", std::ios::out);
			//aodv.PrintRoutingTableAllEvery (ns3::Seconds (10), routingStream);
		}
			
		// now set the routing and install on all nodes
		stack.SetRoutingHelper (list); 
		stack.Install (NpNodes);
		
		// set up IP addresses
		Ipv4AddressHelper address;
		address.SetBase ("10.0.0.0", "255.255.0.0");
		Ipv4InterfaceContainer interfaces = address.Assign (devices);
		
		// Get ip address of source node for later processing
		sourceNodeAddr = interfaces.GetAddress (sourceNodeId);
		std::cout << "Source node is " << sourceNodeId << " and has ip address " << sourceNodeAddr << std::endl;
		
		// Now start apps on all the group nodes and source nodes
		// If allSenders is being used then we will need to start the OnOff app
		// on all nodes to all other nodes and packet sinks on all nodes
		// Just go ahead and put source node on the destination list
		// so we have a list of all destinations to whom we start packet sink.
		// Also put all destinations on source list so we have a list of all sources
		// that need to start OnOff application
		if (allSenders)
		{
			// Put source node on list of listeners
			groupNodes.insert(sourceNodeId);
			
			// Put group nodes on list of senders
			for (auto groupIt = groupNodes.begin (); groupIt != groupNodes.end (); ++groupIt) 
			{
				sourceNodes.insert(*groupIt);
			}
		}
		
		// if many to one is not 10 then the src node will also be receiving
		if (manyToOne != 10)
		{
			// Put source node on list of listeners
			groupNodes.insert(sourceNodeId);
		}
		
		// First start a packet sink on all group nodes. This is the listen side.
		// If "allSenders" or manytoone is not 10 then the source node will be on this list
		for (auto groupIt = groupNodes.begin (); groupIt != groupNodes.end (); ++groupIt) 
		{ 
			int id =  *groupIt;
			
			// This is a group node so start the packet sink
			PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (id), 8080));
			ApplicationContainer apps_sink = sink.Install (NpNodes.Get (id));
			apps_sink.Start (ns3::Seconds (0.0));
			std::cout << "Node " << id << " installed sink to receive on " << interfaces.GetAddress (id) << std::endl;

			// put address on list so we can send to it
			destAddresses.push_back(interfaces.GetAddress (id));
		}
	
		// set up the sink receive callback on all packet sinks
		Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&SinkReceivedBytes));
		
		// Start clients on source node to each group node
		std::list<Ipv4Address>::iterator iter;
	
		Ptr<UniformRandomVariable> appStart = CreateObject<UniformRandomVariable> ();
		appStart->SetAttribute ("Min", DoubleValue (0.0));
		appStart->SetAttribute ("Max", DoubleValue (1.0));
		
		// Now start the OnOff app on all sources to all destinations except the
		// node itself. If we are using allsenders then this list has entries for
		// the 1 source node + all group nodes. Otherwise, it is just a list with 1 entry.
		// For Many to One, if we are using modes 10, 11, 12 we want to start on/off from
		// source to all group nodes.
		if ( allSenders || (manyToOne > 2) )
		{
			for (auto sourceIt = sourceNodes.begin(); sourceIt != sourceNodes.end(); ++sourceIt)
			{
				int id = *sourceIt; 
				Ipv4Address myIp = interfaces.GetAddress (id);
				
				// Now start apps to all other destinations besides ourselves
				for (iter = destAddresses.begin(); iter != destAddresses.end(); ++iter)
				{
					if (myIp == *iter)
						continue;
					
					OnOffHelper onoff = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress ((*iter), 8080)); 
					// Send packets that are ON_OFF_APP_PKT_SIZE bytes
					onoff.SetAttribute ("PacketSize", StringValue (std::to_string(ON_OFF_APP_PKT_SIZE)));
					// The below values are set such that there is 1 packet of ON_OFF_APP_PKT_SIZE bytes is sent every second 
					// until N packets are sent. OnOff stays on for 1 sec then off for 0 which effectively means it is 
					// on all the time. The data rate restricts to sending just one ON_OFF_APP_PKT_SIZE byte packet
					// every second until all N are sent
					onoff.SetAttribute ("DataRate", StringValue (std::to_string(8*ON_OFF_APP_PKT_SIZE)));
					onoff.SetAttribute ("MaxBytes", UintegerValue (ON_OFF_APP_PKT_SIZE * numPackets));
					onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
					onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
					
					// Use the below instead of the 3 lines above if you want to instead just send
					// 1 packet for the whole simulation instead of one per second
					//onoff.SetAttribute ("MaxBytes", StringValue (std::to_string(ON_OFF_APP_PKT_SIZE)));
					
					ApplicationContainer apps = onoff.Install (NpNodes.Get (id));
					std::cout << "Node " << id << " installed app to send to " << (*iter) << std::endl;
					apps.Get (0)->TraceConnectWithoutContext ("Tx", MakeCallback (&AppSendBytes));
					
					double offset = appStart->GetValue ();
					apps.Start (ns3::Seconds (14.0 + offset));
				}
			}
		}
		
		// For mode 1 and mode 11 we also need to start on/off on a randomly selected
		// group node back to the source node
		if ( (manyToOne == 1) || (manyToOne == 11) )
		{
			// start on/off app
			OnOffHelper onoff = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (sourceNodeAddr, 8080)); 
			// Send packets that are ON_OFF_APP_PKT_SIZE bytes
			onoff.SetAttribute ("PacketSize", StringValue (std::to_string(ON_OFF_APP_PKT_SIZE)));
			// The below values are set such that there is 1 packet of ON_OFF_APP_PKT_SIZE bytes is sent every second 
				// until N packets are sent. OnOff stays on for 1 sec then off for 0 which effectively means it is 
				// on all the time. The data rate restricts to sending just one ON_OFF_APP_PKT_SIZE byte packet
				// every second until all N are sent
			onoff.SetAttribute ("DataRate", StringValue (std::to_string(8*ON_OFF_APP_PKT_SIZE)));
			onoff.SetAttribute ("MaxBytes", UintegerValue (ON_OFF_APP_PKT_SIZE * numPackets));
			onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
			onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
			
			// Use the below instead of the 3 lines above if you want to instead just send
			// 1 packet for the whole simulation instead of one per second
			//onoff.SetAttribute ("MaxBytes", StringValue (std::to_string(ON_OFF_APP_PKT_SIZE)));
			
			ApplicationContainer apps = onoff.Install (NpNodes.Get (manyToOneSender));
			std::cout << "Node " << manyToOneSender << " installed app to send to " << sourceNodeAddr << std::endl;
			apps.Get (0)->TraceConnectWithoutContext ("Tx", MakeCallback (&AppSendBytes));
			
			double offset = appStart->GetValue ();
			apps.Start (ns3::Seconds (14.0 + offset));
		}
		
		// For mode 2 and mode 12 we also need to start on/off on all group nodes
		// back to the source node
		if ( (manyToOne == 2) || (manyToOne == 12) )
		{
			for (auto groupIt = groupNodes.begin (); groupIt != groupNodes.end (); ++groupIt) 
			{
				if ( *groupIt == sourceNodeId)
					continue;
				
				OnOffHelper onoff = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (sourceNodeAddr, 8080)); 
				// Send packets that are ON_OFF_APP_PKT_SIZE bytes
				onoff.SetAttribute ("PacketSize", StringValue (std::to_string(ON_OFF_APP_PKT_SIZE)));
				// The below values are set such that there is 1 packet of ON_OFF_APP_PKT_SIZE bytes is sent every second 
				// until N packets are sent. OnOff stays on for 1 sec then off for 0 which effectively means it is 
				// on all the time. The data rate restricts to sending just one ON_OFF_APP_PKT_SIZE byte packet
				// every second until all N are sent
				onoff.SetAttribute ("DataRate", StringValue (std::to_string(8*ON_OFF_APP_PKT_SIZE)));
				onoff.SetAttribute ("MaxBytes", UintegerValue (ON_OFF_APP_PKT_SIZE * numPackets));
				onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
				onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
				
				// Use the below instead of the 3 lines above if you want to instead just send
				// 1 packet for the whole simulation instead of one per second
				//onoff.SetAttribute ("MaxBytes", StringValue (std::to_string(ON_OFF_APP_PKT_SIZE)));
				
				ApplicationContainer apps = onoff.Install (NpNodes.Get (*groupIt));
				std::cout << "Node " << (*groupIt) << " installed app to send to " << sourceNodeAddr << std::endl;
				apps.Get (0)->TraceConnectWithoutContext ("Tx", MakeCallback (&AppSendBytes));
				
				double offset = appStart->GetValue ();
				apps.Start (ns3::Seconds (14.0 + offset));
			}
		} 
	}
	else
	{
		// Create DCE manager and install on all nodes
		DceManagerHelper dceManager;
		dceManager.Install(NpNodes);
		
		// Create the internet stack helper.
		// This doesn't actual create any IP or ARP traffic but is required.
		InternetStackHelper stack;
		stack.Install (NpNodes);
	
		// Create and install all the DCE apps
		DceApplicationHelper dce;
		ApplicationContainer apps;
	
		dce.SetStackSize (1<<20);

		for (it = n.Begin (); it != n.End (); ++it) 
		{ 
			Ptr<Node> node = *it; 
			int id =  node->GetId();
			
			// start GCN on all node
			dce.SetBinary ("gcn");
			dce.ResetArguments();
			dce.AddArgument("-i");
			dce.AddArgument(std::to_string(id));
			dce.AddArgument("-d");
			dce.AddArgument("1");
			dce.AddArgument("-l");
			//dce.AddArgument("6"); // DEBUG
			dce.AddArgument("3"); //ERROR
			if (robustMode)
			{
				dce.AddArgument("-b");  // Turn ON robust mode so we always re-broadcast w/ack DATA
												//  packets even if we have no downstream subscriber
			}
			apps = dce.Install (NpNodes.Get (id));
			apps.Start (ns3::Seconds (1.0));
			
			if (sourceNodes.find(id) != sourceNodes.end())
			{
				// Start client at source node
				dce.SetBinary ("gcnClientBasic");
				dce.ResetArguments();
				dce.AddArgument("-g");
				dce.AddArgument("123");
				dce.AddArgument("-i");
				dce.AddArgument(std::to_string(id));
				dce.AddArgument("-t");
				dce.AddArgument("1");  //send only
				dce.AddArgument("-k");
				dce.AddArgument(std::to_string(probrelay));
				dce.AddArgument("-r");
				if (manyToOne < 10)   // is many to one 1 or 2?
					dce.AddArgument("0");    // don't send any messages. Just ADVERTISE
				else
					dce.AddArgument("1");    // generate a message every 1 second
				dce.AddArgument("-b");
				dce.AddArgument("1386"); // force messages to be 1400 bytes or 100x of an ADVERTISE
				dce.AddArgument("-s");
				dce.AddArgument(std::to_string(srcttl));
				dce.AddArgument("-a");
				dce.AddArgument(std::to_string(advtime));  
				dce.AddArgument("-z");
				dce.AddArgument(std::to_string(numPackets)); //Stop after sending numPackets DATA packets
				dce.AddArgument("-l");
				//dce.AddArgument("6"); // DEBUG
				dce.AddArgument("3"); //ERROR
				if(!regenttl)
				{
					// turn on the no regen ttl feature
					dce.AddArgument("-d");
				}
				apps = dce.Install (NpNodes.Get (id));
				apps.Start (ns3::Seconds (2.0));
				
				if (allSenders || (manyToOne != 10) )
				{
					// Source is also a listener so start client to listen
					// NOTE: we could just start one client of type 2 BUT that does
					// NOT work in NS3 due to some issues with the socket
					dce.SetBinary ("gcnClientBasic");
					dce.ResetArguments();
					dce.AddArgument("-g");
					dce.AddArgument("123");
					dce.AddArgument("-i");
					dce.AddArgument(std::to_string(id));
					dce.AddArgument("-t");
					dce.AddArgument("0");  
					dce.AddArgument("-l");
					//dce.AddArgument("6"); // DEBUG
					dce.AddArgument("3"); //ERROR
					apps = dce.Install (NpNodes.Get (id));
					apps.Start (ns3::Seconds (2.0));
				}
			}
			else if (groupNodes.find(id) != groupNodes.end())
			{
				if (manyToOne > 2)  // don't listen for many to one of 1 and 2
				{
					// start client to listen
					dce.SetBinary ("gcnClientBasic");
					dce.ResetArguments();
					dce.AddArgument("-g");
					dce.AddArgument("123");
					dce.AddArgument("-i");
					dce.AddArgument(std::to_string(id));
					dce.AddArgument("-t");
					dce.AddArgument("0");
					dce.AddArgument("-l");
					//dce.AddArgument("6"); // DEBUG
					dce.AddArgument("3"); //ERROR
					apps = dce.Install (NpNodes.Get (id));
					apps.Start (ns3::Seconds (2.0));
				}
				
				if (allSenders)
				{
					// All group nodes are also senders so start client to send
					// NOTE: we could just start one client of type 2 BUT that does
					// NOT work in NS3 due to some issues with the socket
					dce.SetBinary ("gcnClientBasic");
					dce.ResetArguments();
					dce.AddArgument("-g");
					dce.AddArgument("123");
					dce.AddArgument("-i");
					dce.AddArgument(std::to_string(id));
					dce.AddArgument("-t");
					dce.AddArgument("1");  
					dce.AddArgument("-k");
					dce.AddArgument(std::to_string(probrelay));
					dce.AddArgument("-r");
					dce.AddArgument("1");    // generate a message every 1 second
					dce.AddArgument("-b");
					dce.AddArgument("1386"); // force messages to be 1400 bytes or 100x of an ADVERTISE
					dce.AddArgument("-s");
					dce.AddArgument(std::to_string(srcttl));
					// if adv time is > 0, then we are using advertisements. 
					// for all senders, we use advertise override so that only
					// the source node actually sends advertisements and the other
					// group nodes depend on that node to establish the path
					dce.AddArgument("-a");
					if (advtime > 0)
						dce.AddArgument("0"); 
					else
						dce.AddArgument(std::to_string(advtime));  // this covers case of -1 adv rate
					dce.AddArgument("-z");
					dce.AddArgument(std::to_string(numPackets)); //Stop after sending numPackets DATA packets
					dce.AddArgument("-l");
					//dce.AddArgument("6"); // DEBUG
					dce.AddArgument("3"); //ERROR
					if(!regenttl)
					{
						// turn on the no regen ttl feature
						dce.AddArgument("-d");
					}
					apps = dce.Install (NpNodes.Get (id));
					apps.Start (ns3::Seconds (2.0));
				}
				
				if ( (manyToOne == 2) || (manyToOne == 12) || 
				     ((manyToOne == 1) && (id == manyToOneSender)) || 
				     ((manyToOne == 11) && (id == manyToOneSender)) )
				{
					auto srcIt = sourceNodes.begin();
					
					// start client to send unicast data
					dce.SetBinary ("gcnClientManyToOne");
					dce.ResetArguments();
					dce.AddArgument("-g");
					dce.AddArgument("123");
					dce.AddArgument("-i");
					dce.AddArgument(std::to_string(id));
					dce.AddArgument("-d");
					dce.AddArgument(std::to_string(*srcIt));
					dce.AddArgument("-k");
					dce.AddArgument(std::to_string(probrelay));
					dce.AddArgument("-l");
					//dce.AddArgument("6"); // DEBUG
					dce.AddArgument("3"); //ERROR
					dce.AddArgument("-r");
					dce.AddArgument("1");    // generate a message every 1 second
					dce.AddArgument("-b");
					dce.AddArgument("1386"); // force messages to be 1400 bytes or 100x of an ADVERTISE
					dce.AddArgument("-z");
					dce.AddArgument(std::to_string(numPackets)); //Stop after sending numPackets DATA packets
					dce.AddArgument("-x");
					dce.AddArgument(std::to_string(uniResil));  // unicast resilience
					// if adv time is > 0, then we are using advertisements. 
					// For all manyToOne !=1- we use advertise override so that only
					// the source node actually sends advertisements and the other
					// group nodes depend on that node to establish the path
					dce.AddArgument("-a");
					if (advtime > 0)
						dce.AddArgument("0"); 
					else
						dce.AddArgument(std::to_string(advtime));  // this covers case of -1 adv rate

					apps = dce.Install (NpNodes.Get (id));
					apps.Start (ns3::Seconds (2.0));
				}
			}
		}
	}

	// ***********************************************************************
	// Now we are going to figure out how many neighbors we have
	// ***********************************************************************
	// Loop over all nodes and for each node, loop over all other
	// nodes and check the distance between the nodes to see if
	// in range
	// Go ahead and build up a map of neighbors which is needed for directional networks
	// We make a map:   map<nodeid, set(neighbor nodes)>
	// the key is the node id and the mapped value is a list (set) of node ids that are
	// the neighbors of the node that is the key value.
	std::map<int, std::set<int>> mNetworkNbrs;
	
	for (it = n.Begin (); it != n.End (); ++it) 
	{ 
		Ptr<Node> node = *it; 
		Ptr<MobilityModel> mob = node->GetObject<MobilityModel> (); 
		int id =  node->GetId();
		int neighborCount = 0;
		
		std::cout << " Node " << id << " neighbors: ";
		
		// Define a set of nbrs for this node
		std::set<int> NbrNodes;
		
		// loop over all nodes and look for neighbors
		for (it2 = n.Begin (); it2 != n.End (); ++it2) 
		{
			Ptr<Node> node2 = *it2; 
			int id2 =  node2->GetId();
			// Is this node us? If so then skip it
			if (id == id2)
				continue;
			
			Ptr<MobilityModel> mob2 = node2->GetObject<MobilityModel> (); 

			// Get distance
			double distance = mob->GetDistanceFrom (mob2);
			if (distance <= txRange)
			{
				std::cout << (id2) << "  ";
				neighborCount++;
				// add to the set of neighbors for this node
				NbrNodes.insert(id2);
			}
		}
		std::cout << "    Total neighbors: " << neighborCount << std::endl;
		// Insert into larger map
		mNetworkNbrs.insert(std::pair<int, std::set<int>>(id, NbrNodes));
	}
	std::cout << "Number of group nodes: " << count_group <<"\nNumber In Range: " << count_inRange <<"\nNumber Out of Range: " << count_outRange << std::endl;
	
	// Now print positions at time = 0
	if (writeStats)
	{
		std::cout << "\n0";
		for (it = n.Begin (); it != n.End (); ++it) 
		{ 
			// get node ptr, position and node id
			Ptr<Node> node = *it; 
			Ptr<MobilityModel> mob = node->GetObject<MobilityModel> (); 
			Vector pos = mob->GetPosition (); 
			std::cout << ";(" << pos.x << "," << pos.y << ")";
		}
		std::cout << "\n";
	}
	

	if ( radioModel.compare("SWDIR") ==0 )
	{
		// ***********************************************************************
		// Now we can handle directional networks
		// ***********************************************************************
		// Set up directional networking. This includes:
		//     1. adding directional neighbors to the device
		//     2. creating & setting up the GCN directional shim
		// Do this after adding all the devices because
		// we need to get MAC addresses for the neighbors we want to add
		for ( NetDeviceContainer::Iterator dIt = devices.Begin(); dIt != devices.End(); ++dIt)
		{
			// get the node and node id for this device
			Ptr<Node> node_src = (*dIt)->GetNode();
			uint32_t id_src = node_src->GetId();
			
			std::cout << "Setting up directional network for node " << id_src << std::endl;
		
			// now get the device itself.
			Ptr<NetDevice> dev_src = (*dIt);
			Ptr<SimpleWirelessNetDevice> swDev_src = DynamicCast<SimpleWirelessNetDevice>(dev_src);
			swDev_src->SetAttributeFailSafe ("FixedNeighborListEnabled", BooleanValue (true));
			std::map<uint32_t, Mac48Address> nbrSet;
			
			// Get the entry in the neighbor map
			auto iter = mNetworkNbrs.find(id_src);
			if (iter == mNetworkNbrs.end() )
			{
				std::cout << "**** ERROR: Could not find list of neighbors for node " << id_src << std::endl;
				exit(1);
			}
			
			// Now add each neighbor as a directional neighbor
			for ( NetDeviceContainer::Iterator dIt2 = devices.Begin(); dIt2 != devices.End(); ++dIt2)
			{
				Ptr<Node> node_dst = (*dIt2)->GetNode();
				uint32_t id_dst = node_dst->GetId();
				
				// Only add valid pairs based on connectivity to directional neighbor list
				if ( iter->second.find(id_dst) != iter->second.end() )
				{
					// Get mac addr of node and add to map
					Address addr = (*dIt2)->GetAddress();
					Mac48Address macAddr = Mac48Address::ConvertFrom (addr);
					nbrSet.insert(std::pair<uint32_t, Mac48Address>(id_dst, macAddr));
					std::cout << "Node " << id_src << " Adding node " << id_dst << " with mac address " << macAddr << " to directional network" << std::endl;
				}
			}
			// Now add to device
			if (!swDev_src->AddDirectionalNeighbors(nbrSet))
			{
				NS_FATAL_ERROR ("Call to AddDirectionalNeighbors failed. Please enabled directional neighbors.");
				return 0;
			}
		}
	}
	// ------------------------------------------------------------------------
	
	
	
	// ***********************************************************************
	// and finally ... off we go!
	// ***********************************************************************
	
	Simulator::Stop (ns3::Seconds(simtime));
	Simulator::Run ();
	Simulator::Destroy ();
	
	// ***********************************************************************
	// For OLSR we need to get some stats
	// ***********************************************************************
	if ( (routingType.compare("OLSR") ==0) || (routingType.compare("AODV") ==0) )
	{
		double rcvPercent = 0.0;
		if (count_sent)
			rcvPercent = (double)count_recv/(double)count_sent;
			
		std::cout << "Sent: " << count_sent << "\nReceive Count: " << count_recv
		         << "\n% Received: " << std::fixed << std::setprecision(2) << rcvPercent << std::noshowpoint << std::setprecision(0)
					<< "\nControl Packets: " << pkts_sent_cntl << "\nControl Bytes: "   << bytes_sent_cntl 
					<< "\nData Packets: "    << pkts_sent_data << "\nData Bytes: "      <<  bytes_sent_data
					<< "\nRoutes Converged at: " << rteConvergeTime << std::endl;
	}
	else
	{
		std::cout << "\nSent: " << count_sent << std::endl;
	}
	
	NS_LOG_INFO ("Run Completed Successfully");
	
	return 0;
}
