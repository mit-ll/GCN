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

#include "gcnClient.h"



//************************************************************************
gcnClient::gcnClient(io_service * io_serv)
 : mResolver(*io_serv),
   mSocket(*io_serv),
   mSocketConnected(false),
   mStatTimer(*io_serv, Seconds(1)),
   mStatInterval(1.0),
   mDataProdDI(0),
   mDataRcvDI(0)
{
	mPbPrinter.SetInitialIndentLevel(1);

	// Init class attributes
	mSizeOfSize = sizeof(uint32_t); 
}


//************************************************************************
gcnClient::~gcnClient()
{
	printf("GCN_Client destructor complete\n");
}

//************************************************************************
bool gcnClient::Start(const ClientConfig &config, function<bool(Data & dataMsg)> procFunc)
{
	// Set the values that are the same across all clients
	mCurrentLogLevel = config.logLevel;
	mNodeId = config.nodeId;
	mPort = config.port;
	mDataFile = NULL;
	mDataFilePath = "";
	
	// set values specific to this client
	ClientGroupInfo  clientInfo;
	clientInfo.mType = config.type;
	clientInfo.mSrcttl = config.srcttl;
	clientInfo.mAnnounceRate = config.announceRate;
	clientInfo.mMsgHandler = procFunc;
	clientInfo.mSendResponse = config.sendResponse;
	clientInfo.mSendResponseFreq = config.sendRespFreq;
	clientInfo.mResponseTtl = config.respTtl;
	clientInfo.mHasSubscribers = true;
	clientInfo.mResilience = config.resilience;
	clientInfo.mRegenerateTtl = config.regenerateTtl;
	clientInfo.mAckProbRelay = config.ackProbRelay;
	// init dest to be non-unicast
	clientInfo.mDest = 0;
	// init all stats to 0
	clientInfo.recvCount = 0;
	clientInfo.sendCount = 0;
	clientInfo.rerrCount = 0;
	clientInfo.serrCount = 0;
	clientInfo.recvCountUni = 0;
	clientInfo.sendCountUni = 0;
	mDataFilePath = config.dataFile;
	
	// Add to map and save iter to entry
	std::pair<ClientIt,bool> ret;
	ret = mClientMap.insert(ClientPair(config.gid,clientInfo));
	ClientIt iter = ret.first;
	
	if(mDataFilePath != "") mDataFile = fopen(mDataFilePath.c_str(),"w+");
		
	// open socket to the GCN. retry for 30 seconds before giving up
	// if socket is not already connected
	if (!mSocketConnected)
	{
		int timeoutSec = 30;
		error_code ec;
		while (timeoutSec)
		{
			char buf[64]; sprintf(buf,"%d",mPort);
			connect(mSocket, mResolver.resolve({DEFAULT_SERVER_HOST, string(buf)}), ec);
			if (ec)
			{
				sleep(1);
				timeoutSec--;
				cout << "Not yet connected to server. error code is " << ec << ". " << timeoutSec << " seconds remaining to connect ..." << endl;
				if (!timeoutSec)
				{
					printf("ERROR: Failed to connect to server\n");
					return(false);
				}
			}
			else
			{
				timeoutSec = 0;
				mSocketConnected = true;
				printf("SUCCESS: client connected\n");
			}
		}
		
		// start the stat timer just once
		mStatTimer.async_wait(boost::bind(&gcnClient::OnStatTimeout, this));

		// start listening to GCN
		recvFromGCN();
	}

	// Set timer for sending advertise
	if (iter->second.mType > 0)
	{
		// Send advertise to GCN so it knows we have this content
		sendAdvertise(iter->first, REGISTER);
		
		if (iter->second.mAnnounceRate >= 0)
		{
			// Don't send data until we have a subscriber
			iter->second.mHasSubscribers = false;
		}
		
		LOG(LOG_FORCE, "Starting GCN Client Sender with:\n   NodeId: %d\n   Type: %s\n   Log Level: %s\n   Group Id: %d\n   Port: %d\n   Src TTL: %d\n   PUSH Rate: %lf\n   Announce Rate: %lf\n   Send Unicast: %s\n   Regenerate TTL: %s\n\n",
					mNodeId, AppTypeStr[config.type], LogLevelStr[mCurrentLogLevel], config.gid, mPort, config.srcttl, config.pushRate, config.announceRate, (config.sendResponse ? "True": "False"), (config.regenerateTtl ? "True" : "False"));
	}

	if ( (iter->second.mType == 0) || (iter->second.mType == 2) )
	{
		// We are a listener so send a pull to subscribe
		sendPull(iter->first);
		LOG(LOG_FORCE, "Starting GCN Client Listener with:\n   NodeId: %d\n   Type: %s\n   Log Level: %s\n   Group Id: %d\n   Port: %d\n   Send Unicast Response: %s\n   Send Unicast Response Frequency: %d\n   Unicast TTL: %d\n   Unicast Resilience: %s\n",
					mNodeId, AppTypeStr[config.type], LogLevelStr[mCurrentLogLevel], config.gid, mPort, (config.sendResponse ? "True": "False"), config.sendRespFreq, config.respTtl, UnicastResilience_Name(config.resilience).c_str());
	}

	return true;
}  // end gcnClient::Start()


