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

#include "Common.h"

mutex gLogMutex;


TimeDuration getTime()
{ 
	return(system_clock::now().time_since_epoch()); 
}


void writelog(LogLevel level, LogLevel myLevel, NodeId mId, string file, int line, string funcName, const char * fmt, ...)
{
	char buffer[1024];
	vector<string> vec;
	va_list args;

	if (level > myLevel)
		return;

	// Get arguments from input
	va_start(args, fmt);
	vsnprintf(buffer, 1024, fmt, args);
	va_end(args);

	// Remove directory if present.
	const size_t last_slash_idx = file.find_last_of("\\/");
	if (string::npos != last_slash_idx)
	{
		file.erase(0, last_slash_idx + 1);
	}

	TimeDuration currTime = getTime();
	auto micro = duration_cast<microseconds>(currTime).count();
	auto timestamp = (double)micro/1000000.0;

	// grab mutex, write output and release mutex
	gLogMutex.lock();
	//cout << fixed << setprecision(6) << timestamp << fixed << " [Node " << mId << "][" << LogLevelStr[level] << "][" << file << ":" << line << "][" << funcName << "] "<< buffer << "\n";
	printf("%.6lf[Node %d][%s][%s:%d][%s] %s\n", timestamp, mId, LogLevelStr[level], file.c_str(), line, funcName.c_str(), buffer);
	gLogMutex.unlock();

	if (level == LOG_FATAL)
	{
		// Also print to stderr so user can parse that if they want to
		fprintf(stderr,"%.6lf[Node %d][%s][%s:%d][%s] %s\n", timestamp, mId, LogLevelStr[level], file.c_str(), line, funcName.c_str(), buffer);
		exit(1);
	}

}




#ifdef NS3
void OTASession::open(io_service & io, vector<string> & devices)
{
	// Loop over all devices specified and open device
	// For NS3, list of devices is a list of integers (stored as strings)
	for (auto it = devices.begin(); it != devices.end(); ++it)
	{
		// open raw socket for ctrl
		int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_GCN_CTRL));
		if (fd == -1)
		{
			printf("ERROR: Couldn't open device %s\n", it->c_str());
		}
		else
		{
			shared_ptr<stream_descriptor> pSocket = make_shared<stream_descriptor>(stream_descriptor(io));
			pSocket->assign(::dup(fd));

			// Create entry for map and insert it
			RawSocket rawSocket;
			rawSocket.fd = fd;
			rawSocket.mSocket = pSocket;
			rawSocket.etherType = ETH_P_GCN_CTRL;
			mRawSockets.insert(pair<int, RawSocket>(atoi(it->c_str()), rawSocket));

			printf("Opened socket 0x%x on device %s\n", rawSocket.etherType, it->c_str());
		}
		
		// open raw socket for DATA
		fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_GCN_DATA));
		if (fd == -1)
		{
			printf("ERROR: Couldn't open device %s\n", it->c_str());
		}
		else
		{
			shared_ptr<stream_descriptor> pSocket = make_shared<stream_descriptor>(stream_descriptor(io));
			pSocket->assign(::dup(fd));

			// Create entry for map and insert it
			RawSocket rawSocket;
			rawSocket.fd = fd;
			rawSocket.mSocket = pSocket;
			rawSocket.etherType = ETH_P_GCN_DATA;
			mRawSockets.insert(pair<int, RawSocket>(atoi(it->c_str()), rawSocket));

			printf("Opened socket 0x%x on device %s\n", rawSocket.etherType, it->c_str());
		}
	}
}

void OTASession::read(function<void(char* buffer, int length)> & receiveHandler)
{
	for (auto it = mRawSockets.begin(); it != mRawSockets.end(); ++it)
	{
		it->second.mSocket->async_read_some(boost::asio::null_buffers(),
				bind(&OTASession::handle_read,
					 this,
					 boost::asio::placeholders::error,
					 boost::asio::placeholders::bytes_transferred,
					 it->first,
					 it->second.fd,
					 it->second.mSocket,
					 receiveHandler));
	}
}


