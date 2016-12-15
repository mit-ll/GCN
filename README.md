DISTRIBUTION STATEMENT A. Approved for public release: distribution unlimited.

This material is based upon work supported by the Defense Advanced Research Projects Agency under Air Force Contract No. FA8721-05-C-0002 and/or FA8702-15-D-0001. 
Any opinions, findings, conclusions or recommendations expressed in this material are those of the author(s) and do not necessarily reflect the views of the Defense Advanced Research Projects Agency.

© 2016 Massachusetts Institute of Technology.

The software/firmware is provided to you on an As-Is basis

Delivered to the US Government with Unlimited Rights, as defined in DFARS Part 252.227-7013 or 7014 (Feb 2014). 
Notwithstanding any copyright notice, U.S. Government rights in this work are defined by DFARS 252.227-7013 or DFARS 252.227-7014 as detailed above. 
Use of this work other than as specifically authorized by the U.S. Government may violate any copyrights that exist in this work.


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