//************************************************************************
void gcnClient::Stop()
{
	// tell GCN we are gone and stop timers and close sockets
	for (ClientIt iter = mClientMap.begin(); iter != mClientMap.end(); ++iter)
	{
		if (iter->second.mType > 0) 
		{
			// We are sender and so send an advertise to tell GCN we no longer have content
			sendAdvertise(iter->first, DEREGISTER);
		}
		
		if ( (iter->second.mType == 0) || (iter->second.mType == 2) )
		{
			// We need to unsubscribe
			printf(" ... sending UNPULL for GID %d\n", iter->first);
			sendUnpull(iter->first);
		}
		
		printf("\nSTOPPING GCN Client. GID %d Final stats: rcvd>%d sent>%d rerr>%d serr>%d rcvdUni>%d sentUni> %d\n", 
				iter->first, iter->second.recvCount, iter->second.sendCount, iter->second.rerrCount, iter->second.serrCount, iter->second.recvCountUni, iter->second.sendCountUni);
	
	}
	
	// print final stats
	mStatInterval = 0;
	OnStatTimeout();
	
	// Stop events
	mStatTimer.cancel();
	printf(" ... Stat event canceled\n");
		
	
	// Delete all global objects allocated by libprotobuf.
	google::protobuf::ShutdownProtobufLibrary();
	printf(" ... google buffer shutdown\n");
	
	// close socket to GCN
	mSocket.close();
	printf(" ... Socket closed\n");
	
	if(mDataFilePath != "") fclose(mDataFile);
}

//************************************************************************
// function to send a message to the GCN 
bool gcnClient::sendMessage(GroupId gid, char* msgBuffer, NodeId dest)
{
	ClientIt it = mClientMap.find(gid);
	LOG_ASSERT(it != mClientMap.end(), "Could not find GID %d in client map", gid);
	
	// Create App message, fill in push and send out socket
	AppMessage message;
	auto pData = message.add_data();
	pData->set_gid(gid);
	pData->set_data(msgBuffer);
	
	// We go ahead and build the message in case we are collecting data
	// But we don't send it unless we actually have subscribers
	// Also, we don't want to peg the client send stats if we have none
	bool hasSubs = it->second.mHasSubscribers;
	
	if (dest)
	{
		//This is a unicast packet. Build the header and add to data message
		auto pHeader = pData->mutable_uheader();
		pHeader->set_unicastdest(dest);
		pHeader->set_resilience(it->second.mResilience);
		// If we are not using ADVERTISE/ACK then we need to fill in the src ttl field
		if (it->second.mAnnounceRate < 0)
			pData->set_srcttl(it->second.mResponseTtl);

		if (hasSubs)
			it->second.sendCountUni++;
	}
	else if (it->second.mDest)
	{
		// I am a GID source and am now sending data as unicast. Build the header and add to data message
		auto pHeader = pData->mutable_uheader();
		pHeader->set_unicastdest(it->second.mDest);
		pHeader->set_resilience(it->second.mResilience);
		// If we are not using ADVERTISE/ACK then we need to fill in the src ttl field
		if (it->second.mAnnounceRate < 0)
			pData->set_srcttl(it->second.mResponseTtl);
		
		if (hasSubs)
			it->second.sendCountUni++;
	}
	else if (it->second.mAnnounceRate < 0)
	{
		// This app is NOT using ADVERTISE/ACK then we need to fill
		// in the srcttl
		pData->set_srcttl(it->second.mSrcttl);
		
		// If we aren't regenerating TTL then we need to fill in that field
		if (!(it->second.mRegenerateTtl))
			pData->set_nottlregen(true);
	}
	
	
	
	if(mDataFile != NULL) //DATAITEM
	{
		mDataProdDI++;
		char buf[256];
		uint64_t millis = duration_cast<milliseconds>(getTime()).count();
		int buflen = sprintf(buf,"0,%.0f,ll.gcnClientProdData,node%03d.gcnClient,%.0f,\"{\"\"gid\"\":%d,\"\"size\"\":%d,\"\"ttl\"\":%d,\"\"sent\"\":%d}\"\n",
			(double)mDataProdDI,mNodeId,(double)millis,pData->gid(),(int)pData->data().size(),pData->srcttl(),hasSubs);
		fwrite(buf,sizeof(char),buflen,mDataFile);
		fflush(mDataFile);
	}
	
	// Now check for subscribers and don't actually send the message unless we do have them
	if (!hasSubs)
		return false;
	
	// Send over TCP socket to the GCN
	uint32_t sizeSent = sendToGCN(message);
	
	// Check log level first because printing to string is expensive
	// and can affect throughput
	if (mCurrentLogLevel >= LOG_DEBUG)
	{ 
		string sMessage;
		mPbPrinter.PrintToString(message, &sMessage);
		LOG(LOG_DEBUG, "Sent Content (%d bytes):\n%s", sizeSent, sMessage.c_str());
	}

	it->second.sendCount++;
	return true;
}