void OTASession::handle_read(const error_code& ec,
			     size_t,
			     int device,
			     int fd,
			     shared_ptr<stream_descriptor> socket,
			     function<void(char* buffer, int length)> & receiveHandler)
{
	if (ec)
	{
		fprintf(stderr, "OTASession::handle_read error!");
		return;
	}

	BufferPtr pMsgBuffer(new Buffer);
	struct sockaddr_ll from;
	socklen_t l =  sizeof(from);

	int length = recvfrom(fd, pMsgBuffer->data(), ETH_FRAME_LEN, 0, (sockaddr*) &from, &l);
	
#if 0
	printf("OTASession::handle_read recvfrom received size %d  from device %d\n", length, device);
	for (int i = 0; i < length; i++)
	{
		printf("0x%x  ", (unsigned char)(pMsgBuffer->data())[i]);
	}
	printf("\n");
#endif
	
	// Strip ethernet header
	if (USE_ETHERNET_HEADERS)
	{
		// Strip ethernet header
		length -= sizeof(struct ether_header);
		receiveHandler(pMsgBuffer->data() + sizeof(struct ether_header), length);
	}
	else
	{
		receiveHandler(pMsgBuffer->data(), length);
	}
	
	// read again
	socket->async_read_some(boost::asio::null_buffers(),
					bind(&OTASession::handle_read,
								 this,
								 boost::asio::placeholders::error,
								 boost::asio::placeholders::bytes_transferred,
								 device,
								 fd,
								 socket,
								 receiveHandler));

}

void OTASession::write(GroupId gid, BufferPtr pBuffer, int length, bool ctrlPkt, const unsigned char* destHwAddress)
{
	const unsigned char ether_broadcast_addr[]={0xff,0xff,0xff,0xff,0xff,0xff};

	for (auto it = mRawSockets.begin(); it != mRawSockets.end(); ++it)
	{
		// only send data to DATA and ctrl to CTRL
		if (  (ctrlPkt && (it->second.etherType == ETH_P_GCN_DATA))
				||
				(!ctrlPkt && (it->second.etherType == ETH_P_GCN_CTRL)) )
		{
			//printf("NOT sending ctrlPkt type %d on socket with ether type of 0x%x \n", ctrlPkt, it->second.etherType);
			continue;
		}
		else
		{
			//printf("sending ctrlPkt type %d on socket with ether type of 0x%x \n", ctrlPkt, it->second.etherType);
		}
		
		
		if (USE_ETHERNET_HEADERS)
			prependEthernetHeader(gid, it->second.hwAddress, pBuffer, ctrlPkt);

		struct sockaddr_ll addr={0};
		addr.sll_family=AF_PACKET;
		addr.sll_ifindex=it->first;
		addr.sll_halen=ETHER_ADDR_LEN;
		if (ctrlPkt)
			addr.sll_protocol=htons(ETH_P_GCN_CTRL);
		else
			addr.sll_protocol=htons(ETH_P_GCN_DATA);
			
		if ( mMcastEthernetHeader )
		{
			if ( gid > MAX_MCAST_HEADER_GROUP_ID )
			{
				gid = gid % MAX_MCAST_HEADER_GROUP_ID;
			}

			addr.sll_addr[0] = 0x01;
			addr.sll_addr[1] = 0x00;
			addr.sll_addr[2] = 0x05;
			
			// (Destination set to multicast address, 01:00:05:XX:XX:XX, 
			// where XX:XX:XX represent the group Id. For example group id 1 is
			// destination address of 01:00:05:01:00:00
			memcpy(&addr.sll_addr[3], &gid, ETHER_ADDR_LEN/2);
		}
		else
		{
			// (Destination set to broadcast address, FF:FF:FF:FF:FF:FF.)
			memcpy(addr.sll_addr,ether_broadcast_addr,ETHER_ADDR_LEN);
		}
		
#if 0
		printf("OTASession::write sending packet of length: %d on device %d EtherType 0x%x\n", length, it->first, it->second.etherType);
		for (int i = 0; i < length; i++)
		{
			printf("0x%x  ", (unsigned char)(pBuffer->data())[i]);
		}
		printf("\n");
#endif

		int send_result = 0;
		send_result = sendto(it->second.fd, pBuffer->data(), length, 0,
		      (struct sockaddr*)&addr, sizeof(addr));
		if (send_result == -1)
		{
			printf("ERROR: sendto failed\n");
		}
	}
}

void OTASession::close()
{
	for (auto it = mRawSockets.begin(); it != mRawSockets.end(); ++it)
	{
		::close(it->second.fd);
	}
	mRawSockets.clear();
}

#else // other than NS3

