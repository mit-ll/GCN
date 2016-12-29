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

#include "gcnService.h" 

using boost::asio::buffer;
static uint16_t GcnPort  = 12345;

//************************************************************************
GcnService::GcnService(const GcnServiceConfig &gcnConfig, io_service * io_serv)
:	pIoService(io_serv),
	mClientAcceptor(*pIoService, tcp::endpoint(tcp::v4(), GcnPort)),
	mClientSocket(*pIoService),
	mOTASession(gcnConfig.mcastEthernetHeader),
	mDevices{gcnConfig.devices},
	mDataFilePath(gcnConfig.dataFile),
	mNodeId(gcnConfig.nodeId),
	mCurrentLogLevel(gcnConfig.logLevel),
	mHashExpireTime(gcnConfig.hashExpire),
	mHashCleanupInterval(gcnConfig.hashInterval),
	mReversePathExpireTime(gcnConfig.pathExpire),
	mReversePathCleanupInterval(gcnConfig.pathInterval),
	mRemotePullExpireTime(gcnConfig.pullExpire),
	mRemotePullCleanupInterval(gcnConfig.pullInterval),
	mAlwaysRebroadcast(gcnConfig.alwaysRebroadcast),
	mStatInterval(1.0),
	mHashCleanupTimer(*pIoService, Seconds(1)),
	mRemotePullCleanupTimer(*pIoService, Seconds(1)),
	mReversePathCleanupTimer(*pIoService, Seconds(1)),
	mStatTimer(*pIoService, Seconds(1)),
	clientCount(0), 
	recvCountAdv(0),
	recvCountAck(0), 
	recvCountData(0),
	recvCountDataUni(0),
	dropCount(0), 
	pushCount(0), 
	fwdCount(0),
	fwdCountUni(0),
	clientRcvCount(),
	sentCount(0),
	relayDataGroup(0),
	relayDataNonGroup(0),
	nonGroupRcvAck(0),
	nonGroupRcvAdv(0),
	totalBytesSentCtl(0),
	totalPacketsSentCtl(0),
	totalBytesSentData(0),
	totalPacketsSentData(0),
	mSentDataDI(0),
	mSentAdvDI(0),
	mSentAckDI(0),
	mRcvDataDI(0),
	mRcvAdvDI(0),
	mRcvAckDI(0),
	mLocalPullDI(0),
	mLocalUnpullDI(0),
	mSizeOfSize(sizeof(uint32_t))
{
	// Verify that the version of the library that we linked against is
	// compatible with the version of the headers we compiled against.
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	
	mPbPrinter.SetInitialIndentLevel(1);
	
	vector<string>::iterator iter;
	char devlist[100]  = "";
	
	for (iter = mDevices.begin(); iter != mDevices.end(); ++iter)
	{
		sprintf(&devlist[strlen(devlist)], " %s ", (*iter).c_str());
	}

	// If the user want so use data collection, then open the file
	if(mDataFilePath != "")
		mDataFile = fopen(mDataFilePath.c_str(),"w+");
	else 
		mDataFile = NULL;
	
	// Initialize OTA session handlers
	mOTAReceiveHandler = std::bind(&GcnService::OnNetworkReceive, 
				       this,
				       std::placeholders::_1, 
				       std::placeholders::_2);

	// Initialize the client session handlers
	mClientReceiveHandler = std::bind(&GcnService::OnClientReceive, 
					  this,
					  std::placeholders::_1, 
					  std::placeholders::_2, 
					  std::placeholders::_3);
	
	mClientCloseHandler = std::bind(&GcnService::closeClientConnection,
					this,
					std::placeholders::_1);
					
	// start the stat timer just once
	mStatTimer.async_wait(boost::bind(&GcnService::OnStatTimeout, this));

	LOG(LOG_FORCE, "Creating GCN with:\n  NodeId: %d\n  Log Level: %s\n  Devices: %s\n  Hash Expire Time: %lf\n  Hash Cleanup Interval: %lf\n  Pull Expire Time: %lf\n  Pull Cleanup Interval: %lf\n  Path Expire Time: %lf\n  Path Cleanup Interval: %lf\n  Always Re-Broadcast: %s\n  Port: %d",
	            mNodeId, LogLevelStr[mCurrentLogLevel], devlist, mHashExpireTime, mHashCleanupInterval, mRemotePullExpireTime, mRemotePullCleanupInterval,
	            mReversePathExpireTime, mReversePathCleanupInterval, (mAlwaysRebroadcast ? "True" : "False"), GcnPort);
	
}

//************************************************************************
GcnService::~GcnService()
{
	printf("GcnService destructor complete.\n");
}


//************************************************************************
void GcnService::Start()
{
	// Start the threads and the events in a start method and NOT in the 
	// constructor to be sure that the derived class is instantiated when
	// we do the binding of functions.
	
	// seed random number generator
	srand(time(NULL));
	
	// open our OTA session and start reading
	mOTASession.open(*pIoService, mDevices);
	mOTASession.read(mOTAReceiveHandler);
	
	// begin listening to app client connections
	acceptClientConnections();

	// Create periodic event to clean the hash of old entries
	mHashCleanupTimer.async_wait(boost::bind(&GcnService::hashCleanup, this));
	
	// Create periodic event to clean the Remote Pull table of old entries
	mRemotePullCleanupTimer.async_wait(boost::bind(&GcnService::remotePullCleanup, this));
	
	// Create periodic event to clean the Reverse Path Table of old entries
	mReversePathCleanupTimer.async_wait(boost::bind(&GcnService::reversePathCleanup, this));

}


//************************************************************************
void GcnService::Stop()
{
	LOG(LOG_FORCE,"\nSTOPPING GCN. Final stats\n GCN Client stats: rcvd>%d  sentOTA>%d    \nGCN OTA stats: rcvdAdv>%d rcvdAck>%d rcvdData>%d rcvdUni>%d drop>%d push>%d fwd>%d fwdUni>%d relayDataGroup>%d relayDataNonGroup>%d nonGroupRcvAck>%d nonGroupRcvAdv>%d totalBytesSentCtl>%d totalPacketsSentCtl>%d totalBytesSentData>%d totalPacketsSentData>%d", 
				clientRcvCount, sentCount, recvCountAdv, recvCountAck, recvCountData, recvCountDataUni, dropCount, pushCount, fwdCount, fwdCountUni, relayDataGroup, relayDataNonGroup, nonGroupRcvAck, nonGroupRcvAdv, totalBytesSentCtl, totalPacketsSentCtl, totalBytesSentData, totalPacketsSentData);

	
	// close the socket to the client
	if (clientCount)
	{
		// close all client sockets from the map
		// NOTE that it is possible that we attempt to close the same
		// client socket more than once if the client on that socket
		// is a group node for multiple groups
		for (LocalPullIt iter = mLocalPullTable.begin(); iter != mLocalPullTable.end(); )
		{
			iter->second->close();
			printf(" ... Closed client session for GID %d\n", iter->first);
			iter = mLocalPullTable.erase(iter);
		}
	}
	else
	{
		printf(" ... No active clients\n");
	}
	
	// close the socket to the network
	mOTASession.close();
	printf(" ... Raw Socket closed\n");
	
	// Stop any timers we have for sending ADVERTISE messages
	for (AnnounceIt iter2 = mAnnounceTable.begin(); iter2 != mAnnounceTable.end(); ++iter2)
	{
		if (iter2->second.pTimer)
		{
			iter2->second.pTimer->cancel();
			printf(" ... Advertise event canceled for GID %d\n", iter2->first);
		}
		
		// Now close the session to the client
		// NOTE that it is possible that we attempt to close the same
		// client socket more than once if the client on that socket
		// is a source node for multiple groups or if it was closed
		// above as a client
		iter2->second.pSession->close();
		printf(" ... Closed client session for GID %d\n", iter2->first);
	}
	
	// Stop cleanup events
	mHashCleanupTimer.cancel();
	printf(" ... Hash Cleanup event canceled\n");
	
	mReversePathCleanupTimer.cancel();
	printf(" ... Reverse Path Table Cleanup event canceled\n");
	
	mRemotePullCleanupTimer.cancel();
	printf(" ... Remote Pull Table Cleanup event canceled\n");
	
	pIoService->stop();

	// Delete all global objects allocated by libprotobuf.
	google::protobuf::ShutdownProtobufLibrary();
	printf(" ... google buffer shutdown\n");
	
	if(mDataFile != NULL) fclose(mDataFile);
}

//************************************************************************
void GcnService::Run()
{
	pIoService->run();
}

//************************************************************************
// function that listens for client connections
void GcnService::acceptClientConnections()
{	
	mClientAcceptor.async_accept(mClientSocket,
				     [this](error_code ec)
				     {
					     if (!ec)
						     {
							     // increment count of connected clients
							     clientCount++;
							     // create a new client connection and start reading from it
							     std::make_shared<ClientSession>(std::move(mClientSocket))->read(mClientReceiveHandler, mClientCloseHandler);
						     }
					     // listen for more connections
					     acceptClientConnections();
				     });
}

//************************************************************************
// function to forward a Data message to the subscribing app
void GcnService::forwardToApp(Data & dataMsg, shared_ptr<ClientSession> pSession)
{
	// create app message and fill in with the message passed in
	AppMessage message;
	
	auto pData = message.add_data();
	pData->CopyFrom(dataMsg);
	forwardToApp(message, pSession);
}

//************************************************************************
// function to forward a Pull message to the subscribing app
void GcnService::forwardToApp(Pull & pullMsg, shared_ptr<ClientSession> pSession)
{
	// create app message and fill in with the message passed in
	AppMessage message;
	
	auto pPull = message.add_pull();
	pPull->CopyFrom(pullMsg);
	forwardToApp(message, pSession);
}

//************************************************************************
// function to forward a Unpull message to the subscribing app
void GcnService::forwardToApp(Unpull & unpullMsg, shared_ptr<ClientSession> pSession)
{
	// create app message and fill in with the message passed in
	AppMessage message;
	
	auto pUnpull = message.add_unpull();
	pUnpull->CopyFrom(unpullMsg);
	forwardToApp(message, pSession);
}

//************************************************************************
// function to forward a Advertise message to the subscribing app
void GcnService::forwardToApp(Advertise & advMsg, shared_ptr<ClientSession> pSession)
{
	// create app message and fill in with the message passed in
	AppMessage message;
	
	auto pAdvertise = message.add_advertise();
	pAdvertise->CopyFrom(advMsg);
	forwardToApp(message, pSession);
}

//************************************************************************
// function to forward an AppMessage to the subscribing app
void GcnService::forwardToApp(AppMessage & Msg, shared_ptr<ClientSession> pSession)
{
	// Serialize message for transmission
	uint32_t size = Msg.ByteSize();
	uint32_t totalSize = size + mSizeOfSize;

	// Check the total size can fit in the buffer
	if ( totalSize > MAX_BUFFER_SIZE )
		{
			LOG(LOG_ERROR, "AppMessage too large");
			return;
		}
	
	// Buffer to hold the message to send
	BufferPtr pBuffer(new Buffer);

	// Copy the size of the AppMessage first
	uint32_t htnSize = htonl(size);
	memcpy(pBuffer->data(), &htnSize, mSizeOfSize);

	// Then serialize message
	Msg.SerializeToArray(pBuffer->data() + mSizeOfSize, Msg.ByteSize());
	
	// Check log level first because printing to string is expensive
	// and can affect throughput
	if (mCurrentLogLevel >= LOG_DEBUG)
	{
		string sMessage;
		mPbPrinter.PrintToString(Msg, &sMessage);
		LOG(LOG_DEBUG, "Sent to App (%d bytes):\n%s", Msg.ByteSize(), sMessage.c_str());
	}
	// Send message
	pSession->write(pBuffer, totalSize);
}