//************************************************************************
// function to send a pull to the GCN to notify it of the groups
// we belong to
void gcnClient::sendPull(GroupId gid)
{
	// Send a pull
	// format App message and send out socket
	AppMessage message;

	// set the group id field
	auto pPull = message.add_pull();
	pPull->set_gid(gid);

	// Send over TCP socket to the GCN
	uint32_t sizeSent = sendToGCN(message);
	
	string sMessage;
	mPbPrinter.PrintToString(message, &sMessage);
	LOG(LOG_DEBUG, "Sent PULL (%d bytes):\n%s", sizeSent, sMessage.c_str());
}


//************************************************************************
// function to send a unpull to the GCN to notify it that we don't belong
// anymore to a gid
void gcnClient::sendUnpull(GroupId gid)
{
	// Send a pull
	// format App message and send out socket
	AppMessage message;

	// set the group id field
	auto pUnpull = message.add_unpull();
	pUnpull->set_gid(gid);

	// Send over TCP socket to the GCN
	uint32_t sizeSent = sendToGCN(message);
	
	string sMessage;
	mPbPrinter.PrintToString(message, &sMessage);
	LOG(LOG_DEBUG, "Sent UNPULL (%d bytes):\n%s", sizeSent, sMessage.c_str());
}


//************************************************************************
// function to send ADVERTISE to the GCN to notify it with the info needed
// to send announce
void gcnClient::sendAdvertise(GroupId gid, GCNMessage::AdvertiseType type)
{
	ClientIt it = mClientMap.find(gid);
	LOG_ASSERT(it != mClientMap.end(), "Could not find GID %d in client map", gid);
	
	// Create App message, fill in announce and send out socket
	AppMessage message;
	auto pAdvertise = message.add_advertise();
	pAdvertise->set_gid(it->first);
	pAdvertise->set_srcttl(it->second.mSrcttl);
	pAdvertise->set_type(type);
	if (it->second.mAnnounceRate >= 0)
	{
		// We are using acknowledgements for this app
		// so set the advertise rate and prob of relay
		pAdvertise->set_interval(it->second.mAnnounceRate);
		pAdvertise->set_probrelay(it->second.mAckProbRelay);
		
		// Since the GCN is responsible for sending the ADVERTISE
		// messages, we also need to let it know if we are not
		// regenerating TTL
		if (!(it->second.mRegenerateTtl))
			pAdvertise->set_nottlregen(true);
	}

	// Send to the GCN
	uint32_t sizeSent = sendToGCN(message);

	string sMessage;
	mPbPrinter.PrintToString(message, &sMessage);
	LOG(LOG_DEBUG, "Sent Advertise (%d bytes):\n%s", sizeSent, sMessage.c_str());
}


