The Group Centric Networking (GCN) software is an instantiation of a group-centric network that enables scalable, efficient, and resilient group communications
and was designed to enable a group of devices or users to communicate in a local region. The details of the protocol is described in [1] and has been previously
approved for public release by DARPA.

The primary design goals of GCN is to be able to (1) efficiently and dynamically discover nodes that have interest in the data (i.e. group nodes) and (2) disseminate
information between group nodes in a resilient (against packet errors, interference, and mobility) and bandwidth efficient manner. 

GCN achieves this through three major high level mechanisms:

1.	Group Discovery – Efficient discovery of the local region where group members reside via a a group discovery algorithm that is able to connect group members without the use of global control information.

2.	Tunable resiliency – Relay nodes are activated such that the local region is sufficiently “covered” in data by having a tunable number of redundant data relays. This allows for resiliency
   towards both packet loss and mobility without the need for the constant exchange of control information. The number of activated relay nodes self-adjusts in response to real-time channel conditions. 
   
3.	Targeted flooding – Data can be efficiently and resiliently sent between sets of group members.

The GCN software package includes:
•	The main GCN code in the /gcn/src folder 
•	GCN release notes and sample GCN scenarios with NS3 wifi and LL simple-wireless tests in the ns3/ folder


[1] G. Kuperman, J. Sun, B.-N. Cheng, P. Deutsch, and A. Narula-Tam, “Group Centric Networking: A New Approach for Wireless Multi-Hop Networking to Enable the Internet of Things,” in arXiv, 2015. 


GCN_RELEASE has the following contents:
==============================================
GCN_RELEASE/:
  /gcn
  /ns3
  README

GCN_RELEASE/gcn:
  build
  CMakeLists.txt
  src

GCN_RELEASE/gcn/src:
  CMakeLists.txt
  Common.cpp
  Common.h
  gcnClientBasic.cpp
  gcnClientBasic.h
  gcnClient.cpp
  gcnClient.h
  gcnClientManyToOne.cpp
  gcnClientManyToOne.h
  gcn.cpp
  GCNMessage.proto
  gcnService.cpp
  gcnService.h

GCN_RELEASE/ns3:
  GCN_Code_Release_Notes.docx
  GCN_network_tests_LLSW.cc
  GCN_network_tests_Wifi.cc
  run_sims.sh
  wscript


Installation Instructions
============================================================
Please refer to the file GCN_Code_Release_Notes.docx in the /ns3 folder for more information about GCN, 
installation, the ns3 scenarios and running simulations.

Miscellaneous Notes
===========================================================
* The GCN code has been run with NS3.22/DCE1.5, NS3.21/DCE1.4 and NS3.20/DCE1.3

