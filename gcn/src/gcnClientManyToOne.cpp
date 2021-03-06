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

#include "gcnClientManyToOne.h"

// This is a client that supports unicast data being sent to the source node.
// The process for this is as follows:
//      - Source sets up the group tree through the use of Advertise/Ack messages
//      - The GCN on the nodes running this client are set up for "all senders" to
//        suppress this node sending advertisements
//      - When the GCN on this node receives the advertisement it sends unicast data
//        back to the source.


void usage(const string & sAppName)
{
	cout<<"usage: "<<sAppName <<" [OPTIONS]"<<endl;
	cout<<endl;
	cout<<"options:"<<endl;
	cout<<"  -h, --help               Print this message and exit."<<endl;
	cout<<endl;
	cout<<" REQUIRED options: " << endl;
	cout<<"  -g, --groupid  GID      Group to which this app belongs." <<endl;
	cout<<endl;
	cout<<"  -i, --id   NODEID       Set the node id."<<endl;
	cout<<endl;
	cout<<"  -d, --dest DEST         Set the unicast destination of the DATA generated by this application."<<endl;
	cout<<endl;
	cout<<" OPTIONAL options: " << endl;
	cout<<endl;
	cout<<"  NOTE: Default values will be used if these options are not specified on the command line"<<endl;
	cout<<"  -l, --loglevel [1-6]    Set initial log level."<<endl;
	cout<<"                               1 = "<< LogLevelStr[1] << "   2 = " << LogLevelStr[2] << "   3 = " << LogLevelStr[3] ;
	cout<<"                               4 = "<< LogLevelStr[4] << "   5 = " << LogLevelStr[5] << "   6 = " << LogLevelStr[6] << "   7 = " << LogLevelStr[7]<< endl;
	cout<<"                           Default is " << LogLevelStr[DEFAULT_LOG_LEVEL] <<endl;
	cout<<endl;
	cout<<"  -v, --datafile DATAFILE  Set the file to log data items to. "<<endl;
	cout<<endl;
	cout<<"  -p, --port PORTNUM       Set the port number to listen to clients. "<<endl;
	cout<<"                           Default is "<<DEFAULT_PORTNUM<<endl;
	cout<<endl;
	cout<<"  -r, --pushrate PUSH_RATE     Set the message rate in seconds for this node to generate DATA messages."<<endl;
	cout<<"                               Default is to generate a message every "<< DEFAULT_PUSH_RATE<< " seconds"<<endl;
	cout<<"                               Use 0 to send no DATA messages"<<endl;
	cout<<endl;
	cout<<"  -b, --msgsize MSG_SIZE       Set the the minimum size of DATA messages to send."<<endl;
	cout<<"                               Default is to generate a message that has a minimum size of 100 bytes"<<endl;
	cout<<endl;
	cout<<"  -x, --resilience RESILIENCE  Resilience value this app should use if it is sending unicast response to GID source."<<endl;
	cout<<"                               Default is 0 which means the app uses its know hop count to the source "<<endl;
	cout<<"                               Use 1 or 2 to increase this value."<<endl;
	cout<<endl;
	cout<<"  -a, --annrate ANNOUNCE_RATE  Set the message rate in seconds for this node to generate ANNOUNCE messages."<<endl;
	cout<<"                               Default is to generate a message every "<< DEFAULT_ANNOUNCE_RATE<< " seconds"<<endl;
	cout<<"                               Use -1 to send no ANNOUNCE messages."<<endl;
	cout<<"                               If 0, then App does not send ANNOUNCE but must still get a pull from GCN before it starts sending data (advertise override)."<<endl;
	cout<<"                               If >0, then App sends ANNOUNCE and must get a pull from GCN before it starts sending data."<<endl;
	cout<<endl;
	cout<<"  -k, --ackprobrelay PROB      Set the probability of relay for ACK messages when received"<<endl;
	cout<<"                               at a node that is not the obligatory relay."<<endl;
	cout<<"                               If the value is from 0 to 100, then the value is used as the prob of relay."<<endl;
	cout<<"                               If the number is greater than 100, then the value is used to determine prob of relay"<<endl;
	cout<<"                               based on the number of neighbors as: value/N where N is # neighbors."<< endl;
	cout<<"                               Default value is 0."<< endl;
	cout<<endl;
	cout<<"  -z, --stopcount STOP_COUNT   Set the number of packet t send before stop sending traffic Applies only to sending nodes."<<endl;
	cout<<"                               Default is never stop."<<endl;
	cout<<endl;
}