//************************************************************************
// function to forward a Data OTA
void GcnService::forwardToOTA(Data & dataMsg, uint32_t ttl)
{
	// Create OTA message to send out raw socket
	OTAMessage message;
	
	// get the "header" part of OTA message and set fields
	auto header = message.mutable_header();
	header->set_src(mNodeId);
	
	// Add the message passed in to the OTA message
	// decrement the ttl field based on what user passed in.
	// We alwyas DECREMENT the TTL when we send
	auto pData = message.add_data();
	pData->CopyFrom(dataMsg);
	pData->set_ttl(ttl - 1); 
	
	if(mDataFile != NULL) //DATAITEM
	{
		mSentDataDI++;
		char buf[256];
		uint64_t millis = duration_cast<milliseconds>(getTime()).count();
		int buflen = sprintf(buf,"0,%.0f,ll.gcnSentData,node%03d.gcnService,%.0f,\"{\"\"gid\"\":%d,\"\"size\"\":%d,\"\"seq\"\":%.0f,\"\"srcnode\"\":\"\"node%03d\"\",\"\"ttl\"\":%d,\"\"dist\"\":%d}\"\n",
			(double)mSentDataDI,mNodeId,(double)millis,dataMsg.gid(),(int)dataMsg.data().size(),(double)dataMsg.sequence(),dataMsg.srcnode(),dataMsg.srcttl(),dataMsg.distance());
		fwrite(buf,sizeof(char),buflen,mDataFile);
		fflush(mDataFile);
	}
	forwardToOTA(pData->gid(), message);
}

//************************************************************************
// function to forward a Advertise OTA
void GcnService::forwardToOTA(Advertise & advMsg, uint32_t ttl)
{
	// Create OTA message to send out raw socket
	OTAMessage message;
	
	// get the "header" part of OTA message and set fields
	auto header = message.mutable_header();
	header->set_src(mNodeId);
	
	// Add the message passed in to the OTA message
	// decrement the ttl field based on what user passed in.
	// We alwyas DECREMENT the TTL when we send
	auto pAdvertise = message.add_advertise();
	pAdvertise->CopyFrom(advMsg);
	pAdvertise->set_ttl(ttl - 1);
	
	if(mDataFile != NULL) //DATAITEM
	{
		mSentAdvDI++;
		char buf[256];
		uint64_t millis = duration_cast<milliseconds>(getTime()).count();
		int buflen = sprintf(buf,"0,%.0f,ll.gcnSentAdv,node%03d.gcnService,%.0f,\"{\"\"gid\"\":%d,\"\"srcttl\"\":%d,\"\"seq\"\":%.0f,\"\"srcnode\"\":\"\"node%03d\"\",\"\"ttl\"\":%d,\"\"dist\"\":%d}\"\n",
			(double)mSentAdvDI,mNodeId,(double)millis,advMsg.gid(),advMsg.srcttl(),(double)advMsg.sequence(),advMsg.srcnode(),advMsg.ttl(),advMsg.distance());
		fwrite(buf,sizeof(char),buflen,mDataFile);
		fflush(mDataFile);
	}
	forwardToOTA(pAdvertise->gid(), message);
}

//************************************************************************
// function to forward a Ack OTA
void GcnService::forwardToOTA(Ack & ackMsg)
{
	// Create OTA message to send out raw socket
	OTAMessage message;
	
	// get the "header" part of OTA message and set fields
	auto header = message.mutable_header();
	header->set_src(mNodeId);
	
	// Add the message passed in to the OTA message
	auto pAck = message.add_ack();
	pAck->CopyFrom(ackMsg);
	
	if(mDataFile != NULL) //DATAITEM
	{
		mSentAckDI++;
		char buf[256];
		uint64_t millis = duration_cast<milliseconds>(getTime()).count();
		int buflen = sprintf(buf,"0,%.0f,ll.gcnSentAck,node%03d.gcnService,%.0f,\"{\"\"gid\"\":%d,\"\"seq\"\":%.0f,\"\"srcnode\"\":\"\"node%03d\"\",\"\"obligrelay\"\":%d,\"\"relayprob\"\":%d}\"\n",
			(double)mSentAckDI,mNodeId,(double)millis,ackMsg.gid(),(double)ackMsg.sequence(),ackMsg.srcnode(),ackMsg.obligatoryrelay(),ackMsg.probabilityofrelay());
		fwrite(buf,sizeof(char),buflen,mDataFile);
		fflush(mDataFile);
	}
	forwardToOTA(pAck->gid(), message);
}


//************************************************************************
// function to forward a Push OTA
void GcnService::forwardToOTA(GroupId gid, OTAMessage & Msg)
{
	// set flag for data or control
	bool ctrlPkt = true;
	if (Msg.data_size())
	{
		ctrlPkt = false;
	}
	
	// Serialize message for transmission
	BufferPtr pBuffer(new Buffer);

	// Get length of message (will include potential Ethernet header)
	size_t length = Msg.ByteSize();
	
	// If using Ethernet headers, leave room for them in front
	if (USE_ETHERNET_HEADERS)
	{
		length += sizeof(struct ether_header);
		if ( length > MAX_BUFFER_SIZE )
		{
			LOG(LOG_ERROR, "Message too large for Ethernet headers");
			return;
		}

		// Ethernet header will be added to the front of the message later
		Msg.SerializeToArray(pBuffer->data() + sizeof(struct ether_header), Msg.ByteSize());
	}
	else
	{
		Msg.SerializeToArray(pBuffer->data(), Msg.ByteSize());
	}
	
	// Check log level first because printing to string is expensive
	// and can affect throughput
	if (mCurrentLogLevel >= LOG_DEBUG)
	{
		string sMessage;
		mPbPrinter.PrintToString(Msg, &sMessage);
		LOG(LOG_DEBUG, "Sent OTA (%d bytes):\n%s", length, sMessage.c_str()); 
	}
	
	if (Msg.data_size())
	{
		totalBytesSentData += length;
		totalPacketsSentData++;
	}
	else
	{
		totalBytesSentCtl += length;
		totalPacketsSentCtl++;
	}

	
	// send over RAW socket
	mOTASession.write(gid, pBuffer, length, ctrlPkt);

}


//************************************************************************
void GcnService::closeClientConnection(shared_ptr<ClientSession> pSession)
{
	// connection to this client is gone
	LOG(LOG_DEBUG, "Closing client connection ...");
	// Need to remve client from Local Pull Map and Announce Map
	// Local Pull Map is for all the groups the client subscribed to
	// Announce Map is all the groups the client was the source for
	
	// remove this client session from the Local Pull map
	// We have to go over the entire map and can't break once an entry
	// is found because there could be more than one group from this
	// session (i.e., it could be subscribed to multiple groups)
	for (LocalPullIt iter = mLocalPullTable.begin(); iter != mLocalPullTable.end(); )
	{
		if (iter->second == pSession)
		{
			iter = mLocalPullTable.erase(iter);
		}
		else
		{
			++iter;
		}
	}

	// remove this client session from the Announce map
	// We have to go over the entire map and can't break once an entry
	// is found because there could be more than one group from this
	// socket (i.e., it could be providing content for multiple groups)
	for (AnnounceIt iter2 = mAnnounceTable.begin(); iter2 != mAnnounceTable.end(); )
	{
		if (iter2->second.pSession == pSession)
		{
			// cancel the event used to send announcements if timer was set
			if (iter2->second.pTimer)
			{
				iter2->second.pTimer->cancel();
			}
			// erase entry
			iter2 = mAnnounceTable.erase(iter2);
		}
		else
		{
			++iter2;
		}
	}
	
	// Now close the session
	pSession->close();
	
	// decrement client count
	clientCount--;
}

//************************************************************************
// function serializes a Data message to a string for the hash
bool GcnService::addToHash(Data & dataMsg, HashValue & hashValue)
{
	uint32_t ttl;
	string data;
	
	// We want all the fields of the message in the hash EXCEPT the ttl 
	// and distance because values can change so just set to 0 so all 
	// packets have same value.
	// BUT we need the actual ttl value if it is new and is added to the hash 
	// so first save off the value to use below.
	
	// copy the Data passed in to a new message so we can set the ttl
	// of that new message to 0 then serialize it to a string for hashing
	Data message;
	message.CopyFrom(dataMsg);
	// save off the ttl
	ttl = message.ttl();
	
	// Set ttl and distance to 0 to exclude from hash
	message.set_ttl(0); 
	message.set_distance(0);
	
	// if this has a unicast header, exclude relay distance, obligatory relay and prob of relay
	if(message.has_uheader())
	{
		auto uheader = message.mutable_uheader();
		uheader->set_relaydistance(0);
	}
	
	// Now serialize to a string for the hash and add it
	message.SerializeToString(&data);
	return(addToHash(data, hashValue, ttl));
}

//************************************************************************
// function serializes a Advertise message to a string for the hash
bool GcnService::addToHash(Advertise & advMsg, HashValue & hashValue)
{
	uint32_t ttl;
	string data;
	
	// We want all the fields of the message in the hash EXCEPT the ttl 
	// and distance because values can change so just set to 0 so all 
	// packets have same value.
	// BUT we need the actual ttl value if it is new and is added to the hash 
	// so first save off the value to use below.
	
	// copy the Advertise passed in to a new message so we can set the ttl
	// of that new message to 0 then serialize it to a string for hashing
	Advertise message;
	message.CopyFrom(advMsg);
	// save off the ttl
	ttl = message.ttl();
	
	// Set ttl and distance to 0 to exclude from hash
	message.set_ttl(0); 
	message.set_distance(0);
	
	// Now serialize to a string for the hash and add it to hash
	message.SerializeToString(&data);
	return(addToHash(data, hashValue, ttl));
}

//************************************************************************
// function sets the hash value using the reference passed in
// If it was already in the hash, returns false
// If NEW then it returns true
bool GcnService::addToHash(string data, HashValue & hashValue, uint32_t ttl)
{
	// Get hash value
	hashValue = make_hash(data);
	
	// Now look to see if this is already in the hash
	HashIt iter = mHashTable.find(hashValue);
	if (iter != mHashTable.end())
	{
		// if already in the hash then return the hash value
		LOG(LOG_DEBUG, "Received packet already seen with hash value %d", hashValue);
		return(false);
	}
	else
	{
		// We do NOT have an entry for this hashValue.
		// Add an entry to the HashMap
		mHashTable.insert(HashPair(hashValue, ttl));
		
		// Add an entry to the HashTimeMap
		TimeDuration currDur = getTime();
		double currTime = (double) duration_cast<seconds>(currDur).count();
		mHashTimeTable.insert(HashTimePair(currTime, hashValue));
		
		LOG(LOG_DEBUG, "Received packet NOT seen with hash value %d. Add to map with TTL %d.", hashValue, ttl);
		
		// Since it was not in the hash already, return true.
		return(true);
	}
}


//************************************************************************
// function to get the MaxTTL mapped value from the hash map
uint32_t GcnService::getMaxTTLfromHash(HashValue hashValue)
{
	HashIt iter = mHashTable.find(hashValue);
	if (iter != mHashTable.end())
	{
		return(iter->second);
	}
	else
	{
		LOG(LOG_FATAL, "Could not find hash value in hash map for a packet we have seen already\n");
	}
	return(0);
}

//************************************************************************
// function to change the MaxTTL mapped value in the hash map
void GcnService::changeMaxTTL(HashValue hashValue, uint32_t ttl)
{
	HashIt iter = mHashTable.find(hashValue);
	if (iter != mHashTable.end())
	{
		iter->second = ttl;
	}
	else
	{
		LOG(LOG_FATAL, "Could not find hash value in hash map for a packet we have seen already\n");
	}
}


