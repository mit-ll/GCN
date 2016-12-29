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

#ifndef _GCN_SERVICE_HEADER
#define _GCN_SERVICE_HEADER


#include "Common.h"
#include <google/protobuf/text_format.h>
#include "GCNMessage.pb.h"

#include <boost/date_time/posix_time/posix_time.hpp>

using namespace GCNMessage;


#define LOG(level, ...)  do { if (level <= mCurrentLogLevel) writelog(level,  mCurrentLogLevel, mNodeId, __FILE__, __LINE__, __func__, __VA_ARGS__); } while (0)
#define LOG_ASSERT(condition, ...) do { if (!(condition)) writelog(LOG_FATAL, mCurrentLogLevel, mNodeId, __FILE__, __LINE__, __func__, __VA_ARGS__); } while (0)

// Default values for config attributes
static const double DEFAULT_HASHEXPIRE = 30.0;
static const double DEFAULT_HASHINTERVAL = 10000.0;
static const double DEFAULT_PULLEXPIRE = 3600.0; // Set these very high for now so that nothing expires
static const double DEFAULT_PULLINTERVAL = 5000.0;
static const double DEFAULT_REVPATHEXPIRE = 3600.0;  // Set these very high for now so that nothing expires
static const double DEFAULT_REVPATHINTERVAL = 10000.0;


// structure to hold config attributes
struct GcnServiceConfig
{
	LogLevel logLevel;
	NodeId nodeId;
	vector<string> devices;
	double hashExpire;
	double hashInterval;
	double pullExpire;
	double pullInterval;
	double pathExpire;
	double pathInterval;
	bool mcastEthernetHeader;
	bool alwaysRebroadcast;
	uint32_t ackProbRelay;
	string dataFile;
};

class ClientSession;

// Typdefs for the LOCAL Pull maps; this relates gid to socket
//  (i.e., it is a map of subscribing sockets keyed on gid)
// This is a multimap because we can have more than one attached client
// subcribing to the same gid
// Key: group id
// Mapped value: socket
typedef multimap<GroupId, shared_ptr<ClientSession>>  LocalPullMMap;
typedef pair<GroupId, shared_ptr<ClientSession>>      LocalPullPair;
typedef multimap<GroupId, shared_ptr<ClientSession>>::iterator  LocalPullIt;
typedef pair <multimap<GroupId, shared_ptr<ClientSession>>::iterator, multimap<GroupId, shared_ptr<ClientSession>>::iterator> LocalPullRangeIt;

// Typdefs for the REMOTE Pull maps; this relates gid to Node Id
// This is a multimap because we can have more than one node subscribing
// to the content (gid) that we provide
// Key: group id
// Mapped value: node id and timestamp
struct RemotePullInfo
{
	NodeId	nodeId;
	int		timestamp;
};
typedef multimap<GroupId, RemotePullInfo>  RemotePullMMap;
typedef pair<GroupId, RemotePullInfo>      RemotePullPair;
typedef multimap<GroupId, RemotePullInfo>::iterator  RemotePullIt;
typedef pair <multimap<GroupId, RemotePullInfo>::iterator, multimap<GroupId, RemotePullInfo>::iterator> RemotePullRangeIt;


// typedefs for Reverse Path Map
// Key: group id and GID source node
// Mapped value: previous source node and timestamp
// When receiving an announce response OTA, store the group and the src of the ADVERTISE
// so we have a reverse path back to the source for the data
// We also timestamp when announce is received so old entries can be cleaned up
struct RevPathInfo
{
	NodeId		srcNode;
	uint32_t		seqNum;
	int			timestamp;
	uint32_t		probRelay;
};
typedef map<GIDKey, RevPathInfo> ReservePathMap;
typedef pair<GIDKey, RevPathInfo> ReservePathPair;
typedef map<GIDKey, RevPathInfo>::iterator ReservePathIt;


// typedefs for Sequence Map
// Key: group id and GID source node
// Mapped value: sequence number
// This stores a sequence number by gid,gid src.
// It is used in several maps related to the processing of ACKs
typedef map<GIDKey, uint32_t> SequenceMap;
typedef pair<GIDKey, uint32_t> SequencePair;
typedef map<GIDKey, uint32_t>::iterator SequenceIt;

