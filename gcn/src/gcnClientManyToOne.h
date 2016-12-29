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

#ifndef _GCN_CLIENT_MANY_TO_ONE
#define _GCN_CLIENT_MANY_TO_ONE


#include "gcnClient.h"

// This client application support the group node side of Many To One
class gcnClientManyToOne
{
	public:
		gcnClientManyToOne(io_service * io_serv);
		~gcnClientManyToOne();
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
		
		NodeId  mNodeId;
		NodeId  mDestId;
		GroupId mGroupId;
		double mPushRate;
		uint32_t  mStopCount;
		uint32_t  mMsgSize;
}; 

#endif // _GCN_CLIENT_MANY_TO_ONE
