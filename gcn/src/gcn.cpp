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


void usage(const string & sAppName)
{
	cout<<"usage: "<<sAppName <<" [OPTIONS]"<<endl;
	cout<<endl;
	cout<<"options:"<<endl;
	cout<<"  -h, --help               Print this message and exit."<<endl;
	cout<<endl;
	cout<<" REQUIRED options: " << endl;
	cout<<"  -i, --id   NODEID       Set the node id."<<endl;
	cout<<endl;
	cout<<" OPTIONAL options: " << endl;
	cout<<endl;
	cout<<"  NOTE: Default values will be used if these options are not specified on the command line"<<endl;
	cout<<endl;
	cout<<"  -l, --loglevel [1-6]     Set initial log level."<<endl;
	cout<<"                               1 = "<< LogLevelStr[1] << "   2 = " << LogLevelStr[2] << "   3 = " << LogLevelStr[3] ;
	cout<<"   4 = "<< LogLevelStr[4] << "   5 = " << LogLevelStr[5] << "   6 = " << LogLevelStr[6] << "   7 = " << LogLevelStr[7] << endl;
	cout<<"                           Default is " << LogLevelStr[DEFAULT_LOG_LEVEL] <<endl;
	cout<<endl;
	cout<<"  -d, --devices DEVICE          Comma separated list of OTA devices." << endl;
	cout<<"                                For Linux: pcap lookup is performed if not specified (should return eth0)" << endl;
	cout<<"                                For NS3: NO lookup performed. Use integers beginning with 1 (e.g., \"1\" opens one device, \"1,2\" opens 2 devices" << endl;
	cout<<endl;
	cout<<"  -f, --datafile DATAFILE       Path to file to log DATAITEMS." << endl;
	cout<<"                                If not given, DATAITEM logging will not be done." << endl;
	cout<<endl;
	cout<<"  -e, --hashexpire HASHEXPIRE   Set the amount of time in seconds that an entry will remain "<<endl;
	cout<<"                                in the hash table before being deleted. "<<endl;
	cout<<"                                Default is "<<DEFAULT_HASHEXPIRE<<" seconds" << endl;
	cout<<endl;
	cout<<"  -c, --hashclean HASHCLEAN     Set the interval for executing the hash clean task."<<endl;
	cout<<"                                Default is every "<< DEFAULT_HASHINTERVAL/1000 << " seconds"<<endl;
	cout<<endl;
	cout<<"  -p, --pullexpire PULLEXPIRE   Set the amount of time in seconds that an entry will remain "<<endl;
	cout<<"                                in the remote pull table without receiving a response to an announce"<<endl;
	cout<<"                                before being deleted. "<<endl;
	cout<<"                                Default is "<<DEFAULT_PULLEXPIRE<<" seconds" << endl;
	cout<<endl;
	cout<<"  -t, --pullclean PULLCLEAN     Set the interval for executing the remote pull table clean task."<<endl;
	cout<<"                                Default is every "<< DEFAULT_PULLINTERVAL/1000 << " seconds"<<endl;
	cout<<endl;
	cout<<"  -r, --pathexpire PATHEXPIRE   Set the amount of time in seconds that an entry will remain "<<endl;
	cout<<"                                in the reverse path table without receiving a response to an announce"<<endl;
	cout<<"                                before being deleted. "<<endl;
	cout<<"                                Default is "<<DEFAULT_REVPATHEXPIRE<<" seconds" << endl;
	cout<<endl;
	cout<<"  -x, --pathclean PATHCLEAN     Set the interval for executing the reverse path clean task."<<endl;
	cout<<"                                Default is every "<< DEFAULT_REVPATHINTERVAL/1000 << " seconds"<<endl;
	cout<<endl;
	cout<<"  -m, --mcastethernetheader     Use group Id based multicast Ethernet headers instead of broadcast Ethernet headers."<<endl;
	cout<<endl;
	cout<<"  -b, --alwaysrebroadcast       When running GCN with acknowledgements, always re-broadcast DATA messages we have not"<<endl;
	cout<<"                                seen yet even if we do not have an entry in Remote Pull table. This is also called robust mode." <<endl;
	cout<<"                                Default behavior is to only re-broadcast if we have an entry in Remote Pull table (downstream subscriber)"<<endl;
	cout<<endl;
}


class Gcn
{
	public:
		Gcn(io_service * io_serv) : pGcnService(NULL), pIoService(io_serv) {};
		~Gcn() 
			{
				boost::system::error_code ec;
				Stop(ec,0);
			};
		GcnService*			pGcnService;
		io_service*			pIoService;

		bool Start(int argc, char* argv[]);
		void Run()
		{
			pGcnService->Run();
		};