// typedefs for Ack Timer Map
// Key: group id and GID source node
// Mapped value: shared pointer to a deadline timer
// When a node needs to send an ACK, it waits for a small period
// of time before doing so. The deadline timer is stored in the map. 
// When a node needs to send an ACK it always checks the map first
// to see if it already has a timer set to send an ACK for the GID/GID source.
typedef shared_ptr<deadline_timer> AckTimer;
typedef map<GIDKey, AckTimer> AckTimerMap;
typedef pair<GIDKey, AckTimer> AckTimerPair;

// typedefs for Advertise Timer Map
// Key: group id and GID source node
// Mapped value: shared pointer to a deadline timer
// When a node needs to send an Advertise, it waits for a small period
// of time before doing so. The deadline timer is stored in the map. 
// Unlike when the ack timer, a node does not need to check the map first
// to see if it already has a timer set to send an Advertise.
// However, we keep the map of deadline timers so that once created, it
// has a reference and this is not destroyed. When destroyed the timer is
// cancelled so if we didn't keep the timer, it would get canceled immediately.
// Also, cant just have a private because we could have more than one advertise
// outstanding at any given time
typedef shared_ptr<deadline_timer> AdvTimer;
typedef map<GIDKey, AdvTimer> AdvTimerMap;
typedef pair<GIDKey, AdvTimer> AdvTimerPair;

// typedefs for Data Timer Map
// Key: hash value
// Mapped value: shared pointer to a deadline timer
// When a node needs to forward a data msg, it waits for a small period
// of time before doing so. The deadline timer is stored in the map. 
// We keep the map of deadline timers so that once created, it
// has a reference and this is not destroyed. When destroyed the timer is
// cancelled so if we didn't keep the timer, it would get canceled immediately.
// Also, cant just have a private because we could have more than one advertise
// outstanding at any given time
typedef shared_ptr<deadline_timer> DataTimer;
typedef map<HashValue, DataTimer> DataTimerMap;
typedef pair<HashValue, DataTimer> DataTimerPair;

// typedefs for Distance Map
// Key: group id and GID source node
// Mapped value: distance info
// This map is used to track the distance (# hops) to a GID source node
// as well as how many times we have seen the latest packet from the GID source node.
// When we receive a ADVERTISE or DATA message, the distance is updated and the hash value
// is compared to the current value in the map. If the hash matches then this is a duplicate
// and increment the count. If it does not match then this a new "latest packet" from 
// the GID source
struct DistanceInfo
{
	uint32_t	distance;
	size_t		latestPacketHash;
	uint16_t	packetCount;
	unordered_set<NodeId>	packetSrcs;
};
typedef map<GIDKey, DistanceInfo> DistanceMap;
typedef pair<GIDKey, DistanceInfo> DistancePair;
typedef map<GIDKey, DistanceInfo>::iterator DistanceIt;


// Typdefs for the Announc map; this relates gid to AnnounceInfo
// This is a map because there can only be one provider of gid content
// Key: group id
// Mapped value: announce info
struct AnnounceInfo
{
	shared_ptr<ClientSession>	pSession;
	shared_ptr<deadline_timer>	pTimer;
	double							interval;
	uint32_t						probRelay;
	uint32_t						srcTtl;
	uint32_t						seqNum;
	bool								pullSentToApp;
	bool								noTtlRegen;
};
typedef map<GroupId, AnnounceInfo>  AnnounceMap;
typedef pair<GroupId, AnnounceInfo> AnnouncePair;
typedef map<GroupId, AnnounceInfo>::iterator  AnnounceIt;

// Typdefs for the hash maps
// Map 1:
// key is the hash value 
// mapped value is the max ttl which is used by non-Group nodes (i.e., nodes with no subscribers to the group)
// This map is used to store the hash values so we can search to see if we have seen a packet or not
// and also to get the maxTTL of packets we have seen.
// using an unordered_map because it is more efficient for finds than a map
typedef unordered_map<HashValue, uint32_t>  HashMap;
typedef pair<HashValue, uint32_t> HashPair;
typedef unordered_map<HashValue, uint32_t>::iterator  HashIt;

// Map 2:
// key is the time at which the hash value was inserted into the HashMap
// mapped value is the hash value
// This map is used to store the timestamp of the hash values so we can clear out old entries
// after some period of time. This map is scanned for old records and then the mapped value
// is used to access the hash value in the HashMap to delete it from there.
// using a map so we can order by increasing time stamp
typedef multimap<double, HashValue> HashTimeMap;
typedef pair<double, HashValue> HashTimePair;
typedef multimap<double, HashValue>::iterator HashTimeIt;