//************************************************************************
gcnClientManyToOne::gcnClientManyToOne(io_service * io_serv)
: pIoService(io_serv),
  mSendTimer(*pIoService, Seconds(10)),  
  mMessageCounter(1), 
  mPushRate(DEFAULT_PUSH_RATE),
  mMsgSize(0)
{

}

//************************************************************************
gcnClientManyToOne::~gcnClientManyToOne()
{
	
}


//************************************************************************
bool gcnClientManyToOne::Start(const ClientConfig &config)
{
	pGcnClient.reset(new gcnClient(pIoService));

	mCurrentLogLevel = config.logLevel;

	// We are source node
	// Set timer for sending data (PUSH messages)
	mGroupId = config.gid;
	mPushRate = config.pushRate*1000; // convert to milliseconds
	mStopCount = config.stopCount;
	mMsgSize = config.msgSize;
	mNodeId = config.nodeId;
	mDestId = config.destNodeId;
	mSendTimer.async_wait(boost::bind(&gcnClientManyToOne::OnSendTimeout, this));
	LOG(LOG_FORCE, "scheduling event with start %lfsec and interval %lfmsec", 10.0, mPushRate);

	// init client object;
	function<bool(Data & dataMsg)> handler2 = bind(&gcnClientManyToOne::processDataMessage, this, std::placeholders::_1);
	pGcnClient->Start(config, handler2);

	return true;
}


//************************************************************************
void gcnClientManyToOne::Stop(const error_code& ec, int signal_number)
{
	// If we were sending data stop periodic event for sending
	mSendTimer.cancel();
	printf(" ... Send event canceled\n");
	
	pGcnClient->Stop();
	
	pIoService->stop();
	exit(1);
}

//************************************************************************
void gcnClientManyToOne::Run()
{
	pIoService->run();
}

//************************************************************************
void gcnClientManyToOne::OnSendTimeout()
{
	// get time stamp
	TimeDuration currDur = getTime();
	
	// Get microseconds timestamp to put into packet
	auto micro = duration_cast<microseconds>(currDur).count();
	
	// Construct payload
	array<char, MAX_BUFFER_SIZE> msgBuffer;
	char* tempMsg = msgBuffer.data();
	sprintf(tempMsg, "%ld", micro);
	//LOG_ASSERT(strlen(tempMsg) == TIMESTAMP_SIZE, "Timestamp in message is %d length but it should be %d", strlen(tempMsg), TIMESTAMP_SIZE);
	sprintf(tempMsg + strlen(tempMsg), " %d src %d ", mMessageCounter, mNodeId);
	
	if (strlen(tempMsg) > mMsgSize)
	{
		LOG(LOG_ERROR, "Size of message sent (%d) EXCEEDS the user specified data size (%d)", strlen(tempMsg), mMsgSize);
	}
	
	while (strlen(tempMsg) < mMsgSize) 
	{
		sprintf(tempMsg + strlen(tempMsg), "x");
	}
	
	// only increment message counter if we send successfully
	if (pGcnClient->sendMessage(mGroupId, tempMsg, mDestId))
		mMessageCounter++;

	// reschedule the periodic event
	if ((mPushRate > 0) && !(mStopCount && (mMessageCounter > mStopCount)))
	{
		mSendTimer.expires_at(mSendTimer.expires_at() + Milliseconds(mPushRate));
		mSendTimer.async_wait(boost::bind(&gcnClientManyToOne::OnSendTimeout, this));
	}
	else
	{
		LOG(LOG_DEBUG, "No longer scheduling packet sends. mMessageCounter: %d  mStopCount %d", mMessageCounter, mStopCount);
	}
}

//************************************************************************
// function to process received messages
bool gcnClientManyToOne::processDataMessage(Data & dataMsg)
{
	LOG(LOG_DEBUG, "Received DATA for GID %d", dataMsg.gid());
	return true;
}

//************************************************************************
// main to accept command line and instantiate the GCN client