void OTASession::open(io_service & io, vector<string> & devices)
{
	char errbuf[PCAP_ERRBUF_SIZE];
	
	// If devices not specified, find a device (should be eth0)
	if ( devices.empty() )
		{
			char *dev = pcap_lookupdev(errbuf);
			if (dev == NULL) 
				{
					printf("ERROR: Couldn't find default device: %s\n", errbuf);
				}
			devices.push_back(string(dev));
		}
	
	for (auto it = devices.begin(); it != devices.end(); ++it)
	{
		// IMPORTANT NOTE!!!
		// we used to just use pcap_open_live to create the pcap handle BUT when upgrading
		// to Ubuntu 14.04, there was an issue with libpcap that causes packets to not
		// be received. See https://bugs.launchpad.net/ubuntu/+source/linux/+bug/1457472
		// The work around is to turn on immediate_mode. 
		// See https://github.com/teclo/flow-disruptor/commit/3376d049fc5280aedccc42fc6e09
		// Thanks to Tom Goff for finding the issue and the workaround!
		
		// This is old line of code using pcap_open_live
		// Maybe if we upgrade to another version of Ubuntu, the bug might be fixed and
		// we can just go back to this nice 1 line that replaces about 20 lines below.
		//pcap_t *pcapHandle = pcap_open_live(it->c_str(),BUFSIZ,1,0,errbuf);
		
		int err;
		pcap_t *pcapHandle = pcap_create (it->c_str(), errbuf);
		
		if (pcapHandle == NULL) 
		{
			printf("ERROR: Couldn't create pcap handle for device %s: %s\n", it->c_str(), errbuf);
		}
		else
		{
			do
			{
				err = pcap_set_snaplen (pcapHandle, BUFSIZ);
				if (err)
					break;
				
				err = pcap_set_promisc (pcapHandle, 1);
				if (err)
					break;
				
				err = pcap_set_timeout (pcapHandle, 0);
				if (err)
					break;
				
				// Here it is! The magic line that makes it all work in Ubuntu 14.04
				err = pcap_set_immediate_mode (pcapHandle, 1);
				if (err)
					break;
				
				err = pcap_activate (pcapHandle);
				if (err)
					break;
			} while (0);
			
			
			// Did it all work? If not write error message and close the pcap handle
			if (err)
			{
				if (err == PCAP_ERROR)
				{
					printf("ERROR: PCAP_ERROR for device %s: %s\n", it->c_str(), pcap_geterr (pcapHandle));
				}
				else if (err == PCAP_ERROR_NO_SUCH_DEVICE ||
							err == PCAP_ERROR_PERM_DENIED ||
							err == PCAP_ERROR_PROMISC_PERM_DENIED)
				{
					printf("ERROR: PCAP_ERROR for device %s: %s (%s)\n", it->c_str(), pcap_statustostr (err), pcap_geterr (pcapHandle));
				}
				else
				{
					printf("ERROR: PCAP_ERROR for device %s: %s\n", it->c_str(), pcap_statustostr (err));
				}
			
				pcap_close (pcapHandle);
			}
			else
			{
				shared_ptr<stream_descriptor> pSocket = make_shared<stream_descriptor>(stream_descriptor(io));
				pSocket->assign(::dup(pcap_get_selectable_fd(pcapHandle)));

				PcapSocket pcapSocket;
				pcapSocket.mPcapHandle = pcapHandle;
				pcapSocket.mSocket = pSocket;
				char hwAddress[ETH_ALEN];
				if (getEthernetAddress(*it, hwAddress))
				{
					memcpy(pcapSocket.hwAddress, hwAddress, sizeof(pcapSocket.hwAddress));
					mPcapSockets.insert(pair<string, PcapSocket>(*it, pcapSocket));
					printf("Successfully opened device %s\n", it->c_str());
				}
				else
				{
					pcap_close(pcapSocket.mPcapHandle);
					pcapSocket.mSocket->close();
					printf("ERROR: Unable to get address. Closing device %s\n", it->c_str());
				}
			}
		}
	}
}

void OTASession::read(function<void(char* buffer, int length)> & receiveHandler)
{
	for (auto it = mPcapSockets.begin(); it != mPcapSockets.end(); ++it)
		{
			it->second.mSocket->async_read_some(boost::asio::null_buffers(),
							    bind(&OTASession::handle_read, 
								 this,
								 boost::asio::placeholders::error,
								 boost::asio::placeholders::bytes_transferred,
								 it->second.mPcapHandle,
								 it->second.mSocket,
								 receiveHandler));
		}
}