// Key used for the set that holds advertisements
// that the node has seen
class AdvKey {
  public:
	GroupId	gid;
	NodeId	gidSrc;
	uint32_t	seq;
    
	AdvKey(GroupId k1, NodeId k2, uint32_t k3)
		: gid(k1), gidSrc(k2), seq(k3) {}  

	bool operator<(const AdvKey &right) const 
	{
		if ( gid == right.gid ) 
		{
			if ( gidSrc == right.gidSrc ) 
			{
				return seq < right.seq;
			}
			else 
			{
				return gidSrc < right.gidSrc;
			}
		}
		else 
		{
			return gid < right.gid;
		}
	}    
};



class ClientSession
: public std::enable_shared_from_this<ClientSession>
{
 public:
 ClientSession(tcp::socket socket)
	 : mSocket(std::move(socket)), mSizeOfSize(sizeof(uint32_t))
		{}
	
	void read(function<void(shared_ptr<ClientSession>, char* buffer, int length)> & receiveHandler,
		  function<void(shared_ptr<ClientSession>)> & closeHandler);
	void write(BufferPtr pBuffer, int length);
	void close()
	{
		mSocket.close();
	}
 private:
	tcp::socket mSocket;
	size_t mSizeOfSize;
};


class GcnService
{
	public:
		GcnService(const GcnServiceConfig &gcnConfig, io_service * io_serv);
		~GcnService();
		void Start();
		void Stop();
		void Run();

		void hashCleanup();
		void reversePathCleanup();
		void remotePullCleanup();
		void OnAnnounceTimeout(const error_code & ec, shared_ptr<void> arg);
		
		void acceptClientConnections();
		
		// coinflip function
		bool coinFlip(uint32_t prob);
		
		// common processing for messages
		bool preProcessData(Data & dataMsg, HashValue & hashValue, NodeId msgOtaSrc, shared_ptr<ClientSession> pSession = nullptr);
		
		// message receive functions which must be implemented
		void OnNetworkReceive(char* buffer, int len);
		void OnClientReceive(shared_ptr<ClientSession> pSession, char* buffer, int len);
		
		// message processing functions
		void processNetworkData(Data & dataMsg, NodeId msgOtaSrc);
		void processNetworkAdvertise(Advertise & advertiseMsg, NodeId msgOtaSrc);
		void processNetworkAck(Ack& ackMsg, NodeId msgOtaSrc);
		 
		// message forwarding
		void forwardToApp(Data & dataMsg,     shared_ptr<ClientSession> pSession);
		void forwardToApp(Pull & pullMsg,     shared_ptr<ClientSession> pSession);
		void forwardToApp(Unpull & unpullMsg, shared_ptr<ClientSession> pSession);
		void forwardToApp(Advertise & advMsg, shared_ptr<ClientSession> pSession);
		void forwardToApp(AppMessage & Msg,   shared_ptr<ClientSession> pSession);
		
		void forwardToOTA(Data & dataMsg,     uint32_t ttl);
		void forwardToOTA(Advertise & advMsg, uint32_t ttl);
		void forwardToOTA(Ack & ackMsg);
		void forwardToOTA(GroupId gid, OTAMessage & Msg);
		
		// Functions for ACK timers
		void setAckTimer(Ack & ackMsg);
		void OnAckTimeout(const error_code & ec, Ack & ackMsg);
		
		// Functions for Advertise timers
		void setAdvTimer(Advertise & advertiseMsg, uint32_t ttl);
		void OnAdvTimeout(const error_code & ec, Advertise & advertiseMsg, uint32_t ttl);
		
		// Functions for Data timers
		void setDataTimer(Data & dataMsg, uint32_t ttl, HashValue hashVal);
		void OnDataTimeout(const error_code & ec, Data & dataMsg, uint32_t ttl, HashValue hashVal);
		
		// Hash items
		bool addToHash(Data & dataMsg, HashValue & hashValue);
		bool addToHash(Advertise & advMsg, HashValue & hashValue);
		bool addToHash(string data, HashValue & hashValue, uint32_t ttl);
		uint32_t getMaxTTLfromHash(HashValue hashValue);
		void changeMaxTTL(HashValue hashValue, uint32_t ttl);
		void updateDistanceTable(GroupId gid, NodeId gidsrc, HashValue hashValue, uint32_t distance, NodeId otaSrc, bool newToHash, bool AdvMsg);
		
		void closeClientConnection(shared_ptr<ClientSession> pSession);
		

