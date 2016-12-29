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


#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <set>
#include <list>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <cstring>
#include <stdarg.h> 
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <getopt.h>
//#include <sys/un.h> // unnecessary?

// Linux specific includes
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <unistd.h>
#include <pcap.h>
#include <sys/socket.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

// pull in the items from std that we are using
using std::atoi;
using std::array;
using std::bind;
using std::cerr;
using std::cout;
using std::endl;
using std::exception;
using std::fixed;
using std::function;
using std::hash;
using std::lock_guard;
using std::make_shared;
using std::map;
using std::move;
using std::multimap;
using std::mutex;
using std::pair;
using std::setprecision;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unordered_map;
using std::unordered_set;
using std::set;
using std::vector;
using std::static_pointer_cast;
using namespace std::chrono;

// pull in the boost items
using boost::asio::ip::tcp;
using boost::asio::connect;
using boost::asio::write;
using boost::asio::read;
using boost::asio::buffer;
using boost::asio::io_service;
using boost::asio::deadline_timer;
using boost::system::error_code;
using boost::system::system_error;
using boost::split;
using boost::is_any_of;
using boost::token_compress_on;
using namespace boost::asio::error;
using boost::asio::posix::stream_descriptor;
using boost::asio::async_write;
using boost::asio::async_read;

static constexpr uint32_t ETH_P_GCN_CTRL = 0x88b5; // custom GCN EtherType for control packets
static constexpr uint32_t ETH_P_GCN_DATA = 0x88b6; // custom GCN EtherType for data packets

typedef boost::posix_time::seconds Seconds;
typedef boost::posix_time::milliseconds Milliseconds;
typedef boost::posix_time::microseconds Microseconds;

// Time is based on system clock
typedef system_clock::duration TimeDuration;
typedef uint32_t  GroupId;
typedef uint32_t  NodeId;
typedef size_t     HashValue;

// GCN key.
// ues Group Id and Group source node to identify a traffic flow
class GIDKey {
  public:
	GroupId	gid;
	NodeId	gidSrc;
    
	GIDKey(GroupId k1, NodeId k2)
		: gid(k1), gidSrc(k2) {}  

	bool operator<(const GIDKey &right) const 
	{
		if (gid == right.gid)
		{
			return (gidSrc < right.gidSrc);
		}
		else 
		{
			return (gid < right.gid);
		}
	} 
};


// Logging items
enum LogLevel
{
	LOG_INVALID = 0,
	LOG_FATAL,
	LOG_FORCE,
	LOG_ERROR,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG,
	LOG_TRACE,
	LOG_INVALID_MAX
};

// globals used for logging
// log uses mutex to prevent interruption when writing
extern mutex gLogMutex;
static const char __attribute__ ((used)) *LogLevelStr[] = { "", "FATAL", "FORCE", "ERROR", "WARNING", "INFO", "DEBUG", "TRACE"};


// global constants
static const unsigned int	MAX_BUFFER_SIZE = 8192;
static const LogLevel		DEFAULT_LOG_LEVEL = LOG_ERROR;

// buffer typedefs
typedef array<char, MAX_BUFFER_SIZE> Buffer;
typedef shared_ptr<Buffer> BufferPtr;

// Miscellaneous utility functions
TimeDuration getTime(); // returns current time as a duration

// Logging function
void writelog(LogLevel level, LogLevel myLevel, NodeId mId, string file, int line, string func, const char * fmt, ...);


#define MAX_MCAST_HEADER_GROUP_ID 16777216

// Whether Ethernet headers are to be used OTA
// NOTE: NS3/DCE has a bug in which it always adds
// the Ethernet header (even for a SOCK_RAW) and 
// the header overwrites the first 14 bytes of user data!
// Work around is to force the use of the Ethernet header here
// and just allow DCE to overwrite it.
#ifdef NS3
static const bool USE_ETHERNET_HEADERS = true;  // MUST be true for NS3
#else
static const bool USE_ETHERNET_HEADERS = true;
#endif

class OTASession
{
 public:
 OTASession(bool mcastEthernetHeader) : mMcastEthernetHeader(mcastEthernetHeader){}
	void open(io_service & io, vector<string> & devices);
	void read(function<void(char* buffer, int length)> & receiveHandler);
	void write(GroupId gid, BufferPtr pBuffer, int length, bool ctrlPkt, const unsigned char* destHwAddress = NULL);
	void close();
 private:

	bool mMcastEthernetHeader;

#ifdef NS3
	struct RawSocket
	{
		int fd;
		shared_ptr<stream_descriptor> mSocket;
		char hwAddress[ETH_ALEN];
		uint32_t etherType;
	};
	multimap<int, RawSocket> mRawSockets;

	void handle_read(const error_code& ec,
				size_t,
				int device,
				int fd,
				shared_ptr<stream_descriptor> socket,
				function<void(char* buffer, int length)> & receiveHandler);
#else
	struct PcapSocket
	{
		pcap_t* mPcapHandle;
		shared_ptr<stream_descriptor> mSocket;
		char hwAddress[ETH_ALEN];
	};
	map<string, PcapSocket> mPcapSockets;

	void handle_read(const error_code& error, 
			 size_t, 
			 pcap_t *pPcapHandle,
			 shared_ptr<stream_descriptor> socket,
			 function<void(char* buffer, int length)> & receiveHandler);

	bool getEthernetAddress(string ifname, char* hwAddress);
#endif

	void prependEthernetHeader(GroupId gid, const char* hwAddress, BufferPtr pBuffer, bool ctrlPkt, const unsigned char* destHwAddress = NULL);
};


#endif //COMMON_H