//************************************************************************
// function to process received messages
void gcnClient::OnRecvMessage(char* buffer, unsigned int len)
{

	AppMessage message;
	
	if(message.ParseFromArray(buffer, len))
	{
		// Check log level first because printing to string is expensive
		// and can affect throughput
		if (mCurrentLogLevel >= LOG_DEBUG)
		{ 
			string sMessage;
			mPbPrinter.PrintToString(message, &sMessage);
			LOG(LOG_DEBUG, "Received message (%d bytes):\n%s", message.ByteSize(), sMessage.c_str()); 
		}

		// handle any pulls that are in the message
		for ( auto & pull : *message.mutable_pull() )
		{
			LOG(LOG_DEBUG, "Received PULL for GID %d", pull.gid());
			// We have a subscriber!
			ClientIt it = mClientMap.find(pull.gid());
			LOG_ASSERT(it != mClientMap.end(), "Could not find GID %d in client map", pull.gid());
			it->second.mHasSubscribers = true;
		}
		
		// handle any unpulls that are in the message
		for ( auto & unpull : *message.mutable_unpull() )
		{
			LOG(LOG_DEBUG, "Received UNPULL for GID %d", unpull.gid());
			// We have no subscriber!
			ClientIt it = mClientMap.find(unpull.gid());
			LOG_ASSERT(it != mClientMap.end(), "Could not find GID %d in client map", unpull.gid());
			it->second.mHasSubscribers = false;
		}
		
		// handle any data pushes that we received
		// These are processed by the function that the application provided.
		for ( auto & data : *message.mutable_data() )
		{
			ClientIt it = mClientMap.find(data.gid());
			LOG_ASSERT(it != mClientMap.end(), "Could not find GID %d in client map", data.gid());
			
			if(mDataFile != NULL) //DATAITEM
			{
				mDataRcvDI++;
				char buf[256];
				uint64_t millis = duration_cast<milliseconds>(getTime()).count();
				int buflen = sprintf(buf,"0,%.0f,ll.gcnClientRcvData,node%03d.gcnClient,%.0f,\"{\"\"gid\"\":%d,\"\"srcnode\"\":\"\"node%03d\"\",\"\"size\"\":%d,\"\"seq\"\":%0.f,\"\"ttl\"\":%d,\"\"dist\"\":%d}\"\n",
					(double)mDataRcvDI,mNodeId,(double)millis,data.gid(),data.srcnode(),(int)data.data().size(),(double)data.sequence(),data.ttl(),data.distance());
				fwrite(buf,sizeof(char),buflen,mDataFile);
				fflush(mDataFile);
			}
			
			// Only increment count if told to do so
			if (it->second.mMsgHandler(data))
				it->second.recvCount++;
			
			// Is this application using ADVERTISE/ACK? We test that by whether or not
			// the data packet has the src ttl field. If it is using ADVERTISE/ACK then
			// it will not have this field
			bool usingAck = !(data.has_srcttl());
			
			if(data.has_uheader())
				it->second.recvCountUni++;
				
			if (it->second.mSendResponse)
			{
				// We are set to send unicast responses
				if (it->second.mType == 1)
				{
					// I am a source node set up to send unicast. That means that 
					// I should change my destination to be the source of this
					// unicast message we just received. In other words, we now
					// sent Unicast traffic instead of bcast traffic.
					it->second.mDest = data.srcnode();
				}
				else if (!(it->second.recvCount % it->second.mSendResponseFreq) )
				{
					// I am not a source node so I just send a response every nth packet.
					// Construct payload
					Buffer msgBuffer;
					char* tempMsg = msgBuffer.data();
					sprintf(tempMsg, "Response %d to node %d for GID %d", it->second.recvCount, data.srcnode(), data.gid());
				
					// Create App message, fill in push and send out socket
					AppMessage responseMsg;
					auto pData = responseMsg.add_data();
					pData->set_gid(it->first);
					pData->set_data(msgBuffer.data());
					// If we are not using ADVERTISE/ACK then we need to fill in the src ttl field
					if (!usingAck)
					{
						// If the user specified a value then the stored value is not zero
						if (it->second.mResponseTtl)
						{
							pData->set_srcttl(it->second.mResponseTtl);
						}
						else
						{
							// user did NOT specify a value so use the source ttl
							pData->set_srcttl(data.srcttl());
						}
					}
					
					//This is a unicast packet. Build the header and add to data message
					auto pHeader = pData->mutable_uheader();
					pHeader->set_unicastdest(data.srcnode());
					pHeader->set_resilience(it->second.mResilience);
					
					// Send over TCP socket to the GCN
					uint32_t sizeSent = sendToGCN(responseMsg);
		
					// Check log level first because printing to string is expensive
					// and can affect throughput
					if (mCurrentLogLevel >= LOG_DEBUG)
					{ 
						string sMessage2;
						mPbPrinter.PrintToString(responseMsg, &sMessage2);
						LOG(LOG_DEBUG, "Sent Response (%d bytes):\n%s", sizeSent, sMessage2.c_str());
					}
				
					it->second.sendCountUni++;
					it->second.sendCount++;
				}
			}
		}
	}
}