//************************************************************************
// function to update the distance table based on the packet we
// just received
void GcnService::updateDistanceTable(GroupId gid, NodeId gidsrc, HashValue hashValue, uint32_t distance, NodeId otaSrc, bool newToHash, bool AdvMsg)
{
	
	DistanceIt iter2 = mDistanceTable.find(GIDKey(gid, gidsrc));
	if (iter2 != mDistanceTable.end())
	{
		// already have a distance entry. 
		// if we have already seen it from this OTA source then just update the packet count
		// NOTE that we do not update the distance since this is a
		// duplicate packet that we have already seen
		// Also NOTE that we do not count the same packet more than once from the same OTA source
		if (iter2->second.latestPacketHash == hashValue)
		{
			if ( (iter2->second.packetSrcs.find(otaSrc) == iter2->second.packetSrcs.end())  && (gidsrc != mNodeId))
			{
				iter2->second.packetCount++;
				iter2->second.packetSrcs.insert(otaSrc);
				LOG(LOG_DEBUG,"Received packet already seen for GID %d GID Src %d with hash %d. Packet OTA src is %d. Packet count is now %d", gid, gidsrc, hashValue, otaSrc, iter2->second.packetCount);
			}
			else
			{
				LOG(LOG_DEBUG,"Received packet already seen for GID %d GID Src %d with hash %d from OTA source %d. NOT incrementing packet count %d", 
							gid, gidsrc, hashValue, otaSrc, iter2->second.packetCount);
			}
		}
		else if (newToHash)
		{
			// This is a new packet we have not seen for this GID source.
			// Reset the distance entry since this is now the latest packet
			if (AdvMsg)
			{
				iter2->second.distance = distance;
				iter2->second.latestPacketHash = hashValue;
				iter2->second.packetCount = 1;
				iter2->second.packetSrcs.clear();
				iter2->second.packetSrcs.insert(otaSrc);
				LOG(LOG_DEBUG,"Received NEW packet (advertise) for GID %d GID Src %d with hash %d. Packet OTA src is %d. Packet count is now %d", gid, gidsrc, hashValue, otaSrc, iter2->second.packetCount);
			}
			else
			{
				iter2->second.distance = distance;
			}
			
		}
	}
	else
	{
		// we do not have an entry so add one
		DistanceInfo info;
		info.distance = distance;
		info.latestPacketHash = hashValue;
		info.packetCount = 1;
		info.packetSrcs.insert(otaSrc);
		mDistanceTable.insert(DistancePair(GIDKey(gid, gidsrc), info));
	}
	
	if (mCurrentLogLevel >= LOG_DEBUG)
	{
		printf("\n**************************************************\n");
		printf("Distance Table\n");
		printf("\n**************************************************\n");
		printf("GID    GID Src   Distance  Count   Sources\n");
		printf("-----  --------  --------  ------  -----------------\n");
		for (DistanceIt prtIt = mDistanceTable.begin(); prtIt != mDistanceTable.end(); ++prtIt)
		{
			printf("%5d  %8d  %8d  %6d  ", prtIt->first.gid, prtIt->first.gidSrc, prtIt->second.distance, prtIt->second.packetCount);
			for (auto prtIt2 = prtIt->second.packetSrcs.begin(); prtIt2 != prtIt->second.packetSrcs.end(); ++prtIt2)
	      {
	         if (prtIt2 !=  prtIt->second.packetSrcs.begin() )
	         {
	            printf(", "); 
	         }
	        printf("%2d", *prtIt2);           
	      }
	      printf("\n");
		}
	}
	
	
}

//************************************************************************
// function to "flip a coin"
bool GcnService::coinFlip(uint32_t prob)
{
	// This picks a number between 0 and 99
	if ( (uint32_t)(rand() % 100) < prob)
		return(true);
	else
		return(false);
}



//************************************************************************
// Function called by periodic event to clean hash 
void GcnService::hashCleanup()
{
	HashTimeIt timerIter;
	HashIt     hashIter;
	int count = 0;

	TimeDuration currDur = getTime();
	double currTime = (double) duration_cast<seconds>(currDur).count();
	
	// iterate over the hash time map and find entries that have expired.
	// for each expired entry we need to find that entry in the hash map
	// and delete it
	for (timerIter = mHashTimeTable.begin(); timerIter != mHashTimeTable.end();)
	{
		if ( (currTime - timerIter->first) > mHashExpireTime )
		{
			// this is an expired entry
			// Find and delete it from the hash map
			hashIter = mHashTable.find(timerIter->second);
			LOG_ASSERT(hashIter != mHashTable.end(), "Could not find entry in hash table for expired hash time table entry");
			mHashTable.erase(hashIter);
			
			// Now remove entry from hash time table
			timerIter = mHashTimeTable.erase(timerIter);
			
			count++;
		}
		else
		{
			// If we hit an entry that is NOT expired then we can stop
			// since the entries are ordered by time
			break;
		}
	}
	
	LOG(LOG_FORCE, "Cleaned Hash Table. Removed %d expired entries. Hash table has %d entries.", count, mHashTable.size());
	
	// reschedule the periodic event
	if (mHashCleanupInterval > 0)
	{
		mHashCleanupTimer.expires_at(mHashCleanupTimer.expires_at() + Milliseconds(mHashCleanupInterval));
		mHashCleanupTimer.async_wait(boost::bind(&GcnService::hashCleanup, this));
	}
}


//************************************************************************
// Function called by periodic event to clean reverse path table
void GcnService::reversePathCleanup()
{
	ReservePathIt pathIter;
	int count = 0;

	TimeDuration currDur = getTime();
	double currTime = (double) duration_cast<seconds>(currDur).count();
	
	
	// iterate over the Reverse Path  map and find entries that have expired.
	for (pathIter = mReversePathTable.begin(); pathIter != mReversePathTable.end();)
	{
		LOG(LOG_DEBUG, "Reverse Path Table Entry: gid %d   gidsrc %d   timestamp %d  nexthop %d  seq # %d  probRelay %d", 
				pathIter->first.gid, pathIter->first.gidSrc, pathIter->second.timestamp, pathIter->second.srcNode, pathIter->second.seqNum, pathIter->second.probRelay);
		if ( (currTime - pathIter->second.timestamp) > mReversePathExpireTime )
		{
			// this is an expired entry. remove entry from table
			pathIter = mReversePathTable.erase(pathIter);
			
			count++;
		}
		else
		{
			++pathIter;
		}
	}
	
	LOG(LOG_DEBUG, "Cleaned Reverse Path Table. Removed %d expired entries.", count);
	
	// reschedule the periodic event
	if (mReversePathCleanupInterval > 0)
	{
		mReversePathCleanupTimer.expires_at(mReversePathCleanupTimer.expires_at() + Milliseconds(mReversePathCleanupInterval));
		mReversePathCleanupTimer.async_wait(boost::bind(&GcnService::reversePathCleanup, this));
	}
}


//************************************************************************
// Function called by periodic event to clean remote pull table
void GcnService::remotePullCleanup()
{
	RemotePullIt pullIter;
	int count = 0;

	TimeDuration currDur = getTime();
	double currTime = (double) duration_cast<seconds>(currDur).count();
	
	
	// iterate over the Remote Pull  map and find entries that have expired.
	for (pullIter = mRemotePullTable.begin(); pullIter != mRemotePullTable.end();)
	{
		LOG(LOG_DEBUG, "Remote Pull Table Entry: gid %d   node id %d   timestamp %d", 
				pullIter->first, pullIter->second.nodeId, pullIter->second.timestamp);
		if ( (currTime - pullIter->second.timestamp) > mRemotePullExpireTime )
		{
			// this is an expired entry. remove entry from table
			pullIter = mRemotePullTable.erase(pullIter);
			
			count++;
		}
		else
		{
			++pullIter;
		}
	}
	
	LOG(LOG_DEBUG, "Cleaned Remote Pull Table. Removed %d expired entries.\n     # entries in Remote Pull Table: %d \n     # entries in Local Pull Table: %d \n     # Announce Table: %d", 
						count, mRemotePullTable.size(), mLocalPullTable.size(), mAnnounceTable.size());
	
	// If we have any apps that are using ADVERTISE/ACK we need to check if
	// we still have subscribers either locally or remotely. If not, tell the
	// App to stop sending by sending it a UNPULL
	// We only want to send a UNPULL if we have previously sent a PULL
	// Otherwise, the app is not sending any data anyway.
	for (AnnounceIt iter = mAnnounceTable.begin(); iter != mAnnounceTable.end(); ++iter)
	{
		GroupId gid = iter->first;
		
		// ***** TO DO *****
		// NOTE: the use of condition (iter->second.interval > 0) means that if we are over riding advertise messages
		// then we will NEVER send a unpull!!! This is long term a bad thing so we need to figure out
		// how we want to handle these group nodes that want to send using a tree but don't want to advertise
		// (that is, they depend on another node in the group to send advertisements and when they receive
		//  a ADVERTISE over the air then they start sending). ... Should we add something to remote pull table???
		if ( iter->second.pullSentToApp && (mRemotePullTable.count(gid) == 0) && (mLocalPullTable.count(gid) == 0) && (iter->second.interval > 0) )
		{
			// we no longer have any subscribers so stop sending data
			Unpull unpullMsg;
			unpullMsg.set_gid(gid);
			forwardToApp(unpullMsg, iter->second.pSession);
			// reset flag that we sent pull
			iter->second.pullSentToApp = false;
		}
	}

	// reschedule the periodic event
	if (mRemotePullCleanupInterval > 0)
		{
			mRemotePullCleanupTimer.expires_at(mRemotePullCleanupTimer.expires_at() + Milliseconds(mRemotePullCleanupInterval));
			mRemotePullCleanupTimer.async_wait(boost::bind(&GcnService::remotePullCleanup, this));
		}
}

//************************************************************************
void GcnService::OnStatTimeout()
{
	// Print all the groups for which this node is a relay
	char buffer [500];
	sprintf(buffer, "GCN Relay Node for Groups:");
	for ( auto pullIter = mRemotePullTable.begin(); pullIter != mRemotePullTable.end(); pullIter = mRemotePullTable.upper_bound(pullIter->first))
	{
		sprintf(buffer + strlen(buffer), " %d", pullIter->first);
	}
	
	// print the stats
	LOG(LOG_FORCE,"GCN Client stats: rcvd>%d  sentOTA>%d   GCN OTA stats: rcvdAdv>%d rcvdAck>%d rcvdData>%d rcvdUni>%d drop>%d push>%d fwd>%d fwdUni>%d relayDataGroup>%d relayDataNonGroup>%d nonGroupRcvAck>%d nonGroupRcvAdv>%d totalBytesSentCtl>%d totalPacketsSentCtl>%d totalBytesSentData>%d totalPacketsSentData>%d %s", 
				clientRcvCount, sentCount, recvCountAdv, recvCountAck, recvCountData, recvCountDataUni, dropCount, pushCount, fwdCount, fwdCountUni, relayDataGroup, relayDataNonGroup, nonGroupRcvAck, nonGroupRcvAdv, totalBytesSentCtl, totalPacketsSentCtl, totalBytesSentData, totalPacketsSentData, buffer);

	// Reset flags for relay nodes
	relayDataGroup = 0;
	relayDataNonGroup = 0;
	
	// reschedule the periodic event
	if (mStatInterval > 0)
		{
			mStatTimer.expires_at(mStatTimer.expires_at() + Seconds(mStatInterval));
			mStatTimer.async_wait(boost::bind(&GcnService::OnStatTimeout, this));
		}
}


//************************************************************************
void GcnService::OnAnnounceTimeout(const error_code & ec, shared_ptr<void> arg)
{
	if(ec)
	{
		return;
	}
	
	shared_ptr<AnnounceIt> pIter = static_pointer_cast<AnnounceIt>(arg);

	// Create message, fill in announce and send out socket
	Advertise message;
	HashValue tempHash;

	message.set_gid((*pIter)->first);
	message.set_srcttl((*pIter)->second.srcTtl);
	message.set_srcnode(mNodeId);
	message.set_ttl((*pIter)->second.srcTtl);
	message.set_probrelay((*pIter)->second.probRelay);
	message.set_distance(0);
	message.set_sequence(++((*pIter)->second.seqNum));
	if ((*pIter)->second.noTtlRegen)
	{
		message.set_nottlregen(true);
	}
	
	// Add to hash
	addToHash(message, tempHash);

	// Add to distance table
	updateDistanceTable((*pIter)->first, mNodeId, tempHash, 0, mNodeId, true, true);
	
	LOG(LOG_DEBUG, "Sending ADVERTISE for GID %d\n", (*pIter)->first);
	forwardToOTA(message, (*pIter)->second.srcTtl);

	// reschedule the periodic event
	if ((*pIter)->second.interval > 0)
	{
		(*pIter)->second.pTimer->expires_at((*pIter)->second.pTimer->expires_at() + Seconds((*pIter)->second.interval));
		(*pIter)->second.pTimer->async_wait(boost::bind(&GcnService::OnAnnounceTimeout, this, _1, pIter));
	}
}