		void Stop(const error_code& ec, int signal_number)
		{
			if (pGcnService)
			{
				pGcnService->Stop();
			
				delete pGcnService;
			}
			exit (1);
		}
		
};



bool Gcn::Start(int argc, char* argv[])
{
 	// Verify that the version of the library that we linked against is
	// compatible with the version of the headers we compiled against.
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// Parse command line options
	vector<option> options =
	{
		{"help",       0, nullptr, 'h'},
		{"loglevel",   1, nullptr, 'l'},
		{"datafile",   1, nullptr, 'f'},
		{"id",         1, nullptr, 'i'},
		{"devices",    1, nullptr, 'd'},
		{"hashexpire", 1, nullptr, 'e'},
		{"hashclean",  1, nullptr, 'c'},
		{"pullexpire", 1, nullptr, 'p'},
		{"pullclean",  1, nullptr, 't'},
		{"pathexpire", 1, nullptr, 'r'},
		{"pathclean",   1, nullptr, 'x'},
		{"mcastethernetheader", 0, nullptr, 'm'},
		{"alwaysrebroadcast",   0, nullptr, 'b'},
		{0,            0, nullptr,  0 }
	};

	string sOptString{"hl:f:i:d:e:c:p:t:r:x:mbo"};

	int iOption{};
	int iOptionIndex{};
	
	// create and fill in config structure with default values
	GcnServiceConfig gcnConfig;
	gcnConfig.logLevel = DEFAULT_LOG_LEVEL;
	gcnConfig.nodeId = 0;
	gcnConfig.hashExpire   = DEFAULT_HASHEXPIRE;
	gcnConfig.hashInterval = DEFAULT_HASHINTERVAL;
	gcnConfig.pullExpire   = DEFAULT_PULLEXPIRE;
	gcnConfig.pullInterval = DEFAULT_PULLINTERVAL;
	gcnConfig.pathExpire   = DEFAULT_REVPATHEXPIRE;
	gcnConfig.pathInterval = DEFAULT_REVPATHINTERVAL;
	gcnConfig.mcastEthernetHeader = false;
	gcnConfig.alwaysRebroadcast = false;
	gcnConfig.dataFile = "";

	while((iOption = getopt_long(argc,argv,sOptString.c_str(), &options[0], &iOptionIndex)) != -1)
	{
		switch(iOption)
		{
		case 'h':
			usage(argv[0]);
			return false;
			break;
		case 'l':
			gcnConfig.logLevel = (LogLevel)atoi(optarg);
			break;
		case 'f':
			gcnConfig.dataFile = optarg;
			break;
		case 'i':
			gcnConfig.nodeId = atoi(optarg);
			break;
		case 'd':
			split(gcnConfig.devices, optarg, is_any_of(","), token_compress_on);
			break;
		case 'e':
			gcnConfig.hashExpire = atoi(optarg);
			break;
		case 'c':
			gcnConfig.hashInterval = atoi(optarg) * 1000;
			break;
		case 'p':
			gcnConfig.pullExpire = atoi(optarg);
			break;
		case 't':
			gcnConfig.pullInterval = atoi(optarg) * 1000;
			break;
		case 'r':
			gcnConfig.pathExpire = atoi(optarg);
			break;
		case 'x':
			gcnConfig.pathInterval = atoi(optarg) * 1000;
			break;
		case 'm':
			gcnConfig.mcastEthernetHeader = true;
			break;
		case 'b':
			gcnConfig.alwaysRebroadcast = true;
			break;
		default:
			return false; 
		}
	}
	
	// Check for required variables 
	if ( gcnConfig.nodeId == 0 )
	{
		cout << "\n************** ERROR: Must enter a node id **************\n" << endl;
		usage(argv[0]);
		exit (1);
	}  
	
	// Make sure log level is valid
	if (gcnConfig.logLevel <= LOG_INVALID || gcnConfig.logLevel >= LOG_INVALID_MAX)
	{
		cout <<"\n************** ERROR: Invalid log level: "<< gcnConfig.logLevel <<" **************\n"<<endl;
		usage(argv[0]);
		return false;
	}

	// spawn the GCN
	pGcnService = new GcnService(gcnConfig, pIoService);
	pGcnService->Start();

	return true;

}  // end np::OnStartup()



int main(int argc, char* argv[])
{
	io_service mIoService;
	
	// Create the GCN
	Gcn mGcn(&mIoService);

	if (!(mGcn.Start(argc, argv)) )
	{
		boost::system::error_code ec;
		mGcn.Stop(ec, 0);
	}
	
	boost::asio::signal_set signals(mIoService, SIGINT, SIGTERM);
	signals.async_wait(boost::bind(&Gcn::Stop, &mGcn, _1, _2));

	mGcn.Run();

	return 0;
}
