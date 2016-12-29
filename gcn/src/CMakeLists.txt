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


#Specify the version being used
cmake_minimum_required(VERSION 2.8)

# Make sure the compiler can find include files from our src library.
message (STATUS "CMAKE_CURRENT_SOURCE_DIR: " ${CMAKE_CURRENT_SOURCE_DIR})
include_directories (${CMAKE_CURRENT_SOURCE_DIR})

# Build options are Linux and NS3. By default build is for Linux.
# User must enable NS3 by setting the NS3 flag to ON
option(NS3 "Enable compiling for running in NS3" OFF)
option(RELEASE "Build for Release instead of DEBUG" OFF)

# Find required packages

# Threads
find_package(Threads REQUIRED)

# Boost
find_package(Boost COMPONENTS system REQUIRED)
include_directories( ${Boost_INCLUDE_DIR} )

# Google protocol buffers
include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})

message (STATUS "CMAKE_CURRENT_BINARY_DIR: " ${CMAKE_CURRENT_BINARY_DIR})
link_directories (${CMAKE_CURRENT_BINARY_DIR}/src)

# Set the compile flags for Debug or Release
# DEBUG is the default
# -std=c++0x forces compile for C++11
message (STATUS "RELEASE Build: " ${RELEASE})
if (RELEASE)
	set(CMAKE_BUILD_TYPE "Release")
	set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -DUNIX -DHAVE_IPV6 --fPIC")
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.7.0")
		SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++0x")
	else()
		SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11")
	endif()
	
	#Set linker compile flags and linker flags for running in NS3 DCE
	message (STATUS "NS3 Build: " ${NS3})
	if (NS3)
		SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNS3 -DBOOST_ASIO_DISABLE_EPOLL -DBOOST_ASIO_DISABLE_EVENTFD")
		SET (CMAKE_EXE_LINKER_FLAGS "-pie -rdynamic")
	endif()
	message(STATUS "CMAKE_CXX_FLAGS_RELEASE: " ${CMAKE_CXX_FLAGS_RELEASE})
	message(STATUS "CMAKE_EXE_LINKER_FLAGS: " ${CMAKE_EXE_LINKER_FLAGS})
else()
	set(CMAKE_BUILD_TYPE "Debug")
	set(CMAKE_CXX_FLAGS_DEBUG "-Wall -DUNIX -DHAVE_IPV6 -fPIC")
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.7.0")
		SET (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++0x")
	else()
		SET (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11")
	endif()
	
	#Set linker compile flags and linker flags for running in NS3 DCE
	message (STATUS "NS3 Build: " ${NS3})
	if (NS3)
		SET (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DNS3 -DBOOST_ASIO_DISABLE_EPOLL -DBOOST_ASIO_DISABLE_EVENTFD")
		SET (CMAKE_EXE_LINKER_FLAGS "-pie -rdynamic")
	endif()
	message(STATUS "CMAKE_CXX_FLAGS_DEBUG: " ${CMAKE_CXX_FLAGS_DEBUG})
	message(STATUS "CMAKE_EXE_LINKER_FLAGS: " ${CMAKE_EXE_LINKER_FLAGS})
endif()


# *******************************************************
#  build Google protocol buffers Shared Library
#********************************************************

# Compile Google protocol buffers files
include_directories(${CMAKE_CURRENT_BINARY_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS GCNMessage.proto)

# Add library called "protoBuf" that is built from the source files
add_library(protoBuf SHARED ${PROTO_SRCS})

# Link the executable
target_link_libraries (protoBuf ${PROTOBUF_LIBRARIES})

# Install library and supporting files
install (FILES ${PROTO_HDRS} DESTINATION include)
install (TARGETS protoBuf DESTINATION lib)



# *******************************************************
#  build GCN Client Shared Library
#********************************************************
# define the set of source files to be built
SET (GCN_CLIENT_LIB_SRCS gcnClient.cpp Common.cpp)

# defined headers
SET (GCN_CLIENT_LIB_HDRS gcnClient.h Common.h)

# Add library called "gcnClient" that is built from the source files
add_library(gcnClient SHARED ${GCN_CLIENT_LIB_SRCS})

# Link the executable 
target_link_libraries (gcnClient 
		protoBuf
		pcap
		${CMAKE_THREAD_LIBS_INIT}
		${Boost_LIBRARIES})

# Install library and supporting files
install (FILES ${GCN_CLIENT_LIB_HDRS} DESTINATION include)
install (TARGETS gcnClient DESTINATION lib)

# *******************************************************
#  build GCN Client Basic application
#********************************************************
# define the set of source files to be built
SET (APP_SRCS gcnClientBasic.cpp)

# Add executable called "gcnClientBasic" that is built from the source files
# defined above in the APP_SRCS
add_executable (gcnClientBasic ${APP_SRCS} )

target_link_libraries (gcnClientBasic
		gcnClient
		${CMAKE_THREAD_LIBS_INIT}
		${Boost_LIBRARIES}
		${PROTOBUF_LIBRARIES})

		
# *******************************************************
#  build GCN Client Many to One application
#********************************************************
# define the set of source files to be built
SET (APP_MANY_TO_ONE gcnClientManyToOne.cpp)

# Add executable called "gcnClientManyToOne" that is built from the source files
# defined above in the APP_MULTI_SRCS
add_executable (gcnClientManyToOne ${APP_MANY_TO_ONE} )

target_link_libraries (gcnClientManyToOne
		gcnClient
		${CMAKE_THREAD_LIBS_INIT}
		${Boost_LIBRARIES}
		${PROTOBUF_LIBRARIES})

# *******************************************************
#  build gcn
#********************************************************
# define the set of source files to be built
SET (GCN_SRCS gcn.cpp gcnService.cpp Common.cpp)

# Add executable called "gcn" that is built from the source files
# defined above in the GCN_SRCS
add_executable (gcn ${GCN_SRCS} )

# Link the executable to the ProtoLib library.
target_link_libraries (gcn
		protoBuf
		pcap
		${CMAKE_THREAD_LIBS_INIT}
		${Boost_LIBRARIES})

# *******************************************************
#  Install the gcn and and gcnClientBasic
#********************************************************
install (TARGETS gcn gcnClientBasic gcnClientManyToOne DESTINATION bin)
if (NS3)
install (TARGETS gcn gcnClientBasic gcnClientManyToOne gcnClient protoBuf DESTINATION ~/dce/build/lib)
endif()
