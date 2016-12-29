#
# Group Centric Networking
#
# Copyright (C) 2015 Massachusetts Institute of Technology
# All rights reserved.
#
# Authors:
#           Patricia Deutsch         <patricia.deutsch@ll.mit.edu>
#           Gregory Kuperman         <gkuperman@ll.mit.edu>
#           Leonid Veytser           <veytser@ll.mit.edu>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation;
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

#===================================================================================
# Shell script used to execute a series of NS3 simulations for the GCN scenarios
#===================================================================================
# 
#===========================================================================
# Read in command line arguments
#===========================================================================
# Rules for command line args:
#  0 args specified:
#      - run 10 seeds of every scenario defined
#  1 arg specified: 
#      - arg is number of seeds to run
#      - run specified # of seeds of every scenario defined
#  3 args specified:
#      - arg 1 is number of seeds to run
#      - args 2 and 3 are the starting and ending scenarios
#      - run specified # of seeds of every scenario from start to end inclusive
#        (to run just a single scenario, enter that scenario number as start and end)
#===========================================================================

# get current dir and user id
curr_dir=$(pwd)
user=`whoami`

# set default values then reset to passed in value if the user provided a value
num_runs=10
sc_start=0
sc_end=1000000

if [ $# -ne 0 ]
then
	num_runs=$1
fi

if [ $# -eq 3 ]
then
	sc_start=$2
	sc_end=$3
fi


#===========================================================================
# Set the output files
#===========================================================================
# set base dir where we run dce from and dce out dir where the files-n 
# output is written to. For now these are the same.
base_dir=/home/$user/dce/source/ns-3-dce
dce_out_dir=/home/$user/dce/source/ns-3-dce

# set the outfile directory and summary file
out_dir=$curr_dir/output
if [ ! -d "${out_dir}" ]
then
   mkdir "${out_dir}"
fi
summaryfile="${out_dir}/GCN_summary"

# set stats file directory
stat_dir=$curr_dir/stats;
if [ ! -d "${stat_dir}" ]
then
   mkdir "${stat_dir}"
fi


#===========================================================================
# check the ulimit
#===========================================================================
# This simulation could have large # of nodes and this writes a lot of files. 
# If ulimit (file descriptors) is too low it won't work to go ahead and check it
ulim=$(ulimit -n)
if [ $ulim -lt 2048 ]
then
	ulimit -n 2048
	ulim2=$(ulimit -n)
	echo ""
	echo "*******  ulimit was increased from $ulim to $ulim2 ********"
	echo ""
fi


# define scenarios to run with values for error rate, robust mode, advertise time and prob of relay
#   - nodes is how many nodes to use
#   - srcttl is an integer for the src ttl to use
#   - p_g is an integer between 0 and 100 for the probability that nodes inside radius=250 are group nodes
#   - errorrate is decimal number between 0 and 1 when CONSTANT is used. Otherwise it is AI, AO, BI, BO, CI, CO, DI, DO
#   - robust mode is 0 (off) or 1 (on)
#   - advertise time is the interval for ADVERTISE msg; -1 = no ADVERTISE/ACK message or "flooding"
#   - prob of relay is an integer between 0 and 100
#   - ttlregen is 1 = regenrate ttl  or 0 = don't regenerate ttl; use 0 for classic flood
#   - numpackets is the number of packets to send in the simulation
#   - outerradius is the radius of the outer circle in which all nodes are placed
#   - txrange is the transmission range to use for wireless model
#   - area: 0 for circle and 1 for rectangle
#   - number1 is the inner radius if area is a circle and total x axis length if area is rectangle
#   - number2 is the total y axis height if area is rectangle. NOT used if circle.
#   - mobility: 0 for none and 1 for random waypoint.
#   - minspeed is minimum speed when using random waypoint. Set to 0 if mobility is 0
#   - maxspeed is maximum speed when using random waypoint. Set to 0 if mobility is 0
#   - minpause is minimum pause time when using random waypoint. Set to 0 if mobility is 0
#   - maxpause is maximum pause time when using random waypoint. Set to 0 if mobility is 0
#   - radio is the physical layer to use. Must be either "WIFI", "LLSW" (LL simple wireless),  or "SWDIR" (LL simple wireless directional)
#   - routing is the type of routing that can be used. Must be OLSR, AODV or GCN
#   - allsenders: 1 if all group nodes are also senders. When using GCN, this suppresses ADVERTISE on all nodes except the one source node
#   - node1type: 0 if node 1 is placed at the origin and is the source node. 1 if node 1 is treated like all other nodes (randomly placed). when 1, a source node is randomly picked.
#   - manyToOne:	10 = one to many on  + many to one off
#						11 = one to many on  + many to one on with just 1 group node sending
#						12 = one to many on  + many to one on with all group nodes sending
#						01 = one to many off + many to one on with just 1 group node sending (source node sends ADVERTISE only)
#						02 = one to many off + many to one on with all group nodes sending  (source node sends ADVERTISE only)
#   - uniResil:  0 = Low, 1= Medium, 2= High. Only used if Many to One is enabled.
#   - stats: 1 = write stats files in stat/ for node positions, relay nodes and member nodes. 0 = Off
# Order is as follows:
#   nodes;srcttl;p_g;errorrate;robust;advtime;probrelay;regenttl;numpackets;outerradius;txrange;area;n1;n2;mobility;minS;maxS;minP;maxP;radio;routing;allsenders;node1type;manytoone;uniResil;stats
#
# semi-colon separated WITH NO SPACES!!!!!!!!!!  

#===========================================================================
# Define Scenarios to run
#===========================================================================
# nodes;srcttl;p_g;errorrate;robust;advtime;probrelay;regenttl;numpackets;outerradius;txrange;area;n1;n2;mobility;minS;maxS;minP;maxP;radio;routing;allsenders;node1type;manytoone;uniResil;stats
#===========================================================================
scenario[0]="100;3;75;00;0;-1;000;0;10;100;40;0;100;0;0;10;20;00;00;WIFI;GCN;0;0;10;0;1"
scenario[1]="100;3;75;00;0;-1;000;0;10;100;40;0;100;0;0;10;20;00;00;WIFI;AODV;0;0;10;0;1"
scenario[2]="100;3;75;00;0;-1;000;0;10;100;40;0;100;0;0;10;20;00;00;WIFI;OLSR;0;0;10;0;1"
scenario[3]="100;3;75;00;0;-1;000;0;10;100;40;0;100;0;1;10;20;00;00;WIFI;GCN;0;0;10;0;1"
scenario[4]="100;3;75;00;0;-1;000;0;10;100;40;0;100;0;1;10;20;00;00;WIFI;AODV;0;0;10;0;1"
scenario[5]="100;3;75;00;0;-1;000;0;10;100;40;0;100;0;1;10;20;00;00;WIFI;OLSR;0;0;10;0;1"

scenario[6]="100;3;75;00;0;20;100;1;10;100;40;0;100;0;0;10;20;00;00;WIFI;GCN;0;0;10;0;1"
scenario[7]="100;3;75;00;0;20;200;1;10;100;40;0;100;0;0;10;20;00;00;WIFI;GCN;0;0;10;0;1"
scenario[8]="100;3;75;00;0;20;300;1;10;100;40;0;100;0;0;10;20;00;00;WIFI;GCN;0;0;10;0;1"
scenario[9]="100;3;75;00;0;20;100;1;10;100;40;0;100;0;1;10;20;00;00;WIFI;GCN;0;0;10;0;1"
scenario[10]="100;3;75;00;0;20;200;1;10;100;40;0;100;0;1;10;20;00;00;WIFI;GCN;0;0;10;0;1"
scenario[11]="100;3;75;00;0;20;300;1;10;100;40;0;100;0;1;10;20;00;00;WIFI;GCN;0;0;10;0;1"

#scenario[0]="100;3;75;00;0;-1;000;0;10;100;40;0;100;0;0;10;20;00;00;LLSW;GCN;0;0;10;0;1"
#scenario[1]="100;3;75;00;0;-1;000;0;10;100;40;0;100;0;0;10;20;00;00;LLSW;AODV;0;0;10;0;1"
#scenario[2]="100;3;75;00;0;-1;000;0;10;100;40;0;100;0;0;10;20;00;00;LLSW;OLSR;0;0;10;0;1"
#scenario[3]="100;3;75;00;0;-1;000;0;10;100;40;0;100;0;1;10;20;00;00;LLSW;GCN;0;0;10;0;1"
#scenario[4]="100;3;75;00;0;-1;000;0;10;100;40;0;100;0;1;10;20;00;00;LLSW;AODV;0;0;10;0;1"
#scenario[5]="100;3;75;00;0;-1;000;0;10;100;40;0;100;0;1;10;20;00;00;LLSW;OLSR;0;0;10;0;1"

#scenario[6]="100;3;75;00;0;20;100;1;10;100;40;0;100;0;0;10;20;00;00;LLSW;GCN;0;0;10;0;1"
#scenario[7]="100;3;75;00;0;20;200;1;10;100;40;0;100;0;0;10;20;00;00;LLSW;GCN;0;0;10;0;1"
#scenario[8]="100;3;75;00;0;20;300;1;10;100;40;0;100;0;0;10;20;00;00;LLSW;GCN;0;0;10;0;1"
#scenario[9]="100;3;75;00;0;20;100;1;10;100;40;0;100;0;1;10;20;00;00;LLSW;GCN;0;0;10;0;1"
#scenario[10]="100;3;75;00;0;20;200;1;10;100;40;0;100;0;1;10;20;00;00;LLSW;GCN;0;0;10;0;1"
#scenario[11]="100;3;75;00;0;20;300;1;10;100;40;0;100;0;1;10;20;00;00;LLSW;GCN;0;0;10;0;1"




#=============================================================
# Loop over all the scenarios that are defined but only run 
# the ones that have been selected to run
#=============================================================

for scenarioNum in "${!scenario[@]}"
do
	if [ $scenarioNum -lt $sc_start ]
	then
		continue
	fi
	
	if [ $scenarioNum -gt $sc_end ]
	then
		break
	fi 
	
	# if we get here then we are going to run this scenario so store the
	# scenario contents in sc for processing below
	sc=${scenario[$scenarioNum]}

	# get the values to use for the simulations for this scenario
	nodes=$(echo $sc | cut -d ';' -f 1)
	srcttl=$(echo $sc | cut -d ';' -f 2)
	p_g=$(echo $sc | cut -d ';' -f 3)
	errorrate=$(echo $sc | cut -d ';' -f 4)
	robustmode=$(echo $sc | cut -d ';' -f 5)
	advertisetime=$(echo $sc | cut -d ';' -f 6)
	probrelay=$(echo $sc | cut -d ';' -f 7)
	regenttl=$(echo $sc | cut -d ';' -f 8)
	numpkts=$(echo $sc | cut -d ';' -f 9)
	outerradius=$(echo $sc | cut -d ';' -f 10)
	txrange=$(echo $sc | cut -d ';' -f 11)
	area=$(echo $sc | cut -d ';' -f 12)
	# The next two fields depend on whether the selection area is a circle or rectangle
	if [ $area -eq 0 ]
	then
		# area for group nodes selection is a cirle so we need to read in 1 value
		# before we read the remaining items on the input line
		innerradius=$(echo $sc | cut -d ';' -f 13)
		length=0
		height=0
	else
		# area for group nodes selection is a rectangle so we need t read in 2 values
		# befre we read the remaining items on the input line
		length=$(echo $sc | cut -d ';' -f 13)
		height=$(echo $sc | cut -d ';' -f 14)
		innerradius=0
	fi
	mobility=$(echo $sc | cut -d ';' -f 15)
	# The next four fields depend on whether or not mobility is enabled
	if [ $mobility -eq 1 ]
	then
		minSpeed=$(echo $sc | cut -d ';' -f 16)
		maxSpeed=$(echo $sc | cut -d ';' -f 17)
		minPause=$(echo $sc | cut -d ';' -f 18)
		maxPause=$(echo $sc | cut -d ';' -f 19)
	else
		minSpeed=0
		maxSpeed=0
		minPause=0
		maxPause=0
	fi
	radio=$(echo $sc | cut -d ';' -f 20)
	routing=$(echo $sc | cut -d ';' -f 21)
	allsenders=$(echo $sc | cut -d ';' -f 22)
	node1type=$(echo $sc | cut -d ';' -f 23)
	manytoone=$(echo $sc | cut -d ';' -f 24)
	uniResil=$(echo $sc | cut -d ';' -f 25)
	writestats=$(echo $sc | cut -d ';' -f 26)
	
	#===========================================================================
	# Do some error checking on the mobility. If the user tried turning off
	# mobility by setting the speed to [0,0] NS3 gets hung up so let's just
	# check here if max speed is 0. If so, disable mobility
	#===========================================================================
	if [ $maxSpeed -eq 0 ] 
	then
		mobility=0
	fi
	
	# Can't use mobility with SW Directional
	if [ $radio = 'SWDIR' ]
	then
		mobility=0
		minSpeed=0
		maxSpeed=0
		minPause=0
		maxPause=0
	fi
	
	
	#===========================================================================
	# Do some error checking on the all senders, many to one
	#===========================================================================
	if [[ $allsenders -eq 1  &&  $manytoone != 10 ]] 
	then
		echo " "
		echo "*** ERROR *** Can not enable all senders and many to one at the same time"
		echo "scenario $scenarioNum   scenario line: $sc"
		exit
	fi
	
	if [[ $manytoone != 10  &&  $advertisetime -eq 0  && $routing = 'GCN' ]] 
	then
		echo " "
		echo "*** ERROR *** Must set advertise time to non zero when running many to one"
		echo "scenario $scenarioNum   scenario line: $sc"
		exit
	fi
	
	if [[ $manytoone != 10  &&  $manytoone != 11  &&  $manytoone != 12  &&  $manytoone != 1  &&  $manytoone != 2 ]] 
	then
		echo " "
		echo "*** ERROR *** many to one must be set to 10, 11, 12, 1 or 2"
		echo "scenario $scenarioNum   scenario line: $sc"
		exit
	fi
	
	#===========================================================================
	# Figure out the name of this scenario
	#===========================================================================
	if [ $routing = 'GCN' ]
	then 
		if [ $advertisetime -gt -1 ]
		then
			tree=1
			classic=0
			scenarioName="Group Tree"
		else
			tree=0
			if [ $regenttl -eq 0 ]
			then
				classic=1
				scenarioName="Classic Flood"
			else
				classic=0
				scenarioName="Group Flood"
			fi
		fi
	else
		scenarioName=$routing
	fi
	
	#===========================================================================
	# header for summary file
	#===========================================================================
	echo "=========================================================================================================================================================================================================================">> $summaryfile
	echo "$scenarioName scenario # $scenarioNum" >> $summaryfile
	
	if [ $area -eq 0 ]
	then
		echo "     outer radius: $outerradius   tx Range: $txrange   group node area: circle radius $innerradius" >> $summaryfile
	else
		echo "     outer radius: $outerradius   tx Range: $txrange   group node area: rectangle length $length and height $height" >> $summaryfile
	fi
	if [ $routing = 'GCN' ]
	then
		echo "     robust mode: $robustmode   Advertise Interval: $advertisetime" >> $summaryfile
	fi
	if [ $mobility -eq 1 ]
	then
		echo "     Mobility: Enabled with speed [$minSpeed,$maxSpeed] and pause [$minPause,$maxPause]" >> $summaryfile
	else
		echo "     Mobility: Disabled" >> $summaryfile
	fi
	echo "     number packets: $numpkts" >> $summaryfile
	echo "     all senders: $allsenders" >> $summaryfile
	echo "     many to one: $manytoone" >> $summaryfile
	datetime=$(date)
	echo "     Date: $datetime" >> $summaryfile
	echo "========================================================================================================================================================================================================================">> $summaryfile
	echo -e "\t\t\t\t\t\t\t\tProb\tUni\tClassic\t\t\t\t#Group\t#Group\t%Group\t# Pkts\t% Pkts\tGroup\tNonGrp\tNonGrp\tNonGrp\tUnicast\tControl\tControl\tData\tData" >> $summaryfile
	echo -e "run#\t#Nodes\tRadio\tError\tSpeed\tPause\tRoute\tTree\tRelay\tResil\tFlood\tSrcTtl\tp_g\tSrc\tNodes\tRcvAll\tRecvAll\tRcvd\tRcvd\tRelays\tRelays\tRcv Ack\tRcv Adv\tFwd\tPackets\tBytes\tPackets\tBytes" >> $summaryfile
	echo -e "----\t------\t-----\t-----\t-----\t-----\t-----\t----\t-----\t-----\t------\t-----\t---\t---\t------\t------\t-------\t------\t------\t-------\t-------\t-------\t-------\t-------\t-------\t-------\t-------\t---------" >> $summaryfile

	#===========================================================================
	# Print stuff to the screen
	#===========================================================================
	echo " "
	echo "============================================================================================================================="
	echo "Running $num_runs simulations for scenario $scenarioNum: "
	echo "    nodes: $nodes "
	echo "    radio model: $radio"
	echo "    outer radius: $outerradius"
	echo "    tx Range: $txrange"
	echo "    number packets: $numpkts"
	echo "    all senders: $allsenders"
	echo "    many to one: $manytoone"
	echo "    unicast resilience: $uniResil"
	echo "    p_g: $p_g"
	echo "    node 1 type: $node1type"
	if [ $area -eq 0 ]
	then
		echo "    group node area: circle radius $innerradius"
	
	else
		echo "    rectangle length $length and height $height"
	fi
	
	echo "    routing: $routing"
	if [ $routing = 'GCN' ]
	then
		echo "       tree: $tree"
		echo "       classic flood: $classic"
		echo "       Prob of Relay ACKs: $probrelay"
		echo "       srcttl: $srcttl"
		echo "       Advertise Interval: $advertisetime"
		echo "       robust mode: $robustmode"
	fi
	
	if [[ $errorrate = *[[:digit:]]* ]]
	then
		errormodel='CONSTANT'
		echo "    error model: $errormodel with error rate: $errorrate"
	else 
		errormodel=$errorrate
		errorrate=0.0
		echo "    error model: $errormodel"
	fi
	
	if [ $mobility -eq 1 ]
	then
		echo "    Mobility: Enabled with speed [$minSpeed,$maxSpeed] and pause [$minPause,$maxPause]"
	else
		echo "    Mobility: Disabled"
	fi
	echo "    Write stats: $writestats"
	echo "============================================================================================================================="
	echo " "
	
	#===========================================================================
	# set values for printing error in file
	#===========================================================================
	if [ $radio = 'WIFI' ]
	then
		errorprint=$(echo ' NA ')
	else
		if [ $errormodel = 'CONSTANT' ]
		then
			errorprint=$errorrate
		else
			errorprint=$errormodel
		fi
	fi
	
	#===========================================================================
	# reset counters that are used to calculate averages across all the runs
	#===========================================================================
	AVGgroup_count=0
	AVGgrouprcvcount=0
	AVGgrouppercentRcv=0
	AVGpktrcvcount=0
	AVGpktpercentRcv=0
	AVGrelayGrpCount=0
	AVGrelayNonGrpCount=0
	AVGnonGroupRcvAckCount=0
	AVGnonGroupRcvAdvCount=0
	AVGtotalPktsCtl=0
	AVGtotalBytesCtl=0
	AVGtotalPktsData=0
	AVGtotalBytesData=0
	AVGUnicastFwd=0
	# OLSR only
	AVGrteConverge=0
	
	#===========================================================================
	# run sims
	# This runs all combinations of src_ttl and prob_group_nodes as defined above
	# For each combination, the number of simulations is defined by num_runs
	#===========================================================================
	for ((i = 1; i < ($num_runs + 1); i++))
	do
		#===========================================================================
		# print information to the screen about what sim we are now running
		#===========================================================================
		echo "Running Scenario $scenarioNum Simulation # $i"
		
		#===========================================================================
		# remove all the output files
		# Do this because we grep the output files for results after each sim so we
		# want clean dirs for each sim run
		#===========================================================================
		rm -rf $dce_out_dir/files*
		
		#===========================================================================
		# This next line runs the simulation for the ith scenario
		# The output from the simulation is piped to a text file (outputfile)
		# That file is then grep'd to get the line that has the count of group nodes and
		# that is then piped to cut the line using : delimiter to get last field which is the actual number of group nodes
		# Finally, that value is stored in a variable called group_count.
		# This is done because we need to know the number of group nodes for the simulation to figure out % of group nodes reached
		# Example: This is example output of the simulation. We want the number 9 to be stored in group_count
		#          So grep for that line then split it with cut using : as delimiter and take the second field from the cut
		#		Number of group nodes: 9
		#		Number In Range: 108
		#		Number Out of Range: 291
		#===========================================================================
		outputfile="${out_dir}/sim_out_scenario${scenarioNum}_run${i}.txt"
		args="--nNodes=$nodes \
				--radioModel=$radio \
				--routingType=$routing \
				--srcttl=$srcttl \
				--p_g=$p_g \
				--advtime=$advertisetime \
				--robust=$robustmode \
				--errorModel=$errormodel \
				--error=$errorrate \
				--probrelay=$probrelay \
				--regenttl=$regenttl \
				--numPackets=$numpkts \
				--selectAreaShape=$area \
				--outerRadius=$outerradius \
				--innerRadius=$innerradius \
				--lengthRectX=$length \
				--heightRectY=$height \
				--useMobility=$mobility \
				--minSpeed=$minSpeed \
				--maxSpeed=$maxSpeed \
				--minPause=$minPause \
				--maxPause=$maxPause \
				--txrange=$txrange \
				--allSenders=$allsenders \
				--node1Type=$node1type \
				--manyToOne=$manytoone \
				--uniResil=$uniResil \
				--writeStats=$writestats"
		
		# UNCOMMNENT this line if you want to run with GDB
		#sudo $base_dir/waf --run GCN_network_tests_LLSW --command-template="gdb -ex 'handle SIGUSR1 nostop noprint' --args %s $args"
		
		if [ $radio = 'WIFI' ]
		then
			NS_GLOBAL_VALUE="RngRun=$i" $base_dir/waf --run "GCN_network_tests_Wifi $args"  > $outputfile
		else
			NS_GLOBAL_VALUE="RngRun=$i" $base_dir/waf --run "GCN_network_tests_LLSW $args"  > $outputfile
		fi
		
		#===========================================================================
		# DONE RUNNING now post process
		#===========================================================================
		group_count=$(grep "Number of group nodes" $outputfile | cut -d ':' -f 2)
		
		# set up what we will print for source node
		# if the node 1 type is 1 then we randomly pick a node and so we want
		# to print that node id
		# if the node 1 type is 0 then we use node 1. We just print 0 to show that it was not random.
		if [ $node1type -eq 1 ] 
		then
			srcnode=$(grep 'source node' $outputfile | cut -d ' ' -f 2)
		else
			srcnode=0
		fi

		if [ $routing = 'GCN' ]
		then
			fatal_errs=$(find $dce_out_dir -iname stderr | xargs cat)
			if [ ! -z "$fatal_errs"  ]
			then
				echo "FATAL error occured. Check $dce_out_dir for these errors using: find $dce_out_dir -iname stderr | xargs cat";
				echo -e "\n **** FATAL error occured. Re-run this scenario and check $dce_out_dir for these errors using: find $dce_out_dir -iname stderr | xargs cat \n" >> $summaryfile
				continue;
			fi
			
			#===========================================================================
			# Now we need to check the stats that are written by the GCN code to stdout when the simulation is running
			# To do this, we look at all the output files for all nodes (i.e., in the dirs files-1, ..., files-n)
			# and look for the lines that have the client stats and lines that have GCN stats
			# The first two grep line below dump the output to a file. Note that each sim appends to the file.
			# The next grep lines look for line counts and stores the values in variables so they can be printed
			# and added to totals. The totals are used to get average across the runs for each seed
			# The ".*" in the client stats grep matches any group id
			#===========================================================================
			totalGroupRcvCount=0   # number of group nodes receiving all N packets
			totalPktRcvCount=0     # total number of packets received
			totalPktsSent=0
			if [ $allsenders -eq 1 ] 
			then
				# For GCN when we use all senders, the number of nodes sending is 1 for source node + group nodes
				# and number receiving is the same because each node has a subscribing client.
				totalPktsSent=$(($numpkts*($group_count+1)*($group_count+1)))   # total number of packets sent
			else
				if [ $manytoone -eq 1 ]
				then
					# if many to one is option 1 then there is just one sender
					totalPktsSent=$numpkts
				fi
				if [ $manytoone -eq 11 ]
				then
					# if many to one is option 11 then there are 2 senders: src node to N and 1 group node to source
					totalPktsSent=$(($numpkts + ($numpkts*$group_count)))
				fi
				if [ $manytoone -eq 12 ]
				then
					# if many to one is option 12 then there are N + 1 senders: src node to N and N group nodes to source
					totalPktsSent=$((($numpkts*$group_count) + ($numpkts*$group_count)))
				fi
				if [[ $manytoone -eq 10  || $manytoone -eq 2  ]]
				then
					# if many to one is 10 or 2 
					totalPktsSent=$(($numpkts*$group_count))   # total number of packets sent
				fi
			fi
			
			totalrelayGrpCount=0
			totalrelayNonGrpCount=0
			totalnonGroupRcvAckCount=0
			totalnonGroupRcvAdvCount=0
			totalPktsCtl=0
			totalBytesCtl=0
			totalPktsData=0
			totalBytesData=0
			totalUnicastFwd=0
			for ((j=1; j<$nodes+1; j++))
			do
				# IMPORTANT!!!!! The recvcount grep must match the number of messages sent!!!!
				if [ $allsenders -eq 0 ]
				then
					pkts_expected=$numpkts
				else
					pkts_expected=$(($numpkts*($group_count+1)))
				fi
				
				grprcvcount=0
				nodeid=$j
				groupnode=$(grep "Node $nodeid .* Position" $outputfile | cut -d ' ' -f 3)
				if [ "$groupnode" == "IS" ]
				then
					if [[ $manytoone != 1  &&  $manytoone != 2 ]]
					then
						grprcvcount=$(grep -R "OnStatTimeout] GCN Client stats type: Listener only .* rcvd>$pkts_expected " $dce_out_dir/files-$j | tail -n 1 | wc -l)
					fi
				fi
				totalGroupRcvCount=$(($totalGroupRcvCount + $grprcvcount))
				
				temp=$(grep -R 'OnStatTimeout] GCN Client stats type: Listener only .* ' $dce_out_dir/files-$j | tail -n 1 | wc -l)
				if [ $temp -gt 0 ]
				then
					if [[ $manytoone -eq 11  || $manytoone -eq 12  ]]
					then
						# for 11 and 12 the listener on the source node gets the src's packets so we only want the unicast received here
						# only problem is that for 11 and 12 the source node has unicast received but all the other nodes don't
						# so get the recv uni (field 6) first and if that is 0 get the recv (field 2)
						pktRcvCount=$(grep -R 'OnStatTimeout] GCN Client stats type: Listener only .* ' $dce_out_dir/files-$j | tail -n 1 | cut -d '>' -f 6 | cut -d ' ' -f 1)
						if [ $pktRcvCount -eq 0 ]
						then
							pktRcvCount=$(grep -R 'OnStatTimeout] GCN Client stats type: Listener only .* ' $dce_out_dir/files-$j | tail -n 1 | cut -d '>' -f 2 | cut -d ' ' -f 1)
						fi
					else
						pktRcvCount=$(grep -R 'OnStatTimeout] GCN Client stats type: Listener only .* ' $dce_out_dir/files-$j | tail -n 1 | cut -d '>' -f 2 | cut -d ' ' -f 1)
					fi
					totalPktRcvCount=$(($totalPktRcvCount + $pktRcvCount))
				fi
				
				relayGrpCount=$(grep -R 'relayDataGroup>1' $dce_out_dir/files-$j | tail -n 1 | wc -l)
				totalrelayGrpCount=$(($totalrelayGrpCount + $relayGrpCount))
				
				relayNonGrpCount=$(grep -R 'relayDataNonGroup>1' $dce_out_dir/files-$j | tail -n 1 | wc -l)
				totalrelayNonGrpCount=$(($totalrelayNonGrpCount + $relayNonGrpCount))
				
				nonGroupRcvAckCount=$(grep -R 'nonGroupRcvAck>1' $dce_out_dir/files-$j | tail -n 1 | wc -l)
				totalnonGroupRcvAckCount=$(($totalnonGroupRcvAckCount + $nonGroupRcvAckCount))
				
				nonGroupRcvAdvCount=$(grep -R 'nonGroupRcvAdv>1' $dce_out_dir/files-$j | tail -n 1 | wc -l)
				totalnonGroupRcvAdvCount=$(($totalnonGroupRcvAdvCount + $nonGroupRcvAdvCount))
				
				pkts=$(grep -R 'totalPacketsSentCtl>'  $dce_out_dir/files-$j | tail -n 1 | cut -d '>' -f 17 | cut -d ' ' -f 1)
				totalPktsCtl=$(($totalPktsCtl + $pkts))
				
				bytes=$(grep -R 'totalBytesSentCtl>' $dce_out_dir/files-$j | tail -n 1 | cut -d '>' -f 16 | cut -d ' ' -f 1)
				totalBytesCtl=$(($totalBytesCtl + $bytes))
				
				pkts=$(grep -R 'totalPacketsSentData>'  $dce_out_dir/files-$j | tail -n 1 | cut -d '>' -f 19 | cut -d ' ' -f 1)
				totalPktsData=$(($totalPktsData + $pkts))
				
				bytes=$(grep -R 'totalBytesSentData>' $dce_out_dir/files-$j | tail -n 1 | cut -d '>' -f 18 | cut -d ' ' -f 1)
				totalBytesData=$(($totalBytesData + $bytes))
				
				unifwd=$(grep -R 'fwdUni>' $dce_out_dir/files-$j | tail -n 1 | cut -d '>' -f 11 | cut -d ' ' -f 1)
				totalUnicastFwd=$(($totalUnicastFwd + $unifwd))
			done
			if [ $allsenders -eq 0 ]
			then
				groupPercentRcv=$(echo "scale=2; $totalGroupRcvCount/$group_count" | bc)
			else
				groupPercentRcv=$(echo "scale=2; $totalGroupRcvCount/($group_count+1)" | bc)
			fi
			pktPercentRcv=$(echo "scale=2; $totalPktRcvCount/$totalPktsSent" | bc)
			
			#===========================================================================
			# Now append the info from this simulation to the summary file. That file can be pulled into Excel to graph the results.
			#===========================================================================
			# If we are using mobility, the # of relay nodes is not used because it changes over time
			if [ $mobility -eq 1 ]
			then
				echo -e "$i\t$nodes\t$radio\t$errorprint\t[$minSpeed,$maxSpeed]\t[$minPause,$maxPause]\t$routing\t$tree\t$probrelay\t$uniResil\t$classic\t$srcttl\t$p_g\t$srcnode\t$group_count\t$totalGroupRcvCount\t$groupPercentRcv\t$totalPktRcvCount\t$pktPercentRcv\t NA \t NA \t$totalnonGroupRcvAckCount\t$totalnonGroupRcvAdvCount\t$totalUnicastFwd\t$totalPktsCtl\t$totalBytesCtl\t$totalPktsData\t$totalBytesData" >> $summaryfile
			else
				echo -e "$i\t$nodes\t$radio\t$errorprint\t[$minSpeed,$maxSpeed]\t[$minPause,$maxPause]\t$routing\t$tree\t$probrelay\t$uniResil\t$classic\t$srcttl\t$p_g\t$srcnode\t$group_count\t$totalGroupRcvCount\t$groupPercentRcv\t$totalPktRcvCount\t$pktPercentRcv\t$totalrelayGrpCount\t$totalrelayNonGrpCount\t$totalnonGroupRcvAckCount\t$totalnonGroupRcvAdvCount\t$totalUnicastFwd\t$totalPktsCtl\t$totalBytesCtl\t$totalPktsData\t$totalBytesData" >> $summaryfile
			fi
			
			#===========================================================================
			# Add to running tally
			#===========================================================================
			AVGgroup_count=$(($AVGgroup_count + $group_count))
			AVGgrouprcvcount=$(($AVGgrouprcvcount + $totalGroupRcvCount))
			AVGgrouppercentRcv=$(echo "$AVGgrouppercentRcv + $groupPercentRcv" | bc)
			AVGpktrcvcount=$(($AVGpktrcvcount + $totalPktRcvCount))
			AVGpktpercentRcv=$(echo "$AVGpktpercentRcv + $pktPercentRcv" | bc)
			AVGrelayGrpCount=$(($AVGrelayGrpCount + $totalrelayGrpCount))
			AVGrelayNonGrpCount=$(($AVGrelayNonGrpCount + $totalrelayNonGrpCount))
			AVGnonGroupRcvAckCount=$(($AVGnonGroupRcvAckCount + $totalnonGroupRcvAckCount))
			AVGnonGroupRcvAdvCount=$(($AVGnonGroupRcvAdvCount + $totalnonGroupRcvAdvCount))
			AVGtotalPktsCtl=$(($AVGtotalPktsCtl + $totalPktsCtl))
			AVGtotalBytesCtl=$(($AVGtotalBytesCtl + $totalBytesCtl))
			AVGtotalPktsData=$(($AVGtotalPktsData + $totalPktsData))
			AVGtotalBytesData=$(($AVGtotalBytesData + $totalBytesData))
			AVGUnicastFwd=$(($AVGUnicastFwd + $totalUnicastFwd))
		else
			# The output for OLSR is different than for GCN.
			# For OLSR, all the values are written to the sim.out file so we can just get them all from there
			totalPktRcvCount=$(grep "Receive Count" $outputfile | cut -d ':' -f 2)
			pktPercentRcv=$(grep "% Received" $outputfile | cut -d ':' -f 2)
			totalPktsCtl=$(grep "Control Packets" $outputfile | cut -d ':' -f 2)
			totalBytesCtl=$(grep "Control Bytes" $outputfile | cut -d ':' -f 2)
			totalPktsData=$(grep "Data Packets" $outputfile | cut -d ':' -f 2)
			totalBytesData=$(grep "Data Bytes" $outputfile | cut -d ':' -f 2)
			rteConverge=$(grep "Routes Converged" $outputfile| cut -d ':' -f 2)
			
			echo -e "$i\t$nodes\t$radio\t$errorprint\t[$minSpeed,$maxSpeed]\t[$minPause,$maxPause]\t$routing\t NA \t NA \t NA \t NA \t$p_g\t$srcnode\t$group_count\t NA \t NA \t$totalPktRcvCount\t$pktPercentRcv\t NA \t NA \t NA \t NA \t NA \t$totalPktsCtl\t$totalBytesCtl\t$totalPktsData\t$totalBytesData" >> $summaryfile
			
			#===========================================================================
			# Add to running tally
			#===========================================================================
			AVGgroup_count=$(($AVGgroup_count + $group_count))
			AVGpktrcvcount=$(($AVGpktrcvcount + $totalPktRcvCount))
			AVGpktpercentRcv=$(echo "$AVGpktpercentRcv + $pktPercentRcv" | bc)
			AVGtotalPktsCtl=$(($AVGtotalPktsCtl + $totalPktsCtl))
			AVGtotalBytesCtl=$(($AVGtotalBytesCtl + $totalBytesCtl))
			AVGtotalPktsData=$(($AVGtotalPktsData + $totalPktsData))
			AVGtotalBytesData=$(($AVGtotalBytesData + $totalBytesData))
			AVGrteConverge=$(($AVGrteConverge + $rteConverge))
		fi
		
		#===========================================================================
		# Now files written to the stats directory if enabled
		#===========================================================================
		if [ $writestats -eq 1 ]
		then
			#===========================================================================
			# Build relay node file (this only applies when GCN is enabled)
			#===========================================================================
			if [ $routing = 'GCN' ]
			then
				statfile="${stat_dir}/scenario${scenarioNum}_run${i}_relays.txt"
				# first get the start time and end time
				starttime=$(grep -R 'Node 1' $dce_out_dir/files-1 | head -n 1 | cut -d ':' -f 2 | cut -d '.' -f 1)
				endtime=$(grep -R 'Node 1' $dce_out_dir/files-1 | tail -n 1 | cut -d ':' -f 2 | cut -d '.' -f 1)
				secondCounter=0
				echo -n "" > $statfile
				for ((t=$starttime; t<=$endtime; t++))
				do
					echo -n "$secondCounter" >> $statfile
					for ((j=1; j<$nodes+1; j++))
					do
						relayGrp=$(grep -R $t $dce_out_dir/files-$j | grep 'relayDataGroup>1' | wc -l)
						relayNonGrp=$(grep -R $t $dce_out_dir/files-$j | grep 'relayDataNonGroup>1' | wc -l)
						if [[ $relayGrp -eq 1  || $relayNonGrp -eq 1  ]]
						then
							echo -n ";$j" >> $statfile
						fi
					done
					echo "" >> $statfile
					secondCounter=$((secondCounter + 1))
				done
			fi
			#===========================================================================
			# Build members file
			#===========================================================================
			statfile="${stat_dir}/scenario${scenarioNum}_run${i}_members.txt"
			echo "source:$srcnode" > $statfile
			echo -n "group:" >> $statfile
			# set flag so we write first group node without a preceeding ","
			firstnode=0
			for ((j=1; j<$nodes+1; j++))
			do
				grpNode=$(grep "Node $j " $outputfile  | head -n 1 | grep 'IS'| wc -l)
				if [ $grpNode -eq 1 ]
				then
					if [ $firstnode -eq 0 ]
					then
						echo -n "$j" >> $statfile
						firstnode=1
					else
						echo -n ",$j" >> $statfile
					fi
				fi
			done
			echo "" >> $statfile
			#===========================================================================
			# Build positions file
			#===========================================================================
			# Don't need to check for mobility here because NS3 only writes positions
			# besides at t=0 when mobility is enabled.
			statfile="${stat_dir}/scenario${scenarioNum}_run${i}_positions.txt"
			grep ';(' $outputfile > $statfile
		fi
		
	done
	# we are now done running all the seeds for this scenario
	
	#===========================================================================
	# calculate averages
	#===========================================================================
	AVGgroup_count=$(echo "scale=1; $AVGgroup_count/$num_runs" | bc)
	AVGgrouprcvcount=$(echo "scale=1; $AVGgrouprcvcount/$num_runs" | bc)
	AVGgrouppercentRcv=$(echo "scale=2; $AVGgrouppercentRcv/$num_runs" | bc)
	AVGpktrcvcount=$(echo "scale=1; $AVGpktrcvcount/$num_runs" | bc)
	AVGpktpercentRcv=$(echo "scale=2; $AVGpktpercentRcv/$num_runs" | bc)
	AVGrelayGrpCount=$(echo "scale=1; $AVGrelayGrpCount/$num_runs" | bc)
	AVGrelayNonGrpCount=$(echo "scale=1; $AVGrelayNonGrpCount/$num_runs" | bc)
	AVGnonGroupRcvAckCount=$(echo "scale=1; $AVGnonGroupRcvAckCount/$num_runs" | bc)
	AVGnonGroupRcvAdvCount=$(echo "scale=1; $AVGnonGroupRcvAdvCount/$num_runs" | bc)
	AVGtotalPktsCtl=$(echo "scale=1; $AVGtotalPktsCtl/$num_runs" | bc)
	AVGtotalBytesCtl=$(echo "scale=1; $AVGtotalBytesCtl/$num_runs" | bc)
	AVGtotalPktsData=$(echo "scale=1; $AVGtotalPktsData/$num_runs" | bc)
	AVGtotalBytesData=$(echo "scale=1; $AVGtotalBytesData/$num_runs" | bc)
	AVGUnicastFwd=$(echo "scale=1; $AVGUnicastFwd/$num_runs" | bc)
	AVGrteConverge=$(echo "scale=1; $AVGrteConverge/$num_runs" | bc)
	
	# If we are using mobility, the # of relay nodes is not used because it changes over time
	if [ $mobility -eq 1 ]
	then
		AVGrelayGrpCount=$(echo " NA ")
		AVGrelayNonGrpCount=$(echo " NA ")
	fi
	#===========================================================================
	# print averages
	#===========================================================================
	echo -e "\t\t\t\t\t\\t\t\t\t\t\t\t\t\t------\t------\t-------\t------\t------\t-------\t-------\t-------\t-------\t-------\t-------\t-------\t-------\t---------" >> $summaryfile

	if [ $routing = 'GCN' ]
	then
		echo -e "Sc $scenarioNum\t$nodes\t$radio\t$errorprint\t[$minSpeed,$maxSpeed]\t[$minPause,$maxPause]\t$routing\t$tree\t$probrelay\t$uniResil\t$classic\t$srcttl\t$p_g\tNA\t$AVGgroup_count\t$AVGgrouprcvcount\t$AVGgrouppercentRcv\t$AVGpktrcvcount\t$AVGpktpercentRcv\t$AVGrelayGrpCount\t$AVGrelayNonGrpCount\t$AVGnonGroupRcvAckCount\t$AVGnonGroupRcvAdvCount\t$AVGUnicastFwd\t$AVGtotalPktsCtl\t$AVGtotalBytesCtl\t$AVGtotalPktsData\t$AVGtotalBytesData" >> $summaryfile
	else
		echo -e "Sc $scenarioNum\t$nodes\t$radio\t$errorprint\t[$minSpeed,$maxSpeed]\t[$minPause,$maxPause]\t$routing\t NA \t NA \t NA \t NA \t NA \t$p_g\tNA\t$AVGgroup_count\t NA \t NA \t$AVGpktrcvcount\t$AVGpktpercentRcv\t NA \t NA \t NA \t NA \t NA \t$AVGtotalPktsCtl\t$AVGtotalBytesCtl\t$AVGtotalPktsData\t$AVGtotalBytesData" >> $summaryfile
	fi
	echo " " >> $summaryfile
	
	echo "**** COMPLETED ALL simulations for Scenario $scenarioNum"

done