//************************************************************************
// Function to handle processing when ACK timer expires
void GcnService::OnAckTimeout(const error_code & ec, Ack & ackMsg)
{
	if(ec)
	{
		return;
	}
	
	// Get iter to item in the timer table
	auto it = mAckTimerTable.find(GIDKey(ackMsg.gid(), ackMsg.srcnode()));
	LOG_ASSERT(it != mAckTimerTable.end(), "Hit ACK timeout but had no entry in timer table");
	
	// Get the prob of relay for this <GID, GID source>
	auto revIt = mReversePathTable.find(GIDKey(ackMsg.gid(), ackMsg.srcnode()));
	LOG_ASSERT(revIt != mReversePathTable.end(), "Sending ACK but had no reverse path for GID %d GID src\n", ackMsg.gid(), ackMsg.srcnode());
	uint32_t probRelay = revIt->second.probRelay;

	// If prob of relay is greater than 100 then we use that number as the numerator
	// NOTE that prob of relay in the ACK message is an int between 0 and 100
	if (probRelay > 100 )
	{
		// Set the probability of relay based on how many nodes we have received
		// an ADVERTISE from
		// These values of > 100 might look weird but since the value is 0 to 100, we
		// use the number as is w/o having to multiply by 100. E.g., if we want 2/N
		// we would have to do: 2/N * 100. Instead we just use 200/N. 
		DistanceIt iter = mDistanceTable.find(GIDKey(ackMsg.gid(), ackMsg.srcnode()));
		LOG_ASSERT(iter != mDistanceTable.end(), "Hit ACK timeout but had no entry in distance table");
		// how many nodes have we heard the adv from?
		uint32_t numNodes = iter->second.packetSrcs.size();
		// Calculate the prob of relay.
		int prob = probRelay/numNodes;
		ackMsg.set_probabilityofrelay(prob);
		LOG(LOG_DEBUG, "Sending ACK for GID %d  GID src %d. Number of neighbors is %d and prob of relay is %d\n", it->first.gid, it->first.gidSrc, numNodes, prob);
	}
	else
	{
		ackMsg.set_probabilityofrelay(probRelay);
		LOG(LOG_DEBUG, "Sending ACK for GID %d  GID src %d. prob of relay is %d\n", it->first.gid, it->first.gidSrc, probRelay);
	}

	// Send message
	forwardToOTA(ackMsg);

	// delete the timer from the timer table
	mAckTimerTable.erase(it);
}

//************************************************************************
// function to set a timer for sending the Ack
void GcnService::setAckTimer(Ack & ackMsg)
{
	// See if we already have a timer and if so, do nothing
	auto it = mAckTimerTable.find(GIDKey(ackMsg.gid(), ackMsg.srcnode()));
	
	if (it == mAckTimerTable.end())
	{
		// Set timer for timer to occur
		// Time is 0.1 seconds + random value between 0 and 0.1 seconds
		// (so overall delay is between 0.1 and 0.2 seconds)
		// Use milliseconds (100 to 200 msec)
		double tempTime = 100.0 + (double(rand() % 100));
		AckTimer ackT(new deadline_timer(*pIoService, Milliseconds(tempTime)));
		
		ackT->async_wait(boost::bind(&GcnService::OnAckTimeout, this, _1, ackMsg));
		
		mAckTimerTable.insert(AckTimerPair(GIDKey(ackMsg.gid(), ackMsg.srcnode()),ackT));
		LOG(LOG_DEBUG, "Set Ack timer for GID %d  GID src %d. Timer hits in %.0lf msec", ackMsg.gid(), ackMsg.srcnode(), tempTime);
	}
	else
	{
		LOG(LOG_DEBUG, "received notice to send ACK but already have ACK scheduled for GID %d, GID source %d", ackMsg.gid(), ackMsg.srcnode());
	}
}


//************************************************************************
// Function to handle processing when ADVERTISE timer expires
void GcnService::OnAdvTimeout(const error_code & ec, Advertise & advertiseMsg, uint32_t ttl)
{
	if(ec)
	{
		return;
	}
	
	// Get iter to item in the timer table
	auto it = mAdvTimerTable.find(GIDKey(advertiseMsg.gid(), advertiseMsg.srcnode()));
	LOG_ASSERT(it != mAdvTimerTable.end(), "Hit Advertise timeout but had no entry in timer table");
	
	LOG(LOG_DEBUG, "Advertise timer exired for GID %d  GID src %d  Seq %d  ttl %d.", 
		advertiseMsg.gid(), advertiseMsg.srcnode(), advertiseMsg.sequence(), advertiseMsg.ttl());
	
	// Send message
	forwardToOTA(advertiseMsg, ttl);
	
	// delete the timer from the timer table
	mAdvTimerTable.erase(it);
}

//************************************************************************
// function to set a timer for sending the Advertise
void GcnService::setAdvTimer(Advertise & advertiseMsg, uint32_t ttl)
{
	// Set timer for timer to occur
	// Time is random value between 0 and 1000 microseconds)
	double tempTime = (double)(rand() % 1000);
	AdvTimer tempTimer(new deadline_timer(*pIoService, Microseconds(tempTime)));

	tempTimer->async_wait(boost::bind(&GcnService::OnAdvTimeout, this, _1, advertiseMsg, ttl));

	// Before we insert this into the table, see if we have one already
	// If we do, delete the existing one and use the new one.
	// How can this happen you ask? Well, it is possile that we get an advertise
	// as a non-group node with a ttl of say 1. Then we get another one with a 
	// ttl of 2. In that case, because the ttl is higher than our max ttl for the
	// advertise then we will set a timer for the second one and that can happen
	// quickly before the timer expired. We only want to send the advertise with the highest ttl.
	// If we don't delete the first one then when we do the insert, the new one is not
	// inserted because we have one already. The new timer then gets automatically cancelled
	// when we exit this function and boom! we send the lower ttl advertisement instead
	// of the higher ttl advertisement. So ... see if we have one and delete it first!
	auto it = mAdvTimerTable.find(GIDKey(advertiseMsg.gid(), advertiseMsg.srcnode()));
	if (it != mAdvTimerTable.end())
	{
		// delete the timer from the timer table
		mAdvTimerTable.erase(it);
	}
	mAdvTimerTable.insert(AdvTimerPair(GIDKey(advertiseMsg.gid(), advertiseMsg.srcnode()), tempTimer));
	LOG(LOG_DEBUG, "Set Advertise timer for GID %d  GID src %d  Seq %d  ttl %d. Timer hits in %.0lf usec", 
		advertiseMsg.gid(), advertiseMsg.srcnode(), advertiseMsg.sequence(), advertiseMsg.ttl(), tempTime);

}


//************************************************************************
// Function to handle processing when DATA timer expires
void GcnService::OnDataTimeout(const error_code & ec, Data & dataMsg, uint32_t ttl, HashValue hashVal)
{
	if(ec)
	{
		return;
	}
	
	// Get iter to item in the timer table
	auto it = mDataTimerTable.find(hashVal);
	LOG_ASSERT(it != mDataTimerTable.end(), "Hit Data timeout but had no entry in timer table");
	
	LOG(LOG_DEBUG, "Data timer exired for GID %d  GID src %d  hash value %u.", dataMsg.gid(), dataMsg.srcnode(), hashVal);
	
	// Send message
	forwardToOTA(dataMsg, ttl);
	
	// delete the timer from the timer table
	mDataTimerTable.erase(it);
}

//************************************************************************
// function to set a timer for sending a Data msg
void GcnService::setDataTimer(Data & dataMsg, uint32_t ttl, HashValue hashVal)
{
	// Set timer for timer to occur
	// Time is random between 0.0 and 10.0 microseconds)
	double tempTime = double(rand() % 10);
	DataTimer tempTimer(new deadline_timer(*pIoService, Microseconds(tempTime)));

	tempTimer->async_wait(boost::bind(&GcnService::OnDataTimeout, this, _1, dataMsg, ttl, hashVal));

	// Before we insert this into the table, see if we have one already
	// If we do, delete the existing one and use the new one.
	// How can this happen you ask? Well, it is possile that we get Data
	// as a non-group node with a ttl of say 1. Then we get another one with a 
	// ttl of 2. In that case, because the ttl is higher than our max ttl for the
	// advertise then we will set a timer for the second one and that can happen
	// quickly before the timer expired. We only want to send the data with the highest ttl.
	// If we don't delete the first one then when we do the insert, the new one is not
	// inserted because we have one already. The new timer then gets automatically cancelled
	// when we exit this function and boom! we send the lower ttl data instead
	// of the higher ttl data. So ... see if we have one and delete it first!
	auto it = mDataTimerTable.find(hashVal);
	if (it != mDataTimerTable.end())
	{
		// delete the timer from the timer table
		mDataTimerTable.erase(it);
	}
	mDataTimerTable.insert(DataTimerPair(hashVal, tempTimer));
	LOG(LOG_DEBUG, "Set Data timer for GID %d  GID src %d  hash value %u. Timer hits in %.0lf usec", 
		      dataMsg.gid(), dataMsg.srcnode(), hashVal, tempTime);

}

//************************************************************************
// function to process messages received over the air
void GcnService::OnNetworkReceive(char* buffer, int len)
{
	
	// Suppress protobuf deserialization errors
	google::protobuf::LogSilencer logSilencer; 

#if 0
	LOG(LOG_DEBUG, "Received message of len = %d", len);
	for (int i = 0; i < len; i++)
	{
		printf("0x%x  ", (unsigned char)buffer[i]);
	}
	printf("\n");
#endif
	// Parse OTA message
	OTAMessage message;
	if(message.ParseFromArray(buffer, len))
	{
		// Check log level first because printing to string is expensive
		// and can affect throughput
		if (mCurrentLogLevel >= LOG_DEBUG)
		{ 
			string sMessage;
			mPbPrinter.PrintToString(message, &sMessage);
			LOG(LOG_DEBUG, "Received OTA (%d bytes):\n %s", len, sMessage.c_str());
		}
		
		if (message.header().src() != mNodeId)
		{
			// handle any Ack messages
			for ( auto & ack : *message.mutable_ack() ) 
			{
				processNetworkAck(ack, message.header().src());
			}
			
			// handle any Advertise messages
			for ( auto & advertise : *message.mutable_advertise() ) 
			{
				processNetworkAdvertise(advertise, message.header().src());
			}
			
			// handle any data pushes
			for ( auto & data : *message.mutable_data() )// process data message
			{
				processNetworkData(data, message.header().src());
			}
			
		}
		else
		{
			LOG(LOG_DEBUG, "Received OTA message with my source id (%d). Ignoring", message.header().src());
			dropCount++;
		}
	}
	else
	{
		//LOG(LOG_DEBUG, "Unable to deserialize message");
	}

} 

