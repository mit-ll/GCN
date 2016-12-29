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

#ifndef _GCN_CLIENT_BASIC
#define _GCN_CLIENT_BASIC


#include "gcnClient.h"

// This is a basic client application that uses the gcnClient shared library.

// Key used for the set that holds DATA that the node has seen
class DataKey {
  public:
	GroupId	gid;
	NodeId	gidSrc;
	uint32_t	seq;

	DataKey(GroupId k1, NodeId k2, uint32_t k3)
		: gid(k1), gidSrc(k2), seq(k3) {}  

	bool operator<(const DataKey &right) const 
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


class gcnClientBasic
{
	public:
		gcnClientBasic(io_service * io_serv);
		~gcnClientBasic();
		bool Start(const ClientConfig &config);
		void Stop(const error_code& ec, int signal_number);
		void Run();
		
		io_service*		pIoService;
	private:
		// Required items for a client application
		bool processDataMessage(Data & dataMsg);
		shared_ptr<gcnClient>  pGcnClient;
		
		// These items are specific to this client application
		deadline_timer mSendTimer;
		void OnSendTimeout();
		LogLevel mCurrentLogLevel;
		uint32_t mMessageCounter;
		set<DataKey>	mDataSeenSet;
		
		NodeId	mNodeId;
		GroupId mGroupId;
		double mPushRate;
		uint32_t  mStopCount;
		int    mStopTime;
		int    mStartTime;
		uint32_t  mMsgSize;
}; 

#endif // _GCN_CLIENT_BASIC