		//io_service mIoService;
		io_service* pIoService;

		tcp::acceptor mClientAcceptor;
		tcp::socket mClientSocket;
		
		OTASession mOTASession;

		vector<string> 	mDevices;
		 
		google::protobuf::TextFormat::Printer mPbPrinter;
		
		string mDataFilePath;
		FILE* mDataFile;
		
		// pull maps
		LocalPullMMap	mLocalPullTable;
		RemotePullMMap	mRemotePullTable;
		
		AnnounceMap		mAnnounceTable; 
		ReservePathMap	mReversePathTable;
		SequenceMap		mCoinFlipTable;
		SequenceMap		mAckSentTable;
		DistanceMap		mDistanceTable;
		AckTimerMap		mAckTimerTable;
		AdvTimerMap		mAdvTimerTable;
		DataTimerMap	mDataTimerTable;
		
		set<AdvKey>		mAdvSeenSet;
		
		// hash tables
		hash<string>	make_hash;
		HashMap			mHashTable;
		HashTimeMap		mHashTimeTable;
		
		
		NodeId		mNodeId;
		LogLevel		mCurrentLogLevel;
		double		mHashExpireTime;
		double		mHashCleanupInterval;
		double		mReversePathExpireTime;
		double		mReversePathCleanupInterval;
		double		mRemotePullExpireTime;
		double		mRemotePullCleanupInterval;
		bool			mAlwaysRebroadcast;
		double		mStatInterval;

		deadline_timer mHashCleanupTimer;
		deadline_timer mRemotePullCleanupTimer;
		deadline_timer mReversePathCleanupTimer;
		deadline_timer mStatTimer;
		
		void OnStatTimeout();

		int	clientCount;
		
		// Stats
		unsigned int		recvCountAdv;			// number of Advertise packets received OTA
		unsigned int		recvCountAck;			// number of Ack packets received OTA
		unsigned int		recvCountData;			// number of Data packets (non unicast) received OTA
		unsigned int		recvCountDataUni;		// number of Unicast Data packets received OTA
		unsigned int		dropCount;				// number of packets that we received OTA and dropped because we have seen them already or TTL is 0
		unsigned int		pushCount;				// number of packets that we received OTA and pushed to Client
		unsigned int		fwdCount;				// number of packets that we received OTA and forward back out OTA
		unsigned int		fwdCountUni;			// number of unicast packets that we received OTA and forward back out OTA
		unsigned int		clientRcvCount;		// number of packets that we received from the client to send
		unsigned int		sentCount;  			// number of packets that we received from the client that we sent OTA
		unsigned int		relayDataGroup;		// Flag to indicate if this node relayed a data packet as a group node in the last stat interval
		unsigned int		relayDataNonGroup;	// Flag to indicate if this node relayed a data packet as a non-group node in the last stat interval
		unsigned int		nonGroupRcvAck;		// number of non-Group nodes that receive an ACK
		unsigned int		nonGroupRcvAdv;		// number of non-Group nodes that receive an ADVERTISE
		unsigned int		totalBytesSentCtl;	// number of bytes sent for control messages (ADVERTISE and ACK)
		unsigned int		totalPacketsSentCtl;	// number of packets sent for control messages (ADVERTISE and ACK)
		unsigned int		totalBytesSentData;	// number of bytes sent for data messages (DATA)
		unsigned int		totalPacketsSentData;	// number of packets sent for data messages (DATA)
		uint64_t		mSentDataDI;		// number of sent message data items made so far
		uint64_t		mSentAdvDI;		// number of sent message advertise items made so far
		uint64_t		mSentAckDI;		// number of sent message ack items made so far
		uint64_t		mRcvDataDI;		// number of received message data items made so far
		uint64_t		mRcvAdvDI;		// number of received message advertise items made so far
		uint64_t		mRcvAckDI;		// number of received message ack items made so far
		uint64_t		mLocalPullDI;		// number of local pull items made so far
		uint64_t		mLocalUnpullDI;		// number of local unpull items made so far
		map<unsigned int,unsigned long>	mSeqNumByGID;
		
		size_t mSizeOfSize;

		function<void(char* buffer, int length)> mOTAReceiveHandler;
		function<void(shared_ptr<ClientSession>, char* buffer, int length)> mClientReceiveHandler;
		function<void(shared_ptr<ClientSession>)> mClientCloseHandler;
}; // end class

#endif // _GCN_SERVICE_HEADER