//************************************************************************
// function to pre process Data messages received from the network and the client
// This handles the hash, distance table and local delivery
bool GcnService::preProcessData(Data & dataMsg, HashValue & hashValue, NodeId msgOtaSrc, shared_ptr<ClientSession> pSession)
{
	// get hash Value. 
	// This sets hashValue and returns true if it is NOT already in the hash, 
	bool newToHash = addToHash(dataMsg, hashValue);
	
	// get get fields from message
	GroupId gid = dataMsg.gid();
	NodeId gidsrc = dataMsg.srcnode();
	uint32_t distance = dataMsg.distance();
	
	// First handle distance and packet count
	updateDistanceTable(gid, gidsrc, hashValue, distance, msgOtaSrc, newToHash, false);
	
	// Next handle local delivery
	// Check the "have we seen it" hash and only process if we have not seen this packet yet.
	if (newToHash)
	{
		if (dataMsg.has_uheader())
		{
			recvCountDataUni++;
			// only process if I am the destination
			if (dataMsg.uheader().unicastdest() == mNodeId)
			{
				// Check announce map - deliver to source client
				AnnounceIt appIt = mAnnounceTable.find(gid);
				if (appIt != mAnnounceTable.end())
				{
					forwardToApp(dataMsg, appIt->second.pSession);
					LOG(LOG_DEBUG, "Source Node: Received unicast DATA message we have not already seen. Forwarding to App");
					pushCount++;
				}
				// Also check local pull table - deliver to ALL subscribing clients
				if (mLocalPullTable.count(gid))
				{
					// We have local subscribers!
					// get all the local subscribers and send them the data
					LocalPullRangeIt rangeIt = mLocalPullTable.equal_range(gid);
					for (LocalPullIt iter = rangeIt.first; iter != rangeIt.second; ++iter)
					{
						// send over the socket specified by iter->second
						forwardToApp(dataMsg, iter->second);
						LOG(LOG_DEBUG, "Group Node: Received unicast DATA message we have not already seen. Forwarding to App");
						pushCount++;
					}
				}
			}
		}
		else
		{
			recvCountData++;
			// Do we have any local subscribers?
			if (mLocalPullTable.count(gid))
			{
				// We have local subscribers!
				// get all the local subscribers and send them the data
				LocalPullRangeIt rangeIt = mLocalPullTable.equal_range(gid);
				for (LocalPullIt iter = rangeIt.first; iter != rangeIt.second; ++iter)
				{
					// Only send to this socket if it is NOT the same socket
					// on which we received data. pSession could be NULL if this
					// data packet arrived over the air. If it arrived from a client
					// then pSession is not null and we compare it to the subscriber
					// socket. So send to the subscriber if we have no pSession (i.e., it
					// arrived OTA or pSession does not match
					if ( !pSession || (pSession != iter->second) )
					{
						// send over the socket specified by iter->second
						forwardToApp(dataMsg, iter->second);
						pushCount++;
					}
				}
			}
		}
	}
	return(newToHash);
}

//************************************************************************
// function to process Data messages received from the network
void GcnService::processNetworkData(Data & dataMsg, NodeId msgOtaSrc)
{
	uint32_t myDistance = 0;
	DistanceIt distIter;
	
	// Go ahead and increment the distance field so that it
	// is already set if the advertise message gets forwarded
	// Also need to update the distance so we store the correct
	// value in our distance table
	// NOTE: Do this before storing local value below
	dataMsg.set_distance(dataMsg.distance() + 1);
	
	// pre process the DATA message. This handles the hash, distance table and local delivery
	HashValue hashValue;
	bool newToHash = preProcessData(dataMsg, hashValue, msgOtaSrc);
	
	// get get fields from message
	GroupId gid = dataMsg.gid();
	NodeId gidsrc = dataMsg.srcnode();
	uint32_t ttl = dataMsg.ttl();
	
	// Set values used for if statements. Whether or not we are a group node
	// and whether or not this is using ADVERTISE/ACK
	// For the purposes of forwarding a DATA message, we are a group node
	// if we have a subscriber OR are a producer of data for the group 
	// (i.e., we are a group "participant"). Note however that if we are the source
	// of the DATA that it will NOT be newToHash and we will therefore ignore it
	// ***** TO DO *****
	// How do we know that this is a DATA for app with advertise/ack or not?
	// we are currently using src ttl to detect this. ADVERTISE/ACK application
	// DATA message do not have src ttl 
	// ****************
	bool groupNode = ( (mLocalPullTable.count(gid) > 0) || (mAnnounceTable.count(gid) > 0) );
	bool usingAck = !(dataMsg.has_srcttl());
	
	if(mDataFile != NULL) //DATAITEM
	{
		mRcvDataDI++;
		char buf[256];
		uint64_t millis = duration_cast<milliseconds>(getTime()).count();
		int buflen = sprintf(buf,"0,%.0f,ll.gcnRcvData,node%03d.gcnService,%.0f,\"{\"\"rcvfrom\"\":\"\"node%03d\"\",\"\"gid\"\":%d,\"\"size\"\":%d,\"\"seq\"\":%.0f,\"\"srcttl\"\":%d,\"\"orgsrc\"\":\"\"node%03d\"\",\"\"ttl\"\":%d,\"\"dist\"\":%d,\"\"newhash\"\":%d}\"\n",
			(double)mRcvDataDI,mNodeId,(double)millis,msgOtaSrc,gid,(int)dataMsg.data().size(),(double)dataMsg.sequence(),dataMsg.srcttl(),gidsrc,ttl,dataMsg.distance()-1,newToHash);
		fwrite(buf,sizeof(char),buflen,mDataFile);
		fflush(mDataFile);
	}
	
	// Now handle forwarding
	if ( dataMsg.has_uheader() )
	{
		// This is unicast message
		if ( (newToHash) && (dataMsg.uheader().unicastdest() != mNodeId) )
		{
			// I haven't seen this packet and I'm not destination
			// Compare relay distance to distance I have for this gid source
			uint32_t relayDistance = dataMsg.uheader().relaydistance();
			
			distIter = mDistanceTable.find(GIDKey(gid, dataMsg.uheader().unicastdest()));
			if (distIter != mDistanceTable.end())
			{
				myDistance = distIter->second.distance;
			}
			if ( (myDistance) && (myDistance <= relayDistance) )
			{
				if ( usingAck && ((groupNode && mAlwaysRebroadcast) || (mRemotePullTable.count(gid)))) // I am a relay for the corresponding one-to-many flow
				{
					auto uheader = dataMsg.mutable_uheader();
					uheader->set_relaydistance(myDistance - 1);
					
					LOG(LOG_DEBUG, "Received unicast DATA message with relayDistance %d. My Distance is %d to Node %d. Forwarding OTA",
									relayDistance, myDistance, dataMsg.uheader().unicastdest());
					setDataTimer(dataMsg, 1, hashValue);
					fwdCountUni++;
				}
				else if (!usingAck) 
				{
					if ( (ttl) && (!groupNode || dataMsg.has_nottlregen()) )
					{
						// TTL is NOT 0 and we are either a non-group node OR a group node that is NOT regenerating TTL
						// so we can forward the packet.
						auto uheader = dataMsg.mutable_uheader();
						uheader->set_relaydistance(myDistance - 1);
					
						LOG(LOG_DEBUG, "Received unicast DATA message with relayDistance %d. My Distance is %d to Node %d. Forwarding OTA",
										relayDistance, myDistance, dataMsg.uheader().unicastdest());
						setDataTimer(dataMsg, ttl, hashValue);
						fwdCountUni++;
					}
					else if (groupNode)
					{
						auto uheader = dataMsg.mutable_uheader();
						uheader->set_relaydistance(myDistance - 1);
					
						LOG(LOG_DEBUG, "Received unicast DATA message with relayDistance %d. My Distance is %d to Node %d. Forwarding OTA",
										relayDistance, myDistance, dataMsg.uheader().unicastdest());
						setDataTimer(dataMsg,  dataMsg.srcttl(), hashValue);
						fwdCountUni++;
					}
					else
					{
						LOG(LOG_DEBUG, "Received unicast DATA message with relayDistance %d. My Distance is %d to Node %d, with %d TTL. NOT Forwarding OTA",
								relayDistance, myDistance, dataMsg.uheader().unicastdest(),ttl);
					}
				} 
				else
				{
					LOG(LOG_DEBUG, "Received unicast DATA message with relayDistance %d. My Distance is %d to Node %d, with %d pull table entries. NOT Forwarding OTA",
								relayDistance, myDistance, dataMsg.uheader().unicastdest(),mRemotePullTable.count(gid));
				}
			}
			else
			{
				LOG(LOG_DEBUG, "Received unicast DATA message with relayDistance %d. My Distance is %d to Node %d. NOT Forwarding OTA",
								relayDistance, myDistance, dataMsg.uheader().unicastdest());
			}
		}
	}
	else if (dataMsg.srcnode() != mNodeId)
	{
		if (usingAck)
		{
			// This app is using ADVERTISE/ACK
			if ( (newToHash) && ( (groupNode && mAlwaysRebroadcast) || (mRemotePullTable.count(gid)) ) )
			{
				// We have not seen this packet and we have a downstream subscriber OR
				// we are a group node and are configured to always rebroadcast
				// so we need to forward this OTA
				// Use TTL of 1 so when it is sent, the actual sent TTL will be 0
				// (NOTE: ttl gets decremented before sending)
				setDataTimer(dataMsg, 1, hashValue);
				fwdCount++;
				// Mark flag for statistics
				if (groupNode)
					relayDataGroup=1;
				else
					relayDataNonGroup=1;
			}
		}
		else
		{
			// This is NOT using ADVERTISE/ACK 
			uint32_t srcTtl = dataMsg.srcttl();
			if (groupNode)
			{
				if (newToHash)
				{
					// We are a group node and haven't seen this packet
					if (!(dataMsg.has_nottlregen()))
					{
						// Message does not have nottlregen field so we ARE regenerating TTL
						// For this case: Reset ttl field to src ttl field value
						//  (NOTE: ttl gets decremented before sending)
						setDataTimer(dataMsg, srcTtl, hashValue);
						fwdCount++;
						relayDataGroup=1;
						LOG(LOG_DEBUG, "Group Node: Received DATA message we have not already seen. Forwarding OTA with regenerated TTL (ttl=%d)", srcTtl);
					}
					else if (ttl)
					{
						// Not regenerating TTL so just use the non-zero ttl value.
						setDataTimer(dataMsg, ttl, hashValue);
						fwdCount++;
						relayDataGroup=1;
						LOG(LOG_DEBUG, "Group Node: Received DATA message we have not already seen. Forwarding OTA w/o regenerating TTL (ttl=%d)", ttl);
					}
				}
			}
			else if (ttl)
			{
				// We are NOT a group node and TTL is not zero
				if (newToHash)
				{
					// We have NOT seen this packet yet so just send back out the raw socket
					// (NOTE: ttl gets decremented before sending)
					setDataTimer(dataMsg, ttl, hashValue);
					fwdCount++;
					relayDataNonGroup=1;
					LOG(LOG_DEBUG, "Non-Group Node: Received DATA message we have not already seen. Forwarding OTA");
				}
				else
				{
					// we have seen this message
					uint32_t maxTTL = getMaxTTLfromHash(hashValue);
					if (dataMsg.ttl() > maxTTL)
					{
						// Save off this new (higher TTL) as the max TTL
						changeMaxTTL(hashValue, ttl);
						
						// Construct message to send back out the raw socket
						// (NOTE: ttl gets decremented before sending)
						// Make sure we reset the distance before we send this out
						distIter = mDistanceTable.find(GIDKey(gid, gidsrc));
						LOG_ASSERT(distIter != mDistanceTable.end(), "Received message we have already seen but had no distance entry");
						dataMsg.set_distance(distIter->second.distance);
						
						// now send ota
						setDataTimer(dataMsg, ttl, hashValue);
						fwdCount++;
						relayDataNonGroup=1;
						LOG(LOG_DEBUG, "Non-Group Node: Received DATA message we have already seen. This msg has higher TTL. Forwarding OTA");
					}
				}
			}
		}
	}
}