int main(int argc, char* argv[])
{
	// Parse command line options
	vector<option> options =
	{
		{"help",    0, nullptr, 'h'},
		{"groupid", 1, nullptr, 'g'},
		{"dest",    1, nullptr, 'd'},
		{"loglvel", 1, nullptr, 'l'},
		{"id",      1, nullptr, 'i'},
		{"datafile", 1, nullptr, 'v'},
		{"port",    1, nullptr, 'p'},
		{"pushrate",1, nullptr, 'r'},
		{"annrate", 1, nullptr, 'a'},
		{"ackprobrelay", 1, nullptr, 'k'},
		{"msgsize", 1, nullptr, 'b'},
		{"resilience",1, nullptr, 'x'},
		{"stopcount",1, nullptr, 'z'},
		{0,         0, nullptr,  0 }
	};

	string sOptString{"hg:d:l:i:v:p:r:a:k:b:x:z:"};

	int iOption{};
	int iOptionIndex{};
	int tempRes;
	
	// Init variables with default values
	ClientConfig config;
	config.gid = 0;
	config.logLevel = DEFAULT_LOG_LEVEL;
	config.destNodeId = 0;
	config.nodeId = 0;
	config.type = 1;
	config.port = DEFAULT_PORTNUM;
	config.announceRate = DEFAULT_ANNOUNCE_RATE;
	config.ackProbRelay = 0;
	config.pushRate = DEFAULT_PUSH_RATE;
	config.stopCount = 0;
	config.msgSize = 100;
	config.resilience = LOW;

	while((iOption = getopt_long(argc,argv,sOptString.c_str(), &options[0], &iOptionIndex)) != -1)
	{
		switch(iOption)
		{
		case 'h':
			usage(argv[0]);
			return 0;
			break;
		case 'g':
			config.gid = atoi(optarg);
			break;
		case 'd':
			config.destNodeId = atoi(optarg);
			break;
		case 'l':
			config.logLevel = (LogLevel)atoi(optarg);
			break;
		case 'i':
			config.nodeId = atoi(optarg);
			break;
		case 'v':
			config.dataFile = optarg;
			break;
		case 'p':
			config.port = atoi(optarg);
			break;
		case 'r':
			config.pushRate = atof(optarg);
			break;
		case 'a':
			config.announceRate = atoi(optarg);
			break;
		case 'k':
			config.ackProbRelay = atoi(optarg);
			break;
		case 'z':
			config.stopCount = atoi(optarg);
			break;
		case 'b':
			config.msgSize = atof(optarg);
			break;
		case 'x':
			tempRes = atoi(optarg);
			if (UnicastResilience_IsValid(tempRes))
			{
				config.resilience = (UnicastResilience) tempRes;
			}
			else
			{
				cout <<"\n************** ERROR: Invalid resilience: "<< config.resilience <<" **************"<<endl;
				cout <<"\n************** resilience must be 0, 1 or 2                         **************\n"<<endl;
				usage(argv[0]);
				return 1;
			}
			break;
		default:
			usage(argv[0]);
			exit (1); 
		}
	}

	// Check for required variables 
	if ( config.gid == 0 )
	{
		cout << "\n************** ERROR: Must enter a group id **************\n" << endl;
		usage(argv[0]);
		exit (1);
	} 
	
	if ( config.nodeId == 0 )
	{
		cout << "\n************** ERROR: Must enter a node id **************\n" << endl;
		usage(argv[0]);
		exit (1);
	}  
	
	if ( config.destNodeId == 0 )
	{
		cout << "\n************** ERROR: Must enter a destination node id **************\n" << endl;
		usage(argv[0]);
		exit (1);
	}  
	
	// Make sure log level is valid
	if (config.logLevel <= LOG_INVALID || config.logLevel >= LOG_INVALID_MAX)
	{
		cout <<"\n************** ERROR: Invalid log level: "<< config.logLevel <<" **************\n"<<endl;
		usage(argv[0]);
		return 1;
	}
	
	// Make sure push rate is not zero if we are source
	if ( config.pushRate == 0 )
	{
		cout <<"\n************** ERROR: Invalid push rate: "<< config.pushRate <<" **************"<<endl;
		cout <<"\n************** push rate must be non-zero for source nodes    **************\n"<<endl;
		usage(argv[0]);
		return 1;
	}
	
	// We successfully read the command line 
	// instantiate our app
	io_service mIoService;
	gcnClientManyToOne mApp(&mIoService);
	
	if (!(mApp.Start(config)) )
	{
		boost::system::error_code ec;
		mApp.Stop(ec,0);
	}
	
	boost::asio::signal_set signals(mIoService, SIGINT, SIGTERM);
	signals.async_wait(boost::bind(&gcnClientManyToOne::Stop, &mApp, _1, _2));

	mApp.Run();
	

	return 0;
}
	



