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

#ifndef _GCN_CLIENT
#define _GCN_CLIENT

#include <boost/date_time/posix_time/posix_time.hpp>

#include "Common.h"
#include <google/protobuf/text_format.h>
#include "GCNMessage.pb.h"

using boost::asio::io_service;
using boost::asio::deadline_timer;

// This is the client shared library class.
//
// The shared libary provides the following:
//   - timer event for printing send and receive stats
//   - thread for receiving messages
//   - functionality for opening socket to the GCN
//   - handling of Advertise and Pull messages
//   - function to send content that can be called by client application
//        prototype is: bool sendMessage(GroupId gid, char* msgBuffer, NodeId dest = 0)
//        dest is an optional argument used to send unicast traffic
//
// The client application that uses the shared library is requred to do the following:
//   - have a main which reads input line from user and serves as execution context
//   - provide a function for processing received data messages
//        prototype is: function<void(Data & dataMsg)>
//   - manage the way in which content is sent (e.g., periodically) and
//     for building the actual content. 
//   - Uses the function provided by the client shared library to send the content.
//   - Call the client shared library start function
//   - Define signal handler and initialize signal handling in the main
//   - Provide a Stop function which calls the client shared library stop function
//     (in addition to whatever shut down the client app needs such as canceling
//       sending content)
//
// The client application does NOT need to know anything about how the data is sent
// OTA (for example that we are using google protocol buffers). It deals with simple
// char arrays both when sending and receiving.
//
// The client application does NOT send Advertise or Pull messages and does not process
// received pull messages. This is handled by the shared library. The client application
// only needs to handle the actual application content.

using namespace GCNMessage;

static const unsigned int	DEFAULT_PORTNUM = 12345;
static const string 			DEFAULT_SERVER_HOST{"127.0.0.1"};
static const unsigned int	DEFAULT_SRCTTL = 2;
static const double 			DEFAULT_PUSH_RATE = 1.0;
static const double 			DEFAULT_ANNOUNCE_RATE = 20.0;

// This is the number of characters needed in the message for timestamp.
// Timestamp is sent as microseconds so we need 16 characters in the string.
// 10 characters is the unix timestamp seconds and 6 is for the 6 decimal
// places needed for microseconds.
#define TIMESTAMP_SIZE 16


static const char __attribute__ ((used)) *AppTypeStr[] = { "Listener only", "Sender only", "Listener and Sender"};

// structure to hold config attributes
struct ClientConfig
{
	GroupId gid;
	int type;
	LogLevel logLevel;
	NodeId nodeId;
	unsigned int port;
	unsigned int srcttl;
	double announceRate;
	double pushRate;
	uint32_t stopCount;
	int stopTime;
	uint32_t msgSize;
	int sendResponse;
	int sendRespFreq;
	unsigned int respTtl;
	UnicastResilience  resilience;
	bool regenerateTtl;
	NodeId destNodeId;
	uint32_t ackProbRelay;
	string dataFile;
};

struct ClientGroupInfo
{
	int mType;
	unsigned int mSrcttl;
	double mAnnounceRate;
	uint32_t	mAckProbRelay;
	int mSendResponse;
	int mSendResponseFreq;
	unsigned int mResponseTtl;
	UnicastResilience  mResilience;
	bool mHasSubscribers;
	bool mRegenerateTtl;
	NodeId mDest;
	function<bool(Data & dataMsg)>	mMsgHandler;
	// Stats
	unsigned int    recvCount;  // count of DATA messages received; includes ALL message both bcast and unicast
	unsigned int    sendCount;  // count of DATA messages sent; includes ALL message both bcast and unicast
	unsigned int    rerrCount;  // receive error count
	unsigned int    serrCount;  // send error count
	unsigned int    recvCountUni;  // count of unicast DATA messages received; this is unicast only and is a subset of recvCount
	unsigned int    sendCountUni;  // count of unicast DATA messages sent; this is unicast only and is a subset of sentCount
};

typedef map<GroupId, ClientGroupInfo> ClientMap;
typedef pair<GroupId, ClientGroupInfo> ClientPair;
typedef map<GroupId, ClientGroupInfo>::iterator ClientIt;


#define LOG(level, ...)  do { if (level <= mCurrentLogLevel) writelog(level,  mCurrentLogLevel, mNodeId, __FILE__, __LINE__, __func__, __VA_ARGS__); } while (0)
#define LOG_ASSERT(condition, ...) do { if (!(condition)) writelog(LOG_FATAL, mCurrentLogLevel, mNodeId, __FILE__, __LINE__, __func__, __VA_ARGS__); } while (0)


class gcnClient
{
	public:
		gcnClient(io_service * io_serv);
		~gcnClient();
		bool Start(const ClientConfig &config, function<bool(Data & dataMsg)> procFunc);
		void Stop();
		bool sendMessage(GroupId gid, char* msgBuffer, NodeId dest = 0);
		
		google::protobuf::TextFormat::Printer mPbPrinter;

	private:
		void OnRecvMessage(char* buffer, unsigned int len);
		void OnStatTimeout();
		uint32_t sendToGCN(AppMessage & message);
		void recvFromGCN();

		void sendPull(GroupId gid);
		void sendUnpull(GroupId gid);
		void sendAdvertise(GroupId gid, GCNMessage::AdvertiseType type);

		NodeId mNodeId;
		LogLevel mCurrentLogLevel;
		unsigned int mPort;
		ClientMap mClientMap;

		string mDataFilePath;
		FILE* mDataFile;

		tcp::resolver mResolver;
		tcp::socket mSocket;
		bool  mSocketConnected;

		deadline_timer mStatTimer;

		double mStatInterval;
		unsigned int mDataProdDI;
		unsigned int mDataRcvDI;
		
		// This is used to send the "size" of a message first on the google proto buffer
		size_t    mSizeOfSize;



}; // end class app_stub

#endif // _GCN_CLIENT