//************************************************************************
// function to process Announce messages received from the network
void GcnService::processNetworkAdvertise(Advertise & advertiseMsg, NodeId msgOtaSrc)
{
	// Go ahead and increment the distance field so that it
	// is already set if the advertise message gets forwarded
	// Also need to update the distance so we store the correct
	// value in our distance table
	// NOTE: Do this before storing local value below
	advertiseMsg.set_distance(advertiseMsg.distance() + 1);
	
	// get hash Value. 
	// This sets hashValue and returns true if it is NOT already in the hash, 
	HashValue hashValue;
	bool newToHash = addToHash(advertiseMsg, hashValue);
	
	// get get fields from message
	GroupId gid = 	advertiseMsg.gid();
	uint32_t srcTtl = advertiseMsg.srcttl();
	NodeId gidsrc = advertiseMsg.srcnode();
	uint32_t ttl = 	advertiseMsg.ttl();
	uint32_t seq = 	advertiseMsg.sequence();
	uint32_t distance = 	advertiseMsg.distance();
	uint32_t probRelay = 	advertiseMsg.probrelay();
	
	// For the purposes of forwarding an ADVERTISE message, we are a group node
	// if we have a subscriber OR are a producer of data for the group 
	// (i.e., we are a group "participant"). Note however that if we are the source
	// of the ADVERTISE that it will NOT be newToHash and we will therefore ignore it
	bool groupNode = ( (mLocalPullTable.count(gid) > 0) || (mAnnounceTable.count(gid) > 0) );
	
	// Add advertisement to the set of adv seen below.
	// We use AdvSeen in the decision on forwarding an ACK.
	// It gets added ONLY for the cases which would cause
	// us to actually forward the Advertise (non group nodes
	// do not do that if ttl is 0 so we don't want to add this
	// item to the map in that case)
	// TO DO. IMPORTANT!!! For now there is NO cleanup of this
	// set!! That means that we track every unique advertisement
	// we have ever received (Unique means gid, gid src, seq #).
	// Since we are not running long simulations or scenarios 
	// for now this works BUT we need to implement a method
	// for cleaning out old entries. (Greg and I discussed this
	// but did not come to a final design)

	// Mark flag for statistics if I am not a group node
	if (!groupNode)
	{
		nonGroupRcvAdv=1;
		if (ttl)
			mAdvSeenSet.insert(AdvKey(gid,gidsrc,seq));
	}
	else
	{
		mAdvSeenSet.insert(AdvKey(gid,gidsrc,seq));
	}
	
	// First handle distance and packet count
	updateDistanceTable(gid, gidsrc, hashValue, distance, msgOtaSrc, newToHash, true);
	
	if (newToHash)
	{
		// we have NOT seen it so add/update entry in Reverse Path map
		recvCountAdv++;
		
		// get time stamp
		TimeDuration currDur = getTime();
		
		// Add this to the reverse path map
		// We use the first ADVERTISE received for a sequence # as the reverse path
		ReservePathIt iter = mReversePathTable.find(GIDKey(gid, gidsrc));
		if (iter != mReversePathTable.end())
		{
			// already have a reverse path entry in the table for this gid + gid src
			// update entry if the sequence number is newer than what we have already
			if (seq > iter->second.seqNum)
			{
				iter->second.srcNode = msgOtaSrc;
				iter->second.seqNum = seq;
				iter->second.timestamp = duration_cast<seconds>(currDur).count();
				iter->second.probRelay = probRelay;
			}
		}
		else
		{
			// add new entry
			RevPathInfo  mInfo;
			mInfo.srcNode = msgOtaSrc;
			mInfo.seqNum = seq;
			// convert timestamp to seconds
			mInfo.timestamp = duration_cast<seconds>(currDur).count();
			mInfo.probRelay = probRelay;

			// add to reverse path table
			mReversePathTable.insert(ReservePathPair(GIDKey(gid, gidsrc),mInfo));
		}
	}

	
	// Are we a group node?
	if (groupNode)
	{
		// We are a group node ... but have we seen this before?
		if (newToHash)
		{
			// Handle forwarding of ADVERTISE
			if (!(advertiseMsg.has_nottlregen()))
			{
				// Message does not have nottlregen field so we ARE regenerating TTL
				// For this case: Reset ttl field to src ttl field value
				//  (NOTE: ttl gets decremented before sending)
				setAdvTimer(advertiseMsg, srcTtl);
				fwdCount++;
				LOG(LOG_DEBUG, "Group Node: Received ADVERTISE message we have not already seen. Forwarding OTA with regenerated TTL (ttl=%d)", srcTtl);
			}
			else if (ttl)
			{
				// Not regenerating TTL so just use the non-zero ttl value.
				setAdvTimer(advertiseMsg, ttl);
				fwdCount++;
				LOG(LOG_DEBUG, "Group Node: Received ADVERTISE message we have not already seen. Forwarding OTA w/o regenerating TTL (ttl=%d)", ttl);
			}
			
			// Send an Ack message back to the msg src (not GID source)
			Ack ackMsg;
			// set the group id, gid src and sequence # fields of the ack
			ackMsg.set_gid(gid);
			ackMsg.set_srcnode(gidsrc);
			ackMsg.set_sequence(seq);
			ackMsg.set_obligatoryrelay(msgOtaSrc);
			
			// Set timer for sending the ACK messge. 
			setAckTimer(ackMsg);
			
			// If we are overriding advertisements then we use the receipt of an 
			// advertisement for our group id as indication that we can start
			// sending data. Normally the signal to start sending is based
			// on receipt of an ACK to our advertise but we aren't sending them
			// so we use this as the signal that the path is set up
			// TO DO: use of advertise override is a temporary way of forcing
			// nodes that are for the same group to have only one node set up
			// the group tree and the other source re-use it.
			AnnounceIt anncIt = mAnnounceTable.find(gid);
			if ( (anncIt != mAnnounceTable.end())  && (anncIt->second.interval == 0 ) )
			{
					// Only send PULL to app if we haven't sent one already
					if (!(anncIt->second.pullSentToApp) )
					{
						Pull pullMsg;
						pullMsg.set_gid(gid);
						forwardToApp(pullMsg, anncIt->second.pSession);
						// Set flag that we have sent pull to app
						anncIt->second.pullSentToApp = true;
					}
			}
		}
	}
	else
	{
		// we have no local apps that want this data 
		// Handle for Max TTL and just process for forwarding
		// if TTL is not 0
		if (ttl)
		{
			if (newToHash)
			{
				// We have NOT seen this packet yet so just send back out the raw socket
				// (NOTE: ttl gets decremented before sending)
				setAdvTimer(advertiseMsg, ttl);
				fwdCount++;
				LOG(LOG_DEBUG, "Non-Group Node: Received Announce message we have not already seen. Forwarding OTA");
			}
			else
			{
				// we have seen this message
				uint32_t maxTTL = getMaxTTLfromHash(hashValue);
				if (ttl > maxTTL)
				{
					// Save off this new (higher TTL) as the max TTL
					changeMaxTTL(hashValue, ttl);
					
					// Construct message to send back out the raw socket
					// (NOTE: ttl gets decremented before sending)
					// Make sure we reset the distance before we send this out
					DistanceIt distIter = mDistanceTable.find(GIDKey(gid, gidsrc));
					LOG_ASSERT(distIter != mDistanceTable.end(), "Received message we have already seen but had no distance entry");
					advertiseMsg.set_distance(distIter->second.distance);
					
					// NOTE: If we get here and we already have a timer for the previous lower ttl advertisement
					// that previous timer will be canceled and previous adv will not be sent. Just this new one will be sent!
					setAdvTimer(advertiseMsg, ttl);
					fwdCount++;
					LOG(LOG_DEBUG, "Non-Group Node: Received Announce message we have already seen. This msg has higher TTL. Forwarding OTA");
				}
				else
				{
					dropCount++;
					LOG(LOG_DEBUG, "Non-Group Node: Received Announce message we have already seen with higher TTL. Ignoring");
				}
			}
		}
		else
		{
			dropCount++;
			LOG(LOG_DEBUG, "Non-Group Node: Received Announce message with TTL 0. Ignoring");
		}
	}
	if(mDataFile != NULL) //DATAITEM
	{
		mRcvAdvDI++;
		char buf[256];
		uint64_t millis = duration_cast<milliseconds>(getTime()).count();
		int buflen = sprintf(buf,"0,%.0f,ll.gcnRcvAdv,node%03d.gcnService,%.0f,\"{\"\"rcvfrom\"\":\"\"node%03d\"\",\"\"gid\"\":%d,\"\"seq\"\":%.0f,\"\"orgsrc\"\":\"\"node%03d\"\",\"\"srcttl\"\":%d,\"\"ttl\"\":%d,\"\"dist\"\":%d,\"\"newhash\"\":%d,\"\"grpnode\"\":%d}\"\n",
			(double)mRcvAdvDI,mNodeId,(double)millis,msgOtaSrc,gid,(double)seq,gidsrc,srcTtl,ttl,distance,newToHash,groupNode);
		fwrite(buf,sizeof(char),buflen,mDataFile);
		fflush(mDataFile);
	}
}