//************************************************************************
void gcnClient::OnStatTimeout()
{
	for (ClientIt it = mClientMap.begin(); it != mClientMap.end(); ++it)
	{
		LOG(LOG_FORCE,"GCN Client stats type: %s group %d: rcvd>%d sent>%d rerr>%d serr>%d rcvdUni>%d sentUni>%d", 
			AppTypeStr[it->second.mType], it->first, it->second.recvCount, it->second.sendCount, it->second.rerrCount, 
			it->second.serrCount, it->second.recvCountUni, it->second.sendCountUni);
	}
	
	// reschedule the periodic event
	if (mStatInterval > 0)
		{
			mStatTimer.expires_at(mStatTimer.expires_at() + Seconds(mStatInterval));
			mStatTimer.async_wait(boost::bind(&gcnClient::OnStatTimeout, this));
		}
}

//************************************************************************
void gcnClient::recvFromGCN()
{
	// Create buffer
	BufferPtr pBuffer(new Buffer);
	
	// First we read the size of the incoming message, then the message itself into the pBuffer
	async_read(mSocket, buffer(pBuffer->data(), mSizeOfSize),
		[this, pBuffer](error_code ec, size_t length)
		{
			if (!ec)
			{
				//for (unsigned int i = 0; i < length; i++)
				//{
				//	printf("0x%x  ", (unsigned char)(pBuffer->data())[i]);
				//}
				//printf("\n");
				
				// get the size 
				uint32_t messageSize{};
				memcpy(&messageSize, pBuffer->data(), length);
				messageSize = ntohl(messageSize);

				// read the message
				if ( (messageSize) && (messageSize <= MAX_BUFFER_SIZE - mSizeOfSize) )
				{
					async_read(mSocket, buffer(pBuffer->data() + mSizeOfSize, messageSize),
						[this, pBuffer](error_code ec, size_t length)
						{
							if (!ec)
							{
								// call function to process message
								OnRecvMessage(pBuffer->data() + mSizeOfSize, length);
								recvFromGCN();
							}
							else
							{
								LOG(LOG_ERROR, "Stopping GCN Client. async_receive of msg buffer had error %s\n", ec.message().c_str());
								Stop();
								exit(1);
							}
						});
				}
				else
				{
					LOG(LOG_ERROR, "recvFromGCN messageSize check failed. messageSize is %d\n", messageSize);
					recvFromGCN();
				}
			}
			else
			{
				LOG(LOG_ERROR, "Stopping GCN Client. async_receive of msg size buffer had error %s\n", ec.message().c_str());
				Stop();
				exit(1);
			}
		});
}


//************************************************************************
uint32_t gcnClient::sendToGCN(AppMessage & message)
{
	uint32_t size = message.ByteSize();
	uint32_t totalSize = size + mSizeOfSize;

	// Check the total size can fit in the buffer
	if ( totalSize > MAX_BUFFER_SIZE )
		{
			LOG(LOG_ERROR, "AppMessage too large");
			return 0;
		}
	
	// Buffer to hold the message to send
	BufferPtr pBuffer(new Buffer);

	// Copy the size of the AppMessage first
	uint32_t htnSize = htonl(size);
	memcpy(pBuffer->data(), &htnSize, mSizeOfSize);
	
	// Then serialize message
	message.SerializeToArray(pBuffer->data() + mSizeOfSize, size);

#if 0
	printf("Sending message\n");
	for (unsigned int i = 0; i < totalSize; i++)
	{
		printf("0x%x  ", (unsigned char)(pBuffer->data())[i]);
	}
	printf("\n");
#endif

	// Now send the message
	async_write(mSocket, buffer(*pBuffer, totalSize),
		    [this](error_code ec, size_t /*length*/)
		    {
			    if (ec)
				    {
					    LOG(LOG_ERROR, "Error sending to GCN");
				    }
		    });

	return(totalSize);
}