void OTASession::handle_read(const error_code& ec, 
			     size_t, 
			     pcap_t *pPcapHandle,
			     shared_ptr<stream_descriptor> socket,
			     function<void(char* buffer, int length)> & receiveHandler)
{
	if (ec)
	{
		fprintf(stderr, "OTASession::handle_read error!");
		return;
	}

	pcap_pkthdr* pktHeader;
	const uint8_t* packet;
	int ret = pcap_next_ex(pPcapHandle, &pktHeader, &packet);
	if (ret > 0)
	{
		size_t length = pktHeader->caplen;
		if (USE_ETHERNET_HEADERS)
		{
			// Strip ethernet header
			packet += sizeof(struct ether_header);
			if (pktHeader->caplen < sizeof(struct ether_header))
			  length=0;
			else
			  length -= sizeof(struct ether_header);
		}
			
		if (length > 0 )
		{
		 	receiveHandler((char*)packet, length);
		}
	}
	else
	{
		printf("pcap_next_ex returned %d\n", ret);
	}

	// read again
	socket->async_read_some(boost::asio::null_buffers(),
				bind(&OTASession::handle_read, this,
				     boost::asio::placeholders::error,
				     boost::asio::placeholders::bytes_transferred,
				     pPcapHandle,
				     socket,
				     receiveHandler));
}

void OTASession::write(GroupId gid, BufferPtr pBuffer, int length, bool ctrlPkt, const unsigned char* destHwAddress)
{
	for (auto it = mPcapSockets.begin(); it != mPcapSockets.end(); ++it)
	{
		if (USE_ETHERNET_HEADERS)
			prependEthernetHeader(gid, it->second.hwAddress, pBuffer, ctrlPkt, destHwAddress);
			
		async_write(*it->second.mSocket, buffer(pBuffer->data(), length),
				[this, pBuffer](error_code ec, size_t write_size)
				{
					if (ec)
					{
						fprintf(stderr, ">>>>> OTASession Write error!\n");
					}
#if 0
					else
					{
						cout << ">>>>> OTASession Successful write " << write_size << " bytes"<< endl;
						for (unsigned int i = 0; i < write_size; i++)
						{
							printf("0x%x  ", (unsigned char)(pBuffer->data())[i]);
						}
						printf("\n");
					}
#endif
				});
	}
}

void OTASession::close()
{
	for (auto it = mPcapSockets.begin(); it != mPcapSockets.end(); ++it)
	{
		pcap_close(it->second.mPcapHandle);
		it->second.mSocket->close();
	}
	mPcapSockets.clear();
}

bool OTASession::getEthernetAddress(string ifname, char* hwAddress)
{
	struct ifreq ifr;
	size_t if_name_len=ifname.size();
	if (if_name_len < sizeof(ifr.ifr_name)) 
	{
		memcpy(ifr.ifr_name, ifname.c_str(), if_name_len);
		ifr.ifr_name[if_name_len]=0;
	} 
	else 
	{
		return false;
	}
	
	int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd == -1) 
	{
		return false;
	}
	
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) 
	{
		::close(fd);
		return false;
	}
	::close(fd);
	
	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) 
	{
		return false;
	}

	memcpy(hwAddress, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
	
	return true;
}
#endif

void OTASession::prependEthernetHeader(GroupId gid, const char* hwAddress, BufferPtr pBuffer, bool ctrlPkt, const unsigned char* destHwAddress)
{		
	// Construct Ethernet header
	struct ether_header header;
	if (ctrlPkt)
		header.ether_type = htons(ETH_P_GCN_CTRL);
	else
		header.ether_type = htons(ETH_P_GCN_DATA);

	memcpy(header.ether_shost, hwAddress, sizeof(header.ether_shost));

	if ( mMcastEthernetHeader )
	{
		if ( gid > MAX_MCAST_HEADER_GROUP_ID )
		{
			gid = gid % MAX_MCAST_HEADER_GROUP_ID;
		}

		header.ether_dhost[0] = 0x01;
		header.ether_dhost[1] = 0x00;
		header.ether_dhost[2] = 0x05;
		
		// (Destination set to multicast address, 01:00:05:XX:XX:XX, 
		// where XX:XX:XX represent the group Id. For example group id 1 is
		// destination address of 01:00:05:01:00:00
		memcpy(&header.ether_dhost[3], &gid, sizeof(header.ether_shost)/2);
	}
	else if (destHwAddress)
	{
		memcpy(header.ether_dhost, destHwAddress, sizeof(header.ether_dhost));
	}
	else
	{
		// (Destination set to broadcast address, FF:FF:FF:FF:FF:FF.)
		memset(header.ether_dhost, 0xff, sizeof(header.ether_dhost));
	}

	// copy the header
	memcpy(pBuffer->data(), &header, sizeof(struct ether_header));
}