//************************************************************************
// function to process Pull messages received from the network
void GcnService::processNetworkAck(Ack& ackMsg, NodeId msgOtaSrc)
{
	RemotePullIt pullIt;
	AnnounceIt anncIt;
	ReservePathIt revIt;
	
	recvCountAck++;
	
	TimeDuration currDur = getTime();
	int currTime = duration_cast<seconds>(currDur).count();
	
	// difference between msgOtaSrc and gidSrc is that msgOtaSrc is the node from
	// which this Ack was received and gidSrc is the source node of the
	// GID content
	
	// get fields from message
	GroupId gid = ackMsg.gid();
	uint32_t seq = ackMsg.sequence();
	NodeId gidsrc = ackMsg.srcnode();
	NodeId obligRelay = ackMsg.obligatoryrelay();
	uint32_t probRelay = ackMsg.probabilityofrelay();
	
	// For the purposes of forwarding an ADVERTISE message, we are a group node
	// if we have a subscriber OR are a producer of data for the group 
	// (i.e., we are a group "participant"). Note however that if we are the source
	// that it will NOT be newToHash and we will therefore ignore it
	bool groupNode = ( (mLocalPullTable.count(gid) > 0) || (mAnnounceTable.count(gid) > 0) );
	
	// Mark flag for statistics if I am not a group node
	if (!groupNode)
		nonGroupRcvAck = 1;
	
	// init a flag as to whether or not we put an entry in the Remote Pull Table.
	// We only do this if we actually send an ACK. It gets complicated below so
	// it is easier to just use a flag and add the entry at the end if needed.
	bool addRemotePull = false;
	
	// Set flag as to whether or not we have seen an advertisement
	// for this ack. We need to know if we have an Advertisement if
	// we are not obligatory relay so just figure that out here 
	// and set flag
	bool seenAdv = false;
	auto advIt = mAdvSeenSet.find(AdvKey(gid,gidsrc,seq));
	if (advIt != mAdvSeenSet.end())
	{
		seenAdv = true;
	}
	
	// Set flag as to whether or not we have already flipped a coin for this ACK.
	// This is based on sequence number, i.e., we can flip a coin
	// for each sequence number
	bool coinFlipped = true;
	auto coinIt = mCoinFlipTable.find(GIDKey(gid, gidsrc));
	if ( (coinIt == mCoinFlipTable.end()) || (seq > coinIt->second) )
	{
		// either we have no entry for this gid,gidsrc and have never flipped
		// a coin for the gid,gidsrc OR we have an entry but this is a
		// new sequence number. For either case we act as though we have never
		// flipped the coin
		coinFlipped = false;
	}
	
	// Set flag as to whether or not we have already forwarded an ACK.
	// This is based on sequence number, i.e., we can send an ACK
	// for each sequence number
	bool ackSent = true;
	auto ackSentIt = mAckSentTable.find(GIDKey(gid, gidsrc));
	if ( (ackSentIt == mAckSentTable.end()) || (seq > ackSentIt->second) )
	{
		// either we have no entry for this gid,gidsrc and have never sent
		// an ack for the gid,gidsrc OR we have an entry but this is a
		// new sequence number. For either case we are eligible to send an ACK
		ackSent = false;
	}
	
	// Handle ACK processing
	// 1. First see if we are the GID source. 
	// If so then find the app that announced this content and send the pull
	// so it knows there is a subscriber and can now send the content.
	// We can't just look to see if we have this GID in our Announce Table
	// because there can be multiple sources for a GID. That means we could
	// have an entry in the Announce Table for this GID but we are not the source
	// node that this ack is intended for. So first, check the GID source in the ack.
	// If we match then we are source
	if (gidsrc == mNodeId)
	{
		anncIt = mAnnounceTable.find(gid);
		if (anncIt != mAnnounceTable.end())
		{
			// Set flag so we add a remote pull entry
			addRemotePull = true;
			
			// Only send PULL to app if we haven't sent one already
			if (!(anncIt->second.pullSentToApp) )
			{
				Pull pullMsg;
				pullMsg.set_gid(gid);
				forwardToApp(pullMsg, anncIt->second.pSession);
				// Set flag that we have sent pull to app
				anncIt->second.pullSentToApp = true;
			}
		}
	}
	// 2. See if we are the obligatory relay and forward ACK if we are
	else if (obligRelay == mNodeId)
	{
		// Get reverse path
		// Because we are obligatory relay we expect to have an entry!
		revIt = mReversePathTable.find(GIDKey(gid, gidsrc));
		LOG_ASSERT(revIt != mReversePathTable.end(), "Obligatory relay node received ACK but had no reverse path for GID %d GID src\n", gid, gidsrc);
		
		// We are a relay node since we were selected as obligatory relay. That means we have to send an ACK
		// to tell the source node that there is a subscriber upstream from us. HOWEVER, if we are a group
		// node then we already sent an ACK when we received the advertise message. We don't need to
		// send another one. It is also possible that we already sent an ack for this seq # as a non-group node
		// which can happen if we received an ack and were oblig relay or won the coin toss
		if ( (!groupNode) && (!ackSent) )
		{
			LOG(LOG_DEBUG, "Received ACK. We are obligatory relay for gid %d gid src %d seq %d. Forwarding ACK.", gid, gidsrc, seq);
			// Forward ACK to our next hop on reverse path
			ackMsg.set_obligatoryrelay(revIt->second.srcNode);
			// Set timer for sending the ACK messge. 
			setAckTimer(ackMsg);
			
			// We have just sent ack for this gid, gidsrc, seq # so we must add an entry to the map
			if (ackSentIt == mAckSentTable.end())
			{
				mAckSentTable.insert(SequencePair(GIDKey(gid, gidsrc), seq));
			}
			else if (seq > ackSentIt->second)
			{
				ackSentIt->second = seq;
			}
		}
		else if (groupNode)
			LOG(LOG_DEBUG, "Received ACK. We are obligatory relay for gid %d gid src %d seq %d. Group node so NOT Forwarding ACK.", gid, gidsrc, seq);
		else
			LOG(LOG_DEBUG, "Received ACK. We are obligatory relay for gid %d gid src %d seq %d. Already sent ack. NOT Forwarding ACK.", gid, gidsrc, seq);
		
		// Set flag so we add a remote pull entry -- this is how we say "I am a relay node"
		addRemotePull = true;
	}
	// 3. See if we should still forward this ACK based on coinflip
	// We have to have actually seen an advertisement for this seq #,
	// not have already done the coin flip in order to flip the coin
	else if ( (seenAdv) && (!coinFlipped) && (probRelay) && coinFlip(probRelay))
	{
		// Yeah! We won the coin flip! We get to be a relay.
		// first get entry in reverse path so we know who to send back to.
		// If we don't have an entry, that is a problem! seenAdv is true so
		// we had to have seen an Adv 
		revIt = mReversePathTable.find(GIDKey(gid, gidsrc));
		LOG_ASSERT(revIt != mReversePathTable.end(), "We are not obligatory relay node received ACK but had no reverse path for GID %d GID src\n", gid, gidsrc);
		
		// We are a relay node since we won the coin toss. just like above,
		// no need to send ack if we are a group node
		if (!groupNode && !ackSent)
		{
			LOG(LOG_DEBUG, "Received ACK. We are not obligatory relay. Won coin toss for gid %d gid src %d seq %d. Forwarding ACK.", gid, gidsrc, seq);
			// Forward ACK to our next hop on reverse path
			ackMsg.set_obligatoryrelay(revIt->second.srcNode);
			// Set timer for sending the ACK messge. 
			setAckTimer(ackMsg);
			
			// We have just sent ack for this gid, gidsrc, seq # so we must add an entry to the map
			if (ackSentIt == mAckSentTable.end())
			{
				mAckSentTable.insert(SequencePair(GIDKey(gid, gidsrc), seq));
			}
			else if (seq > ackSentIt->second)
			{
				ackSentIt->second = seq;
			}
		}
		else if (groupNode)
			LOG(LOG_DEBUG, "Received ACK. We are not obligatory relay. Won coin toss for gid %d gid src %d seq %d but we are Group node. NOT Forwarding ACK.", gid, gidsrc, seq);
		else
			LOG(LOG_DEBUG, "Received ACK. We are not obligatory relay but already sent ack for gid %d gid src %d seq %d. NOT Forwarding ACK.", gid, gidsrc, seq);

		// Set flag so we add a remote pull entry -- this is how we say "I am a relay node"
		addRemotePull = true;
		
		// We have just done a coin flip for this gid, gidsrc, seq # so we must add an entry to the map
		if (coinIt == mCoinFlipTable.end())
		{
			mCoinFlipTable.insert(SequencePair(GIDKey(gid, gidsrc), seq));
		}
		else if (seq > coinIt->second)
		{
			coinIt->second = seq;
		}
	}
	else
	{
		if (!seenAdv)
			LOG(LOG_DEBUG, "Received ACK. We are not obligatory relay but have not seen advertise for gid %d gid src %d seq %d. NOT Forwarding ACK.", gid, gidsrc, seq);
		else if (coinFlipped)
			LOG(LOG_DEBUG, "Received ACK. We are not obligatory relay but already did coin toss for gid %d gid src %d seq %d. NOT Forwarding ACK.", probRelay, gid, gidsrc, seq);
		else
		{
			LOG(LOG_DEBUG, "Received ACK. We are not obligatory relay and lost coin toss with prob of %d for gid %d gid src %d seq %d. NOT Forwarding ACK.", probRelay, gid, gidsrc, seq);
			
			// We have just done a coin flip for this gid, gidsrc, seq # so we must add an entry to the map
			if (coinIt == mCoinFlipTable.end())
			{
				mCoinFlipTable.insert(SequencePair(GIDKey(gid, gidsrc), seq));
			}
			else if (seq > coinIt->second)
			{
				coinIt->second = seq;
			}
		}
	}

	
	// Finally, add entry to remote pull table if flag was set
	if (addRemotePull)
	{
		// Add this entry to our Remote pull map if not already in the map
		// get all the entries that have this gid and see if we find the node id
		RemotePullRangeIt rangeIt = mRemotePullTable.equal_range(gid);
		for (pullIt = rangeIt.first; pullIt != rangeIt.second; ++pullIt)
		{
			// Did the mapped value match the src?
			if (pullIt->second.nodeId == msgOtaSrc)
			{
				// reset time stamp
				pullIt->second.timestamp = currTime;
				LOG(LOG_DEBUG, "Found gid %d msgOtaSrc %d in remote Pull table", gid, msgOtaSrc);
				break;
			}
		}
		// Did we find an entry?
		if (pullIt == rangeIt.second)
		{
			// did not find an entry so add it
			RemotePullInfo info;
			info.nodeId = msgOtaSrc;
			// set the timestamp 
			info.timestamp = currTime;
			
			mRemotePullTable.insert(RemotePullPair(gid, info));
			LOG(LOG_DEBUG, "Added gid %d msgOtaSrc %d to remote Pull table", gid, msgOtaSrc);
		}
	}
	
	if(mDataFile != NULL) //DATAITEM
	{
		mRcvAckDI++;
		char buf[256];
		uint64_t millis = duration_cast<milliseconds>(getTime()).count();
		int buflen = sprintf(buf,"0,%.0f,ll.gcnRcvAck,node%03d.gcnService,%.0f,\"{\"\"rcvfrom\"\":\"\"node%03d\"\",\"\"gid\"\":%d,\"\"seq\"\":%.0f,\"\"orgsrc\"\":\"\"node%03d\"\",\"\"grpnode\"\":%d,\"\"obligrelay\"\":%d,\"\"probrelay\"\":%d,\"\"addremotepull\"\":%d,\"\"seenadv\"\":%d,\"\"coinflipped\"\":%d,\"\"acksent\"\":%d}\"\n",
			(double)mRcvAckDI,mNodeId,(double)millis,msgOtaSrc,gid,(double)seq,gidsrc,groupNode,obligRelay,probRelay,addRemotePull,seenAdv,coinFlipped,ackSent);
		fwrite(buf,sizeof(char),buflen,mDataFile);
		fflush(mDataFile);
	}
}

//************************************************************************
// function to process messages received from client
void GcnService::OnClientReceive(shared_ptr<ClientSession> pSession, char* buffer, int len)
{
	clientRcvCount++;
	AnnounceIt iter2;
	shared_ptr<AnnounceIt> pIter;
	

	// Parse App message
	AppMessage message;
	
	if(message.ParseFromArray(buffer, len))
	{
		string sMessage;
		mPbPrinter.PrintToString(message, &sMessage);
		LOG(LOG_DEBUG, "Received Message:\n%s", sMessage.c_str());
		
		// handle any pulls that are in the message
		for ( auto & pull : *message.mutable_pull() )
		{
			// Add this entry to our local pull map
			mLocalPullTable.insert(LocalPullPair(pull.gid(), pSession));
			LOG(LOG_DEBUG, "Added gid %d to local Pull table", pull.gid());
			
			// PREVIOUSLY: we would check to see if we have a local
			// source for the gid and if we do but have not sent
			// a PULL to that source, then we would send one here
			// because we now have a local subscriber to the group.
			
			// NOW: We do NOT send a PULL to the local client based
			// on having a local subscriber. 
			// For group tree (i.e., we are using Advertise/Ack) the
			// source client does not send data until is knows there
			// is a remote subscriber. That is, it must receive an
			// Ack over the air. The local subscriber is NOT enough
			// to start sending. This means we could have a local subscriber
			// but the source will NOT send it any data until it knows of
			// a remote subscriber.
			if(mDataFile != NULL) //DATAITEM
			{
				mLocalPullDI++;
				char buf[256];
				uint64_t millis = duration_cast<milliseconds>(getTime()).count();
				int buflen = sprintf(buf,"0,%.0f,ll.gcnLocalPull,node%03d.gcnService,%.0f,\"{\"\"gid\"\":%d}\"\n",
					(double)mLocalPullDI,mNodeId,(double)millis,pull.gid());
				fwrite(buf,sizeof(char),buflen,mDataFile);
				fflush(mDataFile);
			}
		}

		// handle any unpulls that are in the mesage
		for ( auto & unpull : *message.mutable_unpull() )
		{
			// find and remove this entry from our local pull map
			LocalPullRangeIt rangeIt = mLocalPullTable.equal_range(unpull.gid());
			// loop over all entries that have this group id and find the entry
			// for this socket
			for (LocalPullIt iter = rangeIt.first; iter != rangeIt.second; ++iter)
			{
				if (iter->second == pSession)
				{
					mLocalPullTable.erase(iter);
					if(mDataFile != NULL) //DATAITEM
					{
						mLocalUnpullDI++;
						char buf[256];
						uint64_t millis = duration_cast<milliseconds>(getTime()).count();
						int buflen = sprintf(buf,"0,%.0f,ll.gcnLocalUnpull,node%03d.gcnService,%.0f,\"{\"\"gid\"\":%d}\"\n",
							(double)mLocalUnpullDI,mNodeId,(double)millis,unpull.gid());
						fwrite(buf,sizeof(char),buflen,mDataFile);
						fflush(mDataFile);
					}
					break;
				}
			}
		}

		// handle any data pushes
		for ( auto & data : *message.mutable_data() )
		{
			// add the distance and src node fields
			// IMPORTANT: The src node must be added BEFORE adding this
			// to the hash so that when our neighbor send it OTA and we
			// receive it, we get a hash match
			data.set_distance(0);
			data.set_srcnode(mNodeId);
			GroupId gid = data.gid();
			if(mSeqNumByGID.find(gid) == mSeqNumByGID.end())
			{
				mSeqNumByGID[gid] = 0;
			}
			mSeqNumByGID[gid]++;
			data.set_sequence(mSeqNumByGID[gid]);
			
			// pre process the DATA message. This handles the hash, distance table and local delivery
			HashValue tempHash;
			preProcessData(data, tempHash, mNodeId, pSession);
			
			// set flag for advertise override. Here we assume we are overriding
			// then look for interval > 0 and reset it.
			// Only do this if this is not unicast. Unicast may not have an entry
			// in the announce table if the node is a GID destination that wants to
			// send unicast responses to source. Also, advertiseOverride is not used
			// for unicast so it doesn't even matter what the value is.
			bool advertiseOverride = true;
			if ( !(data.has_uheader()) )
			{
				AnnounceIt anncIt = mAnnounceTable.find(data.gid());
				LOG_ASSERT(anncIt != mAnnounceTable.end(), "Could not find GID %d in announce table", data.gid());
				if ( anncIt->second.interval > 0 )
				{
					advertiseOverride = false;
				}
			}
			
			
			// ***** TO DO *****
			// 1. How do we know how to set the initial relay distance in the packet?
			//    Currently using the distance to the source node (unicast dest) from the distance table
			//    but in the future we will want to use some other value for robustness
			//    such as distance to source + N
			// 2. How do we know that this is a DATA for app with advertise/ack or not?
			//    Currently using src ttl to detect this. ADVERTISE/ACK application
			//    DATA message do not have src ttl
			// ****************
			if (data.has_uheader())
			{
				DistanceIt iter3 = mDistanceTable.find(GIDKey(data.gid(), data.uheader().unicastdest()));
				if (iter3 == mDistanceTable.end())
				{
					LOG(LOG_WARN,"Received unicast message but had no distance entry");
					continue;
				}

				auto pHeader = data.mutable_uheader();
				GCNMessage::UnicastResilience  resil = data.uheader().resilience();
				
				// ***** TO DO *****
				// What are we going to actually do with resilience?
				// For now just use distance - 1, distance or distance + 1
				// Note that this could be done in one line:  pHeader->set_relaydistance(iter3->second.distance + resil)
				// But keeping the switch statement for now in case we change how this is set.
				// ****************
				if (iter3->second.distance > 0)
				{
					int tempRelayDist=0;
					switch(resil)
					{
						case 0:
							tempRelayDist = iter3->second.distance-1;
							break;
						case 1:
							tempRelayDist = iter3->second.distance;
							break;
						case 2:
							tempRelayDist = iter3->second.distance + 1;
							break;
						default:
							tempRelayDist = iter3->second.distance - 1;
							break;
					}
					pHeader->set_relaydistance(tempRelayDist);
					
					// clear the resilience field so it is not sent OTA
					pHeader->clear_resilience();

					LOG(LOG_DEBUG, "Forwarded Unicast Data message OTA for destination GID %d node %d distance %d Relay distance %d", data.gid(), data.uheader().unicastdest(), iter3->second.distance, tempRelayDist);
					if (data.has_srcttl())
						forwardToOTA(data,data.srcttl());
					else
						forwardToOTA(data, 1);
				} else
				{
					LOG(LOG_DEBUG, "*Not* Forwarding Unicast Data message for destination GID %d node %d Relay distance %d", data.gid(), data.uheader().unicastdest(), iter3->second.distance);
				}
			}
			else if (data.has_srcttl())
			{
				// we aren't using ADVERTISE/ACK so use the src ttl in push
				forwardToOTA(data, data.srcttl());
			}
			else if ( (mRemotePullTable.count(data.gid()) != 0) || advertiseOverride )
			{
				// We are using ADVERTISE/ACK and we have either:
				// a) set up the path with ADVERTISE/ACK which we know because we
				//    have a remote pull entry
				// OR
				// b) we are overriding ADVERTISE in which case we will never have an 
				//    entry in remote pull table but the app will only send if we
				//    heard another group node's advertisement
				// In either case we want to forward out the ota link.
				// We use TTL of 1 so when it is sent, the actual sent TTL will be 0
				// (NOTE: ttl gets decremented before sending)
				// PUSH is always sent with TTL = 0
				// WHY do we check for an entry in remote pull table when we re using Advertise?
				// Well, it is possible that we have a subscriber on this node.
				// In that case, as soon as the GID is advertised, we send a PULL
				// to the source client to tell it we have a subscriber (the local subscriber)
				// We may not yet have any remote subscribers to we must have at least one
				// before we start sending the data over the air.
				// This allows a GID source to send the data to a subscriber that is local
				// but still have to send out advertise and get an ack before sending OTA.
				forwardToOTA(data, 1);
			}
			sentCount++;
		}
		
		// handle any advertise messages
		for ( auto & advertise : *message.mutable_advertise() )
		{
			// get fields from message
			GroupId gid = advertise.gid();
			uint32_t srcTtl = advertise.srcttl();
			uint32_t advType = advertise.type();
			int32_t interval = -1;
			uint32_t probRelay = 0;
			// If the advertise has an interval then we need to get that plus the prob relay values
			if (advertise.has_interval())
			{
				interval = advertise.interval();
				probRelay = advertise.probrelay();
			}
			
			LOG(LOG_DEBUG, "Received ADVERTISE for group %d of type %d.", gid, advType);
		
			// get iter to any current entry we might have in announce map
			iter2 = mAnnounceTable.find(gid);
			
			if (advType == DEREGISTER)
			{
				LOG_ASSERT(iter2 != mAnnounceTable.end(), "Received an DE-REGISTER ADVERTISE message for GID %d but had no entry in Announce Table", gid);
			
				if ( iter2->second.interval > 0 )
				{
					iter2->second.pTimer->cancel();
				}
				
				// This app has stopped being a source for the GID so delete it from
				// the Announce table
				mAnnounceTable.erase(iter2);
			}
			else
			{
				if (iter2 == mAnnounceTable.end())
				{
					// this is a new group. Add this to our announce map
					AnnounceInfo info;
					info.pSession = pSession;
					info.interval = interval;
					info.probRelay = probRelay;
					info.srcTtl = srcTtl;
					info.seqNum = 0;
					info.pullSentToApp = false;
					info.noTtlRegen = false;
					
					// Set up the return value from insert (which is a pair with iter and a bool)
					std::pair<AnnounceIt,bool> ret = mAnnounceTable.insert(AnnouncePair(gid, info));
					LOG(LOG_DEBUG, "Added gid %d to local Announce table with interval %d", gid, interval);
					
					if (interval > 0)
					{
						LOG_ASSERT(interval < mRemotePullExpireTime, "Received ANNOUNCE for group %d but the interval (%d) is higher than the Remote Pull Expire Time (%lf)", gid, interval, mRemotePullExpireTime);
						
						// Is this app regenerating TTL or not?
						if (advertise.has_nottlregen())
						{
							ret.first->second.noTtlRegen = true;
							LOG(LOG_DEBUG, "gid %d is not regenerating TTL", gid);
						}
						
						// Start periodic event to send announcements. first one sent 10 sec from now.
						// (yes this next line looks weird with the first->second! But "ret->first" is
						//  an AnnounceIt and we need to get to the mapped value which is "->second")
						ret.first->second.pTimer.reset(new deadline_timer(*pIoService, Seconds(10.0)));
						// Set args of the handler to be a shared pointer to the iter that is the first part of the pair
						// returned on the insert
						pIter = make_shared<AnnounceIt> (ret.first);
						// The handler takes an iterator to the entry in the announce table
						ret.first->second.pTimer->async_wait(boost::bind(&GcnService::OnAnnounceTimeout, this, _1, pIter));
						
						// PREVIOUSLY: we would check to see if we have a local
						// subscriber for the gid and if we do but have not sent
						// a PULL to that source, then we would send one here
						// because we have a local subscriber to the group.
						
						// NOW: We do NOT send a PULL to the local client based
						// on having a local subscriber. 
						// For group tree (i.e., we are using Advertise/Ack) the
						// source client does not send data until is knows there
						// is a remote subscriber. That is, it must receive an
						// Ack over the air. The local subscriber is NOT enough
						// to start sending. This means we could have a local subscriber
						// but the source will NOT send it any data until it knows of
						// a remote subscriber.
					}
				}
				else
				{
					// We already have an entry for this group. This would then be an update to
					// the announcement interval or src ttl or ttl regeneration or prob relay
					if (iter2->second.interval != interval)
					{
						// If current interval is > 0 cancel the existing timer
						// but only if we actually set the timer. If we are set
						// to override advertisements then there is nothing to cancel
						if ( iter2->second.interval > 0 )
						{
							
							iter2->second.pTimer->cancel();
							iter2->second.pTimer.reset();
						} 
						
						// If new interval is > 0 set the timer
						if ( interval > 0 )
						{
							// start periodic event to send announcements. first one sent 1.0 sec from now.
							// The handler takes an iterator to the entry in the announce table
							iter2->second.pTimer.reset(new deadline_timer(*pIoService, Seconds(1.0)));
							pIter = make_shared<AnnounceIt> (iter2);
							iter2->second.pTimer->async_wait(boost::bind(&GcnService::OnAnnounceTimeout, this, _1, pIter));
						}
						
						iter2->second.interval = interval;
						LOG(LOG_DEBUG, "Interval for gid %d changed to %lf", gid, iter2->second.interval);
					}
					
					if (iter2->second.srcTtl != advertise.srcttl())
					{
						iter2->second.srcTtl = advertise.srcttl();
						LOG(LOG_DEBUG, "Src TTL for gid %d changed to %d", gid, iter2->second.srcTtl);
					}
					
					if (iter2->second.probRelay != advertise.probrelay())
					{
						iter2->second.probRelay = advertise.probrelay();
						LOG(LOG_DEBUG, "Prob of Relay for gid %d changed to %d", gid, iter2->second.probRelay);
					}
					
					// Is this app regenerating TTL or not?
					if (advertise.has_nottlregen())
					{
						iter2->second.noTtlRegen = true;
					}
					else
					{
						iter2->second.noTtlRegen = false;
					}
				}
			}
		}
	}
	else
	{
		//LOG(LOG_DEBUG, "Unable to deserialize message");
	}
}

void ClientSession::read(function<void(shared_ptr<ClientSession>, char* buffer, int length)> & receiveHandler,
			 function<void(shared_ptr<ClientSession>)> & closeHandler)
{
	// Create buffer
	BufferPtr pBuffer(new Buffer);
	
	// First we read the size of the incoming message, then the message itself into the pBuffer
	auto self(shared_from_this());
	async_read(mSocket, buffer(pBuffer->data(), mSizeOfSize),
		[this, self, pBuffer, &receiveHandler, &closeHandler](error_code ec, size_t length)
		{
			if (!ec)
			{
				// get the size 
				/*
				printf("Read message size\n");
				for (unsigned int i = 0; i < length; i++)
				{
					printf("0x%x  ", (unsigned char)(pBuffer->data())[i]);
				}
				printf("\n");
				*/
				
				uint32_t messageSize{};
				memcpy(&messageSize, pBuffer->data(), length);
				messageSize = ntohl(messageSize);
				//printf ("message size is %d\n", messageSize);

				// read the message
				if ( (messageSize) && (messageSize <= MAX_BUFFER_SIZE - mSizeOfSize) )
				{
					auto self(shared_from_this());
					async_read(mSocket, buffer(pBuffer->data() + mSizeOfSize, messageSize),
						[this, self, pBuffer, &receiveHandler, &closeHandler](error_code ec, size_t length)
						{
							if (!ec)
							{
								/*
								printf("Read message\n");
								for (unsigned int i = 0; i < length; i++)
								{
									printf("0x%x  ", (unsigned char)(pBuffer->data())[i]);
								}
								printf("\n");
								*/
								
								receiveHandler(self, pBuffer->data() + mSizeOfSize, length);
								read(receiveHandler, closeHandler);
							}
							else
							{
								printf("ClientSession::read error: %s\n", ec.message().c_str());
								closeHandler(self);
							}
						});
				}
				else
				{
					printf("ClientSession::read messageSize check failed. messageSize is %u\n", messageSize);
					read(receiveHandler, closeHandler);
				}
			}
			else
			{
				printf("ClientSession::read error: %s\n",ec.message().c_str());
				closeHandler(self);
			}
		});
}

void ClientSession::write(BufferPtr pBuffer, int length)
{
	
	auto self(shared_from_this());
	async_write(mSocket, buffer(pBuffer->data(), length),
		    [this, self, pBuffer](error_code ec, size_t length)
		    {
			    if (ec)
				    {
					    fprintf(stderr, ">>>>> ClientSession Write error: %s\n", ec.message().c_str());
				    }
		    });
}



