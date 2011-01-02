/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2010 The SWG:ANH Team
---------------------------------------------------------------------------------------
Use of this source code is governed by the GPL v3 license that can be found
in the COPYING file or at http://www.gnu.org/licenses/gpl-3.0.html

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
---------------------------------------------------------------------------------------
*/

// this will send out of orders and just skip the packets missing
//#define SEND_OUT_OF_ORDERS

#include "Session.h"

#include <cstdio>
#include <algorithm>

#if !defined(_MSC_VER)
#include <arpa/inet.h>
#endif

#include <boost/thread/thread.hpp>

#include "Utils/rand.h"
#include "Utils/utils.h"

// Fix for issues with glog redefining this constant
#ifdef ERROR
#undef ERROR
#endif

#include <glog/logging.h>

#include "NetworkManager/MessageFactory.h"
#include "NetworkManager/NetworkClient.h"
#include "NetworkManager/Packet.h"
#include "NetworkManager/PacketFactory.h"
#include "NetworkManager/Service.h"
#include "NetworkManager/SocketReadThread.h"
#include "NetworkManager/SocketWriteThread.h"

//======================================================================================================================

Session::Session(void) :
    mService(0),
    mClient(0),
    mSocketReadThread(0),
    mSocketWriteThread(0),
    mPacketFactory(0),
    mMessageFactory(0),
    mId(0),
    mAddress(0),
    mPort(0),
    mEncryptKey(0),
    mRequestId(0),
    mOutgoingPingSequence(1),
    mFragmentedPacketTotalSize(0),
    mFragmentedPacketCurrentSize(0),
    mFragmentedPacketStartSequence(0),
    mFragmentedPacketCurrentSequence(0),
    mRoutedFragmentedPacketTotalSize(0),
    mRoutedFragmentedPacketCurrentSize(0),
    mRoutedFragmentedPacketStartSequence(0),
    mRoutedFragmentedPacketCurrentSequence(0),
    mConnectStartEvent(0),
    mLastConnectRequestSent(0),
    mLastPacketReceived(0),
    mLastPacketSent(0),
    mLastRoundtripTime(0),
    mAverageRoundtripTime(0),
    mShortestRoundtripTime(0),
    mLongestRoundtripTime(0),
    mClientPacketsSent(0),
    mClientPacketsReceived(0),
    mServerPacketsSent(0),
    mServerPacketsReceived(0),
    mOutSequenceNext(0),
    mInSequenceNext(0),
    mOutSequenceRollover(false),
    mNextPacketSequenceSent(0),
    mLastRemotePacketAckReceived(0),
    mWindowSizeCurrent(8000),
    mWindowResendSize(8000),
    mSendDelayedAck(false),
    mInOutgoingQueue(false),
    mInIncomingQueue(false),
    mStatus(SSTAT_Initialize),
    mCommand(SCOM_None),
    avgTime(0),
    avgPacketsbuild(0),
    avgUnreliablesbuild(0),
    mPacketBuildTimeLimit(15),
    lowest(0),
    lowestCount(0)
{
    mConnectStartEvent = lasttime = Anh_Utils::Clock::getSingleton()->getLocalTime();       // For SCOM_Connect commands
    mLastConnectRequestSent = mConnectStartEvent;

    mLastPacketReceived = mConnectStartEvent;      // General session timeout
    mLastPacketSent = mConnectStartEvent;          // General session timeout
    mLastRemotePacketAckReceived = mConnectStartEvent;          // General session timeout

	//this will be changed by the sessionfactory eventually
    mServerService = false;
    mMaxPacketSize = MAX_PACKET_SIZE;
    mMaxUnreliableSize= MAX_PACKET_SIZE;

    mLastPingPacketSent = 0;

    endCount = 0;
    mHash = 0;

	mLastHouseKeepingTimeTime = mLastWriteThreadTime = mConnectStartEvent;

}

//======================================================================================================================

Session::~Session(void)
{
  
    DLOG(INFO) <<  "Session::~Session " << this->getId();
    Message* message = 0;

    boost::recursive_mutex::scoped_lock lk(mSessionMutex);

    while(mReliableMessageQueue.pop(message))    {
        // We're done with this message.
        message->setPendingDelete(true);
        message->mSession = NULL;
    }

    while(mIncomingMessageQueue.pop(message))    {
        // We're done with this message.
        message->setPendingDelete(true);
        message->mSession = NULL;
    }

    while(mUnreliableMessageQueue.pop(message))    {
        // We're done with this message.
        message->setPendingDelete(true);
        message->mSession = NULL;
    }

    while(!mMultiMessageQueue.empty())
    {
        message = mMultiMessageQueue.front();
        mMultiMessageQueue.pop();

        // We're done with this message.
        message->setPendingDelete(true);
        message->mSession = NULL;
    }


    while(!mRoutedMultiMessageQueue.empty())
    {
        message = mRoutedMultiMessageQueue.front();
        mRoutedMultiMessageQueue.pop();

        // We're done with this message.
        message->setPendingDelete(true);
        message->mSession = NULL;
    }

    while(!mMultiUnreliableQueue.empty())
    {
        message = mMultiUnreliableQueue.front();
        mMultiUnreliableQueue.pop();

        // We're done with this message.
        message->setPendingDelete(true);
        message->mSession = NULL;
    }

    PacketWindowList::iterator it;

    it = mWindowPacketList.begin();

    while(it != mWindowPacketList.end())
    {
        Packet* packet = (*it);
        mPacketFactory->DestroyPacket(packet);
        mWindowPacketList.erase(it++);
    }

	
	Packet* packet;
	while(!mNewWindowPacketList.empty())    {
		packet = mNewWindowPacketList.front();
		mNewWindowPacketList.pop();
        mPacketFactory->DestroyPacket(packet);
    }

    while(mOutgoingReliablePacketQueue.pop(packet))    {
        mPacketFactory->DestroyPacket(packet);
    }


    while(mOutgoingUnreliablePacketQueue.pop(packet))    {
        mPacketFactory->DestroyPacket(packet);
    }

    //PacketQueue                 mOutgoingReliablePacketQueue;		//these are packets put on by the sessionwrite thread to send
    //PacketQueue                 mOutgoingUnreliablePacketQueue;   //build unreliables they will get send directly by the socket write thread  without storing for possible r esends

}



//======================================================================================================================

//======================================================================================================================

void Session::ProcessWriteThread(void)
{

    uint64 now = Anh_Utils::Clock::getSingleton()->getLocalTime();

    //only process when we are busy - we dont need to iterate through possible resends all the time
	if((!mUnreliableMessageQueue.filled())&&(!mReliableMessageQueue.filled()) && (mNewWindowPacketList.empty()))
    {
        if(!mSendDelayedAck)
        {
            if(now - mLastWriteThreadTime < 500)
            {
                endCount++;
                return;
            }
        }
    }

    mLastWriteThreadTime = now;

    // Process our delayed ack here
    if(mSendDelayedAck)
    {
        // please note that we push it directly on the packet queue it wont be build as unreliable and wont end up in a 00 03

        // clear our send flag
        mSendDelayedAck = false;

        // send an ack for this packet.
        Packet* ackPacket = mPacketFactory->CreatePacket();
        ackPacket->addUint16(SESSIONOP_DataAck1);
        ackPacket->addUint16(htons(mInSequenceNext - 1));

        // Acks only need encryption
        ackPacket->setIsCompressed(false);
        ackPacket->setIsEncrypted(true);

        // Push the packet on our outgoing queue
        _addOutgoingUnreliablePacket(ackPacket);
    }

    // We need to prepare any outgoing packets here

    uint32 pBuild = 0;
    uint32 pUnreliableBuild = 0;

    //build reliable packets dont use timeGetTime ... -its expensive
    while((pBuild < 200) && mReliableMessageQueue.filled())
    {
        pBuild += _buildPackets();
    }
    
    //build unreliable packets
    while((pUnreliableBuild < 200) && mUnreliableMessageQueue.filled())
    {
        //unreliables are directly put on the wire without getting in the way of our window
        //this way they get lost when we have lag but thats not exactly a hughe problem
        //and we dont get problems with our window list filled with unreliables
        //which never get acked
        pUnreliableBuild += _buildPacketsUnreliable();
    }

    // Now check to see if we can send any more reliable packets out the wire yet.  NOT optimized
    PacketWindowList::iterator	iter;
    
    Packet*						windowPacket	= NULL;
    uint32						packetsSent		= 0;

	// we need the mutex as the windowpacketList needs to remain a list 
	boost::recursive_mutex::scoped_lock lk(mSessionMutex);
    while(! mNewWindowPacketList.empty())    {
		
		windowPacket = mNewWindowPacketList.front();
		mNewWindowPacketList.pop();

		_addOutgoingReliablePacket(windowPacket);

		//holds the packets waiting to be acknowledged or resend
        mWindowPacketList.push_back(windowPacket);
        packetsSent++;

		// If we've sent our mWindowSizeCurrent of packets, break out and wait for some acks.
        // make sure we send at least a minimum as we dont wont any stalling
        if (packetsSent >= mWindowSizeCurrent)
            break;

    }

    lk.unlock();


    // Handle any specific commands
    switch (mCommand)
    {
    case SCOM_Connect:
    {
        _processConnectCommand();
        break;
    }
    case SCOM_Disconnect:
    {
		DLOG(INFO) << "Handle Session Disconnect " << this->getId() <<" endcount " << endCount;
        _processDisconnectCommand();
        break;
    }

    default:
        break;
    } //end switch(mCommand)

    // Process our timeouts.

    if (mStatus == SSTAT_Connected)
    {
        int64 t = now - mLastPacketReceived;

        //!!! as receiving packets happens in the readthread and this code is executed in the write thread
        //it can happen that a packet was received AFTER the now is read out - especially when our thread gets interrupted at this point!!!!
        //to solve this we take an int instead an uint as lastpacket can be bigger than now :)

        //please leave this be - otherwise the tc will upset the testers as they dont
        //disconnect anymore
        if (t > 60000)
        {
            if(this->mServerService)
            {
                LOG(INFO) << "Session disconnect last received packet > 60  ("<< (float)t/1000 << ") seconds session Id : " << this->getId();
				LOG(INFO) << "Session lastpacket " << mLastPacketReceived << " now " << now << " diff :" << (now - mLastPacketReceived);
                mCommand = SCOM_Disconnect;
            }
            else
            {
                LOG(INFO) << "Session disconnect last received packet > 60  ("<< (float)t/1000 << ") seconds session Id : " << this->getId();

                mCommand = SCOM_Disconnect;
            }
        }
        else if (this->mServerService && (t > 20000))
        {
            if((now - mLastPingPacketSent) > 2000)
                _sendPingPacket();
        }
        //dont spend time here to often (calling mutexes and such)
	    if(now - mLastHouseKeepingTimeTime < 1000)    {
            if(!mCommand)
			    return;
        }

	    mLastHouseKeepingTimeTime = now;
    }

	//dont spend time here to often (calling mutexes and such)
	if(now - mLastHouseKeepingTimeTime < 1000)    {
		return;
    }

	mLastHouseKeepingTimeTime = now;

	//we might stall if the last packets get lost and the client wont generate ooo packets ( or those get lost)
    if(!this->mServerService )    {
        _resendData();
    }

}


//======================================================================================================================
void Session::SendChannelA(Message* message)
{

    message->mSession = this;
    //check whether we are disconnecting  this happens when a client or server crashes without sending a disconnect
    //however in these cases we get a lot of stuck messages on the heap which are orphaned
    if(mStatus != SSTAT_Connected)    {
        message->setPendingDelete(true);
        return;
    }

    //the connectionserver puts a lot of fastpaths here  - so just put them were they belong
    //this alone takes roughly 5% cpu off of the connectionserver
    if(message->getFastpath()&& (message->getSize() < mMaxUnreliableSize))	{
		if(mMessageFactory->getHeapsize() > 95.0)	{
			message->setPendingDelete(true);
			return;
		}
        mUnreliableMessageQueue.push(message);
	}    else    {
        message->setFastpath(false);	  //send it as reliable if its to big
		mReliableMessageQueue.push(message);
    }
}

void Session::SendChannelAUnreliable(Message* message)
{
    message->mSession = this;

    //check whether we are disconnecting
    if((mMessageFactory->getHeapsize() > 95.0) || (mStatus != SSTAT_Connected))
    {
        message->setPendingDelete(true);
        return;
    }

    if(message->getSize() > mMaxUnreliableSize)		{//I send the attribute messages as unreliables	 but they can be to big!!
        message->setFastpath(false);	  //send it as reliable if its to big
		
        mReliableMessageQueue.push(message);
    }
    else	{
        mUnreliableMessageQueue.push(message);
	}
}


void Session::SortSessionPacket(Packet* packet, uint16 type)
{
    packet->setReadIndex(2);
    switch (type)
    {
        // New connection request.

        // our user data channels
    case SESSIONOP_DataChannel2:
    {
        // All data channels get handled here.
        _processDataChannelB(packet);
        break;
    }
    case SESSIONOP_DataChannel1:
    case SESSIONOP_DataChannel3:
    case SESSIONOP_DataChannel4:
    {
        // All data channels get handled here.
        _processDataChannelPacket(packet, false);
        break;
    }

    case SESSIONOP_DataFrag2:
    {
        _processRoutedFragmentedPacket(packet);
        break;
    }

    // This is part of a message fragment.  Just add it to the packet queue as it must be handled further up.
    case SESSIONOP_DataFrag1:
    case SESSIONOP_DataFrag3:
    case SESSIONOP_DataFrag4:
    {
        // Handle our fragmented packets here.
        _processFragmentedPacket(packet);
        break;
    }


    default:
    {
        // Unknown SESSIONOP code
        DLOG(INFO) << "Destroying packet because!!! --tmr <3";
        mPacketFactory->DestroyPacket(packet);
        break;
    }
    }


}
//======================================================================================================================
void Session::HandleSessionPacket(Packet* packet)
{
    // Reset our packet read index and start parsing...
    packet->setReadIndex(0);
    uint16 packetType = packet->getUint16();

    // Set our last packet time index
    mLastPacketReceived = Anh_Utils::Clock::getSingleton()->getStoredTime();
    mClientPacketsReceived++;

    // If this is fastpath data, send it up. all fastpath data should go the other pathway
    if (packetType < 0x0100)    {
		assert(false);
        HandleFastpathPacket(packet);
        return;
    }

    //get rid of all the unsequenced stuff
    switch (packetType)
    {
        // New connection request.
    case SESSIONOP_SessionRequest:
    {
        // This is a new session request.
        _processSessionRequestPacket(packet);
        return;
    }
    // New connection request.
    case SESSIONOP_SessionResponse:
    {
        // This is the session response
        mCommand = SCOM_None;
        mStatus = SSTAT_Connected;
        packet->getUint32();                          // unkown
        mEncryptKey = ntohl(packet->getUint32());     // Encryption key
        mPacketFactory->DestroyPacket(packet);
        return;
    }

    case SESSIONOP_DataOrder1:
    case SESSIONOP_DataOrder2:
	case SESSIONOP_DataOrder3:
    case SESSIONOP_DataOrder4:
    {
        _processDataOrderPacket(packet);
        return;
    }

    // Multiple session packets
    case SESSIONOP_MultiPacket:
    {
        // itself has no sequence but its contained packets might   - we dont send reliables in multis
        // however they get handled through this function anyway
        _processMultiPacket(packet);
        return;
    }
    // Remote side disconnceted
    case SESSIONOP_Disconnect:
    {
        DLOG(INFO) << "Session::remote side disconnected";

        mStatus = SSTAT_Disconnecting;
        _processDisconnectPacket(packet);
        return;
    }

    // Data channel acks
    case SESSIONOP_DataAck1:
    case SESSIONOP_DataAck2:
    case SESSIONOP_DataAck3:
    case SESSIONOP_DataAck4:
    {
        _processDataChannelAck(packet);
        return;
    }


    // Ping, don't know what it's for yet.
    case SESSIONOP_Ping:
    {
        //_sendPingPacket();
        _processPingPacket(packet);
        return;
    }
    // Network Status.  Must return a proper packet or network layer stops working.
    case SESSIONOP_NetStatRequest:
    {
        _processNetStatRequestPacket(packet);
        return;
    }

	case SESSIONOP_DataChannel1:
	case SESSIONOP_DataChannel2:
	case SESSIONOP_DataChannel3:
	case SESSIONOP_DataChannel4:
	{
		//now we have got ourselves a sequenced packet
		//check if we are in sequence
		uint16 sequence = ntohs(packet->getUint16());

		if (mInSequenceNext == sequence)    {
			SortSessionPacket(packet,packetType);
		}    else    {
        
			//were missing something
			DLOG(INFO) << "Handle Session Packet :: Incoming data - seq: " << sequence << "expect: " << mInSequenceNext
			<< "Session:0x"<< mService->getId() << getId() << "4x";

			Packet* orderPacket;
			orderPacket = mPacketFactory->CreatePacket();
			orderPacket->addUint16(SESSIONOP_DataOrder1);
			orderPacket->addUint16(htons(sequence));
			orderPacket->setIsCompressed(false);
			orderPacket->setIsEncrypted(true);

			_addOutgoingUnreliablePacket(orderPacket);

			mPacketFactory->DestroyPacket(packet);

		}
	
	}
    }

    
    
}


//======================================================================================================================

void Session::HandleFastpathPacket(Packet* packet)
{

    uint8		priority		= 0;
    uint8		routed			= 0;
    uint8		dest			= 0;
    uint32	accountId		= 0;

    // Fast path is raw data.  Just send it up.
    mLastPacketReceived = Anh_Utils::Clock::getSingleton()->getStoredTime();

    packet->setReadIndex(0);

    priority = packet->getUint8();

    routed = packet->getUint8();
    if (routed)    {
        dest = packet->getUint8();
        accountId = packet->getUint32();
    }

	//make sure we dont crush our heap when busy
	//reliables can be easily spared
	if(mMessageFactory->getHeapsize() >= 95.0)	{
		//assert(false);
		mPacketFactory->DestroyPacket(packet);
		return;
	}


    // Create our message and send it up.
	Message* newMessage;
    mMessageFactory->StartMessage();

    if (routed)    {
        mMessageFactory->addData(packet->getData() + 7, packet->getSize() - 7); // +2 priority/routed, +5 routing header
		newMessage = mMessageFactory->EndMessage();
		newMessage->setPriority(priority);
		newMessage->setDestinationId(dest);
		newMessage->setAccountId(accountId);
		newMessage->setFastpath(true);
        newMessage->setRouted(true);
    }
    else    {
        mMessageFactory->addData(packet->getData() + 2, packet->getSize() - 2); // +2 priority/routed
		newMessage = mMessageFactory->EndMessage();
		newMessage->setPriority(priority);
		newMessage->setDestinationId(dest);
		newMessage->setAccountId(accountId);
		newMessage->setFastpath(true);
        newMessage->setRouted(false);
    }

    _addIncomingMessage(newMessage, priority);

    mPacketFactory->DestroyPacket(packet);
    //_processDataChannelPacket(packet, true);
}

//======================================================================================================================

void Session::DestroyIncomingMessage(Message* message)
{
    message->setPendingDelete(true);
}

//======================================================================================================================

void Session::DestroyPacket(Packet* packet)
{
    mPacketFactory->DestroyPacket(packet);
}

//======================================================================================================================

bool Session::getOutgoingReliablePacket(Packet*& packet)
{
	Packet* p = nullptr;
    if (!mOutgoingReliablePacketQueue.pop(p))	{
		packet = p;
		return false;
	}

    mServerPacketsSent++;
    mLastPacketSent = Anh_Utils::Clock::getSingleton()->getStoredTime();

	packet = p;

    return true;
}



//======================================================================================================================
bool Session::getOutgoingUnreliablePacket(Packet*& packet)	{
	packet = nullptr;
	return(mOutgoingUnreliablePacketQueue.pop(packet));
	/*
    Packet* p = nullptr;
    if (!mOutgoingUnreliablePacketQueue.pop(p))	{
		packet = p;
		return false;
	}

    mServerPacketsSent++;
    mLastPacketSent = Anh_Utils::Clock::getSingleton()->getStoredTime();

	packet = p;

    return true;*/
}


//======================================================================================================================
bool Session::getIncomingQueueMessage(Message*& message)
{
    message = nullptr;
	return(mIncomingMessageQueue.pop(message));
}


//======================================================================================================================
void Session::_processSessionRequestPacket(Packet* packet)
{

    // retrieve our request id.
    packet->getUint32();                      // Unknown
    mRequestId = packet->getUint32();         // Request id.

    // Get a new encryption key if we're not already initilized.
    // If we're not a new session, then we should drop the packet.
    if (mStatus == SSTAT_Initialize)
    {
        seed_rand_mwc1616(static_cast<uint32>(Anh_Utils::Clock::getSingleton()->getLocalTime()));
        mEncryptKey = rand_mwc1616();
    }

    // Build a SessionResponse packet here.
    Packet* newPacket = mPacketFactory->CreatePacket();
    newPacket->addUint16(SESSIONOP_SessionResponse);      // Session packet type
    newPacket->addUint32(mRequestId);
    newPacket->addUint32(htonl(mEncryptKey));
    newPacket->addUint8(2);
    newPacket->addUint8(1);
    newPacket->addUint8(4);
    newPacket->addUint32(htonl(mMaxPacketSize));
    newPacket->setIsCompressed(false);
    newPacket->setIsEncrypted(false);

    // This session is in the process of connecting
    mStatus = SSTAT_Connecting;

    // Push the packet on our outgoing queue
    _addOutgoingUnreliablePacket(newPacket);
    mService->AddSessionToProcessQueue(this);

    // Destroy our incoming packet, it's not needed any longer.
    mPacketFactory->DestroyPacket(packet);
}


//======================================================================================================================
void Session::_processDisconnectPacket(Packet* packet)
{
    // Let the application know there was a disconnect.
    mService->AddSessionToProcessQueue(this);

    // Destroy our incoming packet, it's not needed any longer.
    mPacketFactory->DestroyPacket(packet);
}


//======================================================================================================================
void Session::_processMultiPacket(Packet* packet)
{
    Packet* newPacket = 0;
    uint16 packetSize = 0;

    // Iterate through our multi-packet

    while (packet->getReadIndex() < packet->getSize())
    {
        // Create a new packet
        newPacket = mPacketFactory->CreatePacket();

        // Get the size of our next packet and increment our index
        packetSize = packet->getUint8();

        // insert the data.
        newPacket->Reset();
        newPacket->addData(&(packet->getData()[packet->getReadIndex()]), packetSize);
        packet->setReadIndex(packet->getReadIndex() + packetSize);
        // Process the new packet.
        this->HandleSessionPacket(newPacket);
    }

    // Destroy our incoming packet, it's not needed any longer.
    mPacketFactory->DestroyPacket(packet);
}



void Session::_processDataChannelPacket(Packet* packet, bool fastPath)
{
    uint8		priority		= 0;
    uint8		routed			= 0;
    uint8		dest			= 0;
    uint32	accountId		= 0;
    bool		destroyPacket	= true;



    // Otherwise ack this packet then send it up
    packet->setReadIndex(0);
    packet->getUint16(); // packet type
    packet->getUint16(); // sequence

    // check to see if this is a multi-message message
    //uint16 len = packet->getSize() - 4;  // -2 header, -2 sequence
    priority = packet->getUint8();

    routed = packet->getUint8();

    // If we're from the server, strip off our routing header.
    if (routed == 0x01)
    {
        dest = packet->getUint8();
        accountId = packet->getUint32();
    }

    if (routed == 0x19)
    {
        // Next byte is size
        uint32 size = packet->getUint8();

        do
        {
            // More size bytes?
            if (size == 0xff)
            {
                size = ntohs(packet->getUint16());
            }

            // We need to get theses again.
            priority = packet->getUint8();
            packet->getUint8();  // Routing byte not used inside a multi-message, just dump it. (only for channel A!!!)

            // Init a new message for this data.
            mMessageFactory->StartMessage();
            mMessageFactory->addData(packet->getData() + packet->getReadIndex(), static_cast<uint16>(size) - 2); // -1 priority, -1 routing
            Message* newMessage = mMessageFactory->EndMessage();

            // set our account and server id's
            newMessage->setAccountId(accountId);
            newMessage->setDestinationId(dest);
            newMessage->setPriority(priority);

            // Need to specify whether this is routed or not here, so we know in the app
            newMessage->setRouted(false);

            // Push the message on our incoming queue
            _addIncomingMessage(newMessage, priority);

            // Advance the message index
            packet->setReadIndex(packet->getReadIndex() + static_cast<uint16>(size) - 2); // -1 priority, -1 routing

            size = packet->getUint8();
        }
        while (packet->getReadIndex() < packet->getSize() && size != 0);//do while
    }
    else
    {
        // Create our new message and send it up.
        mMessageFactory->StartMessage();
        mMessageFactory->addData(packet->getData() + packet->getReadIndex(), packet->getSize() - packet->getReadIndex());
        Message* newMessage = mMessageFactory->EndMessage();

        // Need to specify whether this is routed or not here, so we know in the app
        if (routed)
            newMessage->setRouted(true);
        else
            newMessage->setRouted(false);

        // set our message parameters
        newMessage->setAccountId(accountId);
        newMessage->setDestinationId(dest);
        newMessage->setPriority(priority);

        // Push the message on our incoming queue
        _addIncomingMessage(newMessage, priority);
    }

    // Set our inSequence number so we know we recieved this packet.
    // in sequence is per packet not per message
    // with the one exception of 03's

    mInSequenceNext++;
    mSendDelayedAck = true;



    // Destroy our incoming packet, it's not needed any longer.
    if(destroyPacket)
        mPacketFactory->DestroyPacket(packet);
}


//======================================================================================================================
//Data Channel B is used for interserver communication (ie of routed Packets)
//this way we are able to pass around zipping
//======================================================================================================================

void Session::_processDataChannelB(Packet* packet)
{
    uint8		priority		= 0;
    uint8		routed			= 0;
    uint8		dest			= 0;
    uint32	accountId		= 0;



    // Otherwise ack this packet then send it up
    packet->setReadIndex(0);
    packet->getUint16();   // packet type
    packet->getUint16(); // sequence

    // check to see if this is a multi-message message

    priority = packet->getUint8();

    routed = packet->getUint8();

    //this is a nultimessage ??
    if (routed== 0x19)//routed
    {
        // Next byte is size
        uint32 size = packet->getUint8();
        do
        {
            // More size bytes?
            if (size == 0xff)
            {
                //right now max size 255 (1 byte!!!)
                size = ntohs(packet->getUint16());
            }

            // We need to get these again.
            priority = packet->getUint8();

            routed =  packet->getUint8();  // Routing byte in a B 019 used!!! as we have interserver communication here

            dest = packet->getUint8();
            accountId = packet->getUint32();

            // Init a new message for this data.
            mMessageFactory->StartMessage();

            //type 2 sequence 2 priority 1 routed 1 routing header 5 = 11
            //type 2 sequence 2 priority 1 routed 1 size 1 (or 3) routing header 5 = 12 or 14
            //but size - 7	(priority + routed + routing header)
            mMessageFactory->addData(packet->getData() + packet->getReadIndex(), static_cast<uint16>(size) - 7); // -1 priority, -1 routing and routing header
            Message* newMessage = mMessageFactory->EndMessage();

            // set our account and server id's
            newMessage->setAccountId(accountId);
            newMessage->setDestinationId(dest);
            newMessage->setPriority(priority);

            // Need to specify whether this is routed or not here, so we know in the app
            newMessage->setRouted(true);

            // Push the message on our incoming queue
            _addIncomingMessage(newMessage, priority);

            // Advance the message index
            packet->setReadIndex(packet->getReadIndex() + static_cast<uint16>(size) - 7); // -1 priority, -1 routing

            size = packet->getUint8();

        }
        while ((packet->getReadIndex() < packet->getSize()) && size != 0);

    }
    else // no 019
    {
        dest = packet->getUint8();
        accountId = packet->getUint32();

        // Create our new message and send it up.
        mMessageFactory->StartMessage();
        mMessageFactory->addData(packet->getData() + packet->getReadIndex(), packet->getSize() - packet->getReadIndex());
        Message* newMessage = mMessageFactory->EndMessage();

        newMessage->setRouted(true);



        // set our message parameters
        newMessage->setAccountId(accountId);
        newMessage->setDestinationId(dest);

        newMessage->setPriority(priority);

        // Push the message on our incoming queue
        _addIncomingMessage(newMessage, priority);

    }

    // Set our inSequence number so we know we recieved this packet.
    // in sequence is per packet not per message
    mInSequenceNext++;
    mSendDelayedAck = true;



    // Destroy our incoming packet, it's not needed any longer.

    mPacketFactory->DestroyPacket(packet);
}




//======================================================================================================================
void Session::_processDataChannelAck(Packet* packet)
{
    Packet* windowPacket = 0;
    uint16 window_sequence = 0;

    PacketWindowList::iterator iter;
    PacketWindowList::iterator itN;

    // Get the sequence off our incoming packet
    packet->setReadIndex(2);  //skip the header
    uint16 ack_sequence = ntohs(packet->getUint16());//20
	uint16 ack_window = 0;
	uint16 low_ack_window = 0;
	//access the writethreads packetwindowlist
    boost::recursive_mutex::scoped_lock lk(mSessionMutex);

    if (mWindowPacketList.size() == 0)    {
        //gLogger->log(LogManager::DEBUG,"Dupe ACK received - Nothing in resend window - seq: %u, Session:0x%x%.4x", sequence, mService->getId(), getId());
        mPacketFactory->DestroyPacket(packet);
        return;
    }

    iter = mWindowPacketList.begin();
    windowPacket = *iter;
    windowPacket->setReadIndex(2); //skip the header
    window_sequence = ntohs(windowPacket->getUint16());//10
	//5              65520                 20
	ack_window		= window_sequence + mWindowPacketList.size();
	uint16 list_size = mWindowPacketList.size();

	if ((ack_sequence < window_sequence ) && (ack_sequence > ack_window))	{
		
		LOG(WARNING) << "Session::_processDataChannelAck:: Invalid Ack : AckSequence " <<ack_sequence<<" window_sequence " << window_sequence << " list size " << list_size;
		//invalid ack bail out
		mPacketFactory->DestroyPacket(packet);
        return;
	}

	if ((ack_sequence < window_sequence ) && ((ack_sequence + list_size) < ack_window))	{
		LOG(WARNING) << "Session::_processDataChannelAck:: Invalid Ack : AckSequence " <<ack_sequence<<" window_sequence " << window_sequence << " list size " << list_size;
		//invalid ack bail out
		mPacketFactory->DestroyPacket(packet);
        return;
	}

	if ((ack_sequence > window_sequence ) && (ack_sequence > ack_window))	{
		LOG(WARNING) << "Session::_processDataChannelAck:: Invalid Ack : AckSequence " <<ack_sequence<<" window_sequence " << window_sequence << " list size " << list_size;
		//invalid ack bail out
		mPacketFactory->DestroyPacket(packet);
        return;
	}

    LOG(WARNING) << "Session::_processDataChannelAck:: Proper Ack : AckSequence " <<ack_sequence<<" window_sequence " << window_sequence << " list size " << list_size;
    
    // This is a proper ack, so handle it.
    //open our datachannel - we have connection!
	if(mWindowSizeCurrent < mWindowResendSize)    {
        mWindowSizeCurrent += uint32(mWindowResendSize/10); //resize the window for 10%
        if(mWindowSizeCurrent >mWindowResendSize)
            mWindowSizeCurrent = mWindowResendSize;
    }
	//       20              30
    while (window_sequence != ack_sequence)    {
        mPacketFactory->DestroyPacket(*iter);
        iter = mWindowPacketList.erase(iter);

        // If the window is empty, break out
        if (iter == mWindowPacketList.end())
            break;

        windowPacket = *iter;
        windowPacket->setReadIndex(2); //skip the header
        window_sequence = ntohs(windowPacket->getUint16());
    }

    mLastRemotePacketAckReceived = Anh_Utils::Clock::getSingleton()->getStoredTime();


    // Destroy our incoming packet, it's not needed any longer.
    mPacketFactory->DestroyPacket(packet);
}


//======================================================================================================================
void Session::_processDataOrderPacket(Packet* packet)
{

    boost::recursive_mutex::scoped_lock lk(mSessionMutex); // mRolloverWindowPacketList and WindowPacketList get accessed by the socketwritethread and by the socketreadthread both through the session

    packet->setReadIndex(2);
    uint16 failure_sequence = ntohs(packet->getUint16());

    PacketWindowList::iterator	iter			= mWindowPacketList.begin();

    // If the window packet list is empty just bail out now.
    if (iter == mWindowPacketList.end()) {
        return;
    }

    Packet*						windowPacket	= *iter;

    windowPacket->setReadIndex(2);
    uint16 window_sequence = ntohs(windowPacket->getUint16());


	uint16 ack_window = window_sequence + mWindowPacketList.size();

	LOG(WARNING) << "Out-Of-order packet session 0x"<< mService->getId() << mId <<" seq: " << failure_sequence <<" windowsequ : " << window_sequence;

	// basic bound check for validity of the sequence
   
	
	uint16 list_size = mWindowPacketList.size();

	if ((failure_sequence < window_sequence ) && (failure_sequence > ack_window))	{
		
		LOG(WARNING) << "Session::_processDataOrderPacket:: Invalid Ack : AckSequence " <<failure_sequence<<" window_sequence " << window_sequence << " list size " << list_size;
		//invalid ack bail out
		mPacketFactory->DestroyPacket(packet);
        return;
	}

	if ((failure_sequence < window_sequence ) && ((failure_sequence + list_size) < ack_window))	{
		LOG(WARNING) << "Session::_processDataOrderPacket:: Invalid Ack : AckSequence " <<failure_sequence<<" window_sequence " << window_sequence << " list size " << list_size;
		//invalid ack bail out
		mPacketFactory->DestroyPacket(packet);
        return;
	}

	if ((failure_sequence > window_sequence ) && (failure_sequence > ack_window))	{
		LOG(WARNING) << "Session::_processDataOrderPacket:: Invalid Ack : AckSequence " <<failure_sequence<<" window_sequence " << window_sequence << " list size " << list_size;
		//invalid ack bail out
		mPacketFactory->DestroyPacket(packet);
        return;
	}

	if (mWindowSizeCurrent > (mWindowResendSize/10))//keep at least a 10% window going
		mWindowSizeCurrent--;

	uint64 localTime = Anh_Utils::Clock::getSingleton()->getLocalTime();//we need to be more precise here than 1/1sec

	//only resend every 10ms or so - we want to avoid flooding
	if(localTime - windowPacket->getTimeOOHSent() < 10)        {
		mPacketFactory->DestroyPacket(packet);
        return;
	}

	uint16 amount_send = 0;
    
    for (iter = mWindowPacketList.begin(); iter != mWindowPacketList.end(); iter++)    {
        // Grab our window packet
        windowPacket = (*iter);
        windowPacket->setReadIndex(2);
        windowPacket->getUint16(); // windowsequence ?

        // If it's smaller/bigger (keep the Rollover in mind) than the order packet send it, otherwise break;
		// only send every 10ms or so we dont want to drown the receiver in packetbursts
    
        _addOutgoingReliablePacket(windowPacket);

		amount_send++;
		if(amount_send > mWindowResendSize)	{
			LOG(WARNING) << "Session::_processDataOrderPacket : Throttled resends to : " << mWindowResendSize << " Packets";
			break;
		}

        windowPacket->setTimeOOHSent(localTime);

        if (mWindowSizeCurrent > (mWindowResendSize/10))
            mWindowSizeCurrent--;

    }

    // Destroy our incoming packet, it's not needed any longer.
    mPacketFactory->DestroyPacket(packet);

	// the idea is to send all we have *in the hope* that it is received
	// as we otherwise just produce even more out of order packets because of the gap between the resend and the window list
	// it *might* be an idea to reduce the amount nevertheless, as we dont want to drown the receiver in packetbursts
}

//======================================================================================================================
//
// resend packets in case we stall due to packetloss of the Ack or oop (happens regularly if rarely)
// the waittime is set to 700ms which might seem long, but we do not want to flood the line with unnecessarily (double)send
// packets
//

void Session::_resendData()
{

    boost::recursive_mutex::scoped_lock lk(mSessionMutex); // mRolloverWindowPacketList and WindowPacketList get accessed by the socketwritethread and by the socketreadthread both through the session

    PacketWindowList::iterator	iter			= mWindowPacketList.begin();

    Packet*		windowPacket;

    // If the window packet list is empty just bail out now.
    if (mWindowPacketList.size() == 0)    {
        return;
    }

    uint64 localTime = Anh_Utils::Clock::getSingleton()->getLocalTime();
    uint64 waitTime = 0;
    uint64 oooTime = 0;
    uint32 packetsSend = 0;

    for (iter = mWindowPacketList.begin(); iter != mWindowPacketList.end(); iter++)    {
        windowPacket = (*iter);

        waitTime = localTime - windowPacket->getTimeSent();
        oooTime = localTime - windowPacket->getTimeOOHSent();
        
		//bail out
		if((waitTime < 700) || (oooTime < 700))	{
			return;
		}

		if(packetsSend > mWindowResendSize)	{
			return;
		}

		windowPacket->setTimeOOHSent(localTime);
        _addOutgoingReliablePacket(windowPacket);
        packetsSend++;
        
    }

}

//======================================================================================================================
void Session::_processFragmentedPacket(Packet* packet)
{
    packet->setReadIndex(2); //skip the header
    uint16 sequence = ntohs(packet->getUint16());
    uint8 priority = 0;
    uint8 routed = 0;
    uint8 dest = 0;
    uint32 accountId = 0;

    //if (mService->getId() == 1)
    {
    }

    // check our sequence number.
    if (sequence < mInSequenceNext)
    {
        // This is a duplicate packet that we've already recieved.
        LOG(INFO) << "Duplicate Fragged Packet Recieved.  seq: " << sequence;

        // Destroy our incoming packet, it's not needed any longer.
        mPacketFactory->DestroyPacket(packet);
        return;
    }
    else if (sequence > mInSequenceNext)
    {
		LOG(INFO) << "Fragged packet received out of order - expect: " <<mInSequenceNext << " received: " << sequence;

        mPacketFactory->DestroyPacket(packet);
        return;
    }

    // Inc our in seq
    mInSequenceNext++;

    // Need to send out acks
    mSendDelayedAck = true;

    // If we are not already processing a multi-packet message, start to.
    if (mFragmentedPacketTotalSize == 0)
    {
        mFragmentedPacketTotalSize = ntohl(packet->getUint32());

        mFragmentedPacketCurrentSize = packet->getSize() - 8;  // -2 header, -2 sequence, -4 size - crc compflag have already been removed
        mFragmentedPacketCurrentSequence = mFragmentedPacketStartSequence = sequence;

        packet->setReadIndex(8);	//2opcode, 2 sequence and 4 size
        priority = packet->getUint8();

        if (priority > 0x10)
        {
            // the packet has had a proper sequence .. otherwise we wouldnt be here ...
           LOG(WARNING) << "Start incoming fragged packets - total: " << mFragmentedPacketTotalSize << " seq: " << sequence;

        }

        // Now push the packet into our fragmented queue
        mIncomingFragmentedPacketQueue.push(packet);
    }
    // This is the next packet in the multi-packet sequence.
    else
    {
        mFragmentedPacketCurrentSize += packet->getSize() - 4;  // -2 header, -2 sequence
        mFragmentedPacketCurrentSequence = sequence;

        //if (mService->getId() == 1)
        {
        }

        // If this is our last packet, send them all up to the application
        if (mFragmentedPacketCurrentSize >= mFragmentedPacketTotalSize)
        {
            mIncomingFragmentedPacketQueue.push(packet);
            Packet* fragment = 0;

            // Build the message from the fragmented packet here and send it up
            mMessageFactory->StartMessage();
            uint32 fragmentCount = mIncomingFragmentedPacketQueue.size();
            for (uint32 i = 0; i < fragmentCount; i++)
            {
                fragment = mIncomingFragmentedPacketQueue.front();
                mIncomingFragmentedPacketQueue.pop();

                // if this is the first packet, make sure to skip the size.
                if (i == 0)
                {
                    fragment->setReadIndex(8);	//2opcode, 2 sequence and 4 size
                    priority = fragment->getUint8();
                    routed = fragment->getUint8();

                    if (routed)
                    {
                        dest = fragment->getUint8();
                        accountId = fragment->getUint32();
                        mMessageFactory->addData(fragment->getData() + 15, fragment->getSize() - 15); // -2 header, -2 sequence, -4 size, -2 priority/routing, -5 routing header
                    }
                    else
                    {
                        mMessageFactory->addData(fragment->getData() + 10, fragment->getSize() - 10); // -2 header, -2 sequence, -4 size, -2 priority/routing
                    }
                }
                // This is just additional data
                else
                {
                    mMessageFactory->addData(fragment->getData() + 4, fragment->getSize() - 4); // -2 header, -2 sequence
                }

                // delete our fragment
                mPacketFactory->DestroyPacket(fragment);
            }
            Message* newMessage = mMessageFactory->EndMessage();

            // Set our message variables.
            if (routed)
                newMessage->setRouted(true);
            else
                newMessage->setRouted(false);

            newMessage->setPriority(priority);
            newMessage->setDestinationId(dest);
            newMessage->setAccountId(accountId);

            if (priority > 0x10)
            {
                LOG(INFO) << "Fragmented Packet priority messup!!!";
                return;
            }


            // Push the message on our incoming queue
            _addIncomingMessage(newMessage, priority);

            // Clear our size counters
            mFragmentedPacketTotalSize = 0;
            mFragmentedPacketCurrentSize = 0;
            mFragmentedPacketCurrentSequence = 0;
            mFragmentedPacketStartSequence = 0;
        }
        // This is just the next packet in sequence.  Throw it on the queue
        else
        {
            mIncomingFragmentedPacketQueue.push(packet);
        }

    }
}

//======================================================================================================================
void Session::_processRoutedFragmentedPacket(Packet* packet)
{
    packet->setReadIndex(2); //skip the header

    uint16 sequence = ntohs(packet->getUint16());

    uint8 priority = 0;
    uint8 dest = 0;
    uint32 accountId = 0;


    // Inc our in seq
    mInSequenceNext++;

    // Need to send out acks
    mSendDelayedAck = true;

    // If we are not already processing a multi-packet message, start to.
    if (mRoutedFragmentedPacketTotalSize == 0)
    {
        mRoutedFragmentedPacketTotalSize = ntohl(packet->getUint32());

        mRoutedFragmentedPacketCurrentSize = packet->getSize() - 8;  // -2 header, -2 sequence, -4 size -
        mRoutedFragmentedPacketCurrentSequence = mRoutedFragmentedPacketStartSequence = sequence;

        // Now push the packet into our fragmented queue
        mIncomingRoutedFragmentedPacketQueue.push(packet);
    }
    // This is the next packet in the multi-packet sequence.
    else
    {
        mRoutedFragmentedPacketCurrentSize += packet->getSize() - 4;  // -2 header, -2 sequence

        mRoutedFragmentedPacketCurrentSequence = sequence;

        // If this is our last packet, send them all up to the application
        if (mRoutedFragmentedPacketCurrentSize == mRoutedFragmentedPacketTotalSize)
        {
            mIncomingRoutedFragmentedPacketQueue.push(packet);
            Packet* fragment = 0;

            // Build the message from the fragmented packet here and send it up
            mMessageFactory->StartMessage();

            uint32 fragmentCount = mIncomingRoutedFragmentedPacketQueue.size();

            for (uint32 i = 0; i < fragmentCount; i++)
            {
                fragment = mIncomingRoutedFragmentedPacketQueue.front();
                mIncomingRoutedFragmentedPacketQueue.pop();

                // if this is the first packet, make sure to skip the size.
                if (i == 0)
                {
                    fragment->setReadIndex(8);	//2opcode, 2 sequence and 4 size

                    priority = fragment->getUint8();
                    fragment->getUint8(); // routed
                    dest = fragment->getUint8();
                    accountId = fragment->getUint32();

                    mMessageFactory->addData(fragment->getData() + 15, fragment->getSize() - 15); // -2 header, -2 sequence, -4 size, -2 priority/routing, -5 routing header

                }
                // This is just additional data
                else
                {
                    mMessageFactory->addData(fragment->getData() + 4, fragment->getSize() - 4); // -2 header, -2 sequence
                }

                // delete our fragment
                mPacketFactory->DestroyPacket(fragment);
            }
            Message* newMessage = mMessageFactory->EndMessage();

            // Set our message variables.

            newMessage->setRouted(true);
            newMessage->setPriority(priority);

            if (priority > 0x10)
            {

                LOG(INFO) << "Fragmented Packet priority messup!!!";
                return;
            }

            newMessage->setDestinationId(dest);
            newMessage->setAccountId(accountId);

            // Push the message on our incoming queue
            _addIncomingMessage(newMessage, priority);

            // Clear our size counters
            mRoutedFragmentedPacketTotalSize = 0;
            mRoutedFragmentedPacketCurrentSize = 0;
            mRoutedFragmentedPacketCurrentSequence = 0;
            mRoutedFragmentedPacketStartSequence = 0;
        }
        // This is just the next packet in sequence.  Throw it on the queue
        else
        {
            mIncomingRoutedFragmentedPacketQueue.push(packet);
        }

    }
}

//======================================================================================================================
void Session::_processPingPacket(Packet* packet)
{

    //with the old code *every* ping caused a new ping
    //so once pinged we kept pinging.
    //when we then decided to add a ping we just doubled the pinging
    //as fast as the servers possibly could spam packets

    if((Anh_Utils::Clock::getSingleton()->getStoredTime() - mLastPingPacketSent) < 1000)
    {
        mPacketFactory->DestroyPacket(packet);
        return;
    }

    // Client sends a simple 5 byte ping.
    if (packet->getSize() == 5)
    {
        // Echo the ping packet back.
        Packet* newPacket = mPacketFactory->CreatePacket();
        newPacket->addUint16(SESSIONOP_Ping);

        newPacket->setIsCompressed(false);
        newPacket->setIsEncrypted(true);

        // Push the packet on our outgoing queue
        _addOutgoingUnreliablePacket(newPacket);
        mLastPingPacketSent = Anh_Utils::Clock::getSingleton()->getStoredTime();
    }
    // Backend servers are larger to incorporate more features, 9 bytes(packet size).
    else
    {
        uint32 pingType = packet->getUint32();

        if (pingType == 1) // ping request
        {
            // Echo the ping packet back.
            Packet* newPacket = mPacketFactory->CreatePacket();
            newPacket->addUint16(SESSIONOP_Ping);
            newPacket->addUint32(2);    // ping response

            newPacket->setIsCompressed(false);
            newPacket->setIsEncrypted(true);

            // Push the packet on our outgoing queue
            _addOutgoingUnreliablePacket(newPacket);
            mLastPingPacketSent = Anh_Utils::Clock::getSingleton()->getStoredTime();
        }
    }

    // Destroy our incoming packet, it's not needed any longer.
    mPacketFactory->DestroyPacket(packet);
}


//======================================================================================================================
void Session::_processNetStatRequestPacket(Packet* packet)
{
    uint16 tick = packet->getUint16();

    mLastRoundtripTime        = packet->getUint32();
    mAverageRoundtripTime     = packet->getUint32();
    mShortestRoundtripTime    = packet->getUint32();
    mLongestRoundtripTime     = packet->getUint32();
    packet->getUint32();

    mClientPacketsSent        = packet->getUint64();
    mClientPacketsReceived    = packet->getUint64();

    uint64 clientSent         = mClientPacketsSent;
    uint64 clientReceived     = mClientPacketsReceived;
    uint64 serverSent         = mClientPacketsReceived;//mServerPacketsSent;
    uint64 serverReceived     = mClientPacketsSent;//mServerPacketsReceived;


    //clientSent = swap64(clientSent);
    //clientReceived = swap64(clientReceived);
    //serverSent = swap64(serverSent);
    //serverReceived = swap64(serverReceived);


    Packet* newPacket = mPacketFactory->CreatePacket();
    newPacket->addUint16(SESSIONOP_NetStatResponse);
    newPacket->addUint16(tick);
    newPacket->addUint32(htonl(static_cast<uint32>(Anh_Utils::Clock::getSingleton()->getLocalTime()) + tick));
    newPacket->addUint64(clientSent);
    newPacket->addUint64(clientReceived);
    newPacket->addUint64(serverSent);
    newPacket->addUint64(serverReceived);

    // Set our compression and encryption flags
    newPacket->setIsCompressed(false);
    newPacket->setIsEncrypted(true);

    // Push the packet on our outgoing queue
    _addOutgoingUnreliablePacket(newPacket);

    // Destroy our incoming packet, it's not needed any longer.
    mPacketFactory->DestroyPacket(packet);
}


//======================================================================================================================
void Session::_sendPingPacket(void)
{
    mLastPingPacketSent = Anh_Utils::Clock::getSingleton()->getLocalTime();

    // Create a new ping packet and send it on.
    Packet* packet = mPacketFactory->CreatePacket();
    packet->addUint16(SESSIONOP_Ping);
    packet->addUint32(1);       // ping request

    // Set our compression and encryption flags
    packet->setIsCompressed(false);
    packet->setIsEncrypted(true);

    // Push the packet on our outgoing queue
    _addOutgoingUnreliablePacket(packet);
}


//======================================================================================================================
void Session::_processConnectCommand(void)
{
    // If we havne't started connecting yet, then do so
    if (mStatus == SSTAT_Initialize)
    {
        // Update our status
        mStatus = SSTAT_Connecting;
        mConnectStartEvent = Anh_Utils::Clock::getSingleton()->getLocalTime();
        mLastConnectRequestSent = 0;
        LOG(INFO) << "Attempting to make a connection at [" << mConnectStartEvent << "]";
    }

    // Otherwise, see if we need to send another request packet, or if our timeout expired
    if (mStatus == SSTAT_Connecting && mCommand == SCOM_Connect)
    {
        if (Anh_Utils::Clock::getSingleton()->getLocalTime() - mLastConnectRequestSent > 5000)  // Send a request packet every 5 seconds
        {
            // If we hit our timeout, then cancel the connect
            if (Anh_Utils::Clock::getSingleton()->getLocalTime() - mConnectStartEvent > 60000)   // Timeout at 30 seconds
            {
                // Cancel our connect command and put us in a back in an uninit state
                mStatus = SSTAT_Initialize;
                mCommand = SCOM_None;
            }
            else
            {
                mLastConnectRequestSent = Anh_Utils::Clock::getSingleton()->getLocalTime();

                LOG(INFO) << "Sending session request";

                // Build a session request packet and send it.
                Packet* newPacket = mPacketFactory->CreatePacket();
                newPacket->addUint16(SESSIONOP_SessionRequest);
                newPacket->addUint16((uint16)htonl(2));
                newPacket->addUint32(htonl(37563635));
                newPacket->addUint32(htonl(mMaxPacketSize));

                newPacket->setIsCompressed(false);
                newPacket->setIsEncrypted(false);

                // Send out packet out.
                _addOutgoingUnreliablePacket(newPacket);
            }
        }
    }

}


//======================================================================================================================
void Session::_processDisconnectCommand(void)
{
    // Set our status and clear the command
    mStatus = SSTAT_Disconnecting;
    mCommand = SCOM_None;

    // Send a disconnect packet
    Packet* newPacket = mPacketFactory->CreatePacket();
    newPacket->addUint16(SESSIONOP_Disconnect);
    newPacket->addUint32(mRequestId);
    newPacket->addUint16(0x0006);

    newPacket->setIsCompressed(false);
    newPacket->setIsEncrypted(true);

    mService->AddSessionToProcessQueue(this);

    DLOG(INFO) << "Disconnect Command added session to processqueue";

    // Send out packet out.
    _addOutgoingUnreliablePacket(newPacket);
}


//======================================================================================================================
void Session::_addOutgoingMessage(Message* message, uint8 priority, bool fastpath)
{

}


//======================================================================================================================
void Session::_addIncomingMessage(Message* message, uint8 priority)
{
    // Incomings wont know about priority
    //assert(priority < 0x10);
    mIncomingMessageQueue.push(message);

    // Let the service know we need to be processed.
    mService->AddSessionToProcessQueue(this);
}



//======================================================================================================================
void Session::_buildOutgoingReliableRoutedPackets(Message* message)
{
    Packet*	newPacket = 0;
    uint16	messageIndex = 0;
    uint16	envelopeSize = 0;
    uint16	messageSize = message->getSize();

    // fragments envelope sizes
    envelopeSize = 18; // -2 header -2 seq -4 size -1 priority -1 routed flag -1 route destination -4 account id -3 comp/CRC

    // If we're too large to fit in a single packet, split us up.
    if(messageSize + envelopeSize > mMaxPacketSize)    {

        // Build our first packet with the total size.
        newPacket = mPacketFactory->CreatePacket();

        newPacket->addUint16(SESSIONOP_DataFrag2);

        newPacket->addUint16(htons(mOutSequenceNext));

        newPacket->addUint32(htonl(messageSize + 7));

        newPacket->addUint8(message->getPriority());

        newPacket->addUint8(1);                               // There is a routing header next
        newPacket->addUint8(message->getDestinationId());
        newPacket->addUint32(message->getAccountId());
        newPacket->addData(message->getData(), mMaxPacketSize- envelopeSize); // -2 header, -2 sequence, -4 size, -2 priority/routing, -5 routing, -2 crc
        messageIndex += mMaxPacketSize - envelopeSize;                         // -2 header, -2 sequence, -4 size, -2 priority/routing, -5 routing, -2 crc


        // Data channels need compression and encryption
        // no compression in server server communication!
        newPacket->setIsCompressed(false);
        newPacket->setIsEncrypted(true);

        // Push the packet on our outgoing queue
        mNewWindowPacketList.push(newPacket);

        // Now build any remaining packets.
        while (messageSize > messageIndex)
        {
            newPacket = mPacketFactory->CreatePacket();

            // Build our remaining packets
            newPacket->addUint16(SESSIONOP_DataFrag2);

            newPacket->addUint16(htons(mOutSequenceNext));
            newPacket->addData(message->getData() + messageIndex, std::min<uint16>(mMaxPacketSize - 7, messageSize - messageIndex));

            //no new routing header necessary here
            messageIndex += mMaxPacketSize - 7;  // -2 header, -2 sequence, -3 comp/crc

            // Data channels need compression and encryption
            // no compression in server server communication!
            newPacket->setIsCompressed(false);

            newPacket->setIsEncrypted(true);

            // Push the packet on our outgoing queue
            mNewWindowPacketList.push(newPacket);

        }
    }
    else
    {

        // Create a new packet and push the data into it.
        newPacket = mPacketFactory->CreatePacket();

        newPacket->addUint16(SESSIONOP_DataChannel2);
        //newPacket->setSequence(mOutSequenceNext);
        newPacket->addUint16(htons(mOutSequenceNext));
        newPacket->addUint8(message->getPriority());

        newPacket->addUint8(message->getRouted());
        newPacket->addUint8(message->getDestinationId());
        newPacket->addUint32(message->getAccountId());
        newPacket->addData(message->getData(), message->getSize());  // -2 header, -2 sequence, -2 priority/routing, -5 routing, -3 comp/crc
        newPacket->setIsCompressed(false);

        newPacket->setIsEncrypted(true);

        // Push the packet on our outgoing queue
        mNewWindowPacketList.push(newPacket);

    }
    message->setPendingDelete(true);
}



//======================================================================================================================
// reliable NOT Routed Packets

void Session::_buildOutgoingReliablePackets(Message* message)
{

    Packet*	newPacket = 0;
    uint16	messageIndex = 0;
    uint16	envelopeSize = 13;// -2 header -2 seq -4 size -1 priority -1 routed flag -3 comp/CRC
    uint16	messageSize = message->getSize();

    // If we're too large to fit in a single packet, split us up.
    if(messageSize + envelopeSize > mMaxPacketSize) //why >= ??
    {
        // Build our first packet with the total size.
        newPacket = mPacketFactory->CreatePacket();

        newPacket->addUint16(SESSIONOP_DataFrag1);

        newPacket->addUint16(htons(mOutSequenceNext));

        newPacket->addUint32(htonl(messageSize + 2));

        newPacket->addUint8(message->getPriority());

        newPacket->addUint8(0);                                       // This byte is always 0 on the client
        newPacket->addData(message->getData(), mMaxPacketSize - envelopeSize); // -2 header, -2 sequence, -4 size, -2 priority/routing, -2 crc
        messageIndex += mMaxPacketSize - envelopeSize;                         // -2 header, -2 sequence, -4 size, -2 priority/routing, -2 crc

        // Data channels need compression and encryption
        // as this is server client communication
        newPacket->setIsCompressed(true);
        newPacket->setIsEncrypted(true);

        // Push the packet on our outgoing queue
        mNewWindowPacketList.push(newPacket);

     
        // Now build any remaining packets.
        while (messageSize > messageIndex)
        {
            newPacket = mPacketFactory->CreatePacket();

            // Build our remaining packets
            newPacket->addUint16(SESSIONOP_DataFrag1);

            newPacket->addUint16(htons(mOutSequenceNext));
            newPacket->addData(message->getData() + messageIndex, std::min<uint16>(mMaxPacketSize - 7, messageSize - messageIndex));

            messageIndex += mMaxPacketSize - 7;  // -2 header, -2 sequence, -3 comp/crc

            // Data channels need compression and encryption
            // no compression in server server communication!
            newPacket->setIsCompressed(true);

            newPacket->setIsEncrypted(true);

            // Push the packet on our outgoing queue
            mNewWindowPacketList.push(newPacket);

        }
    }
    else
    {

        // Create a new packet and push the data into it.
        newPacket = mPacketFactory->CreatePacket();

        newPacket->addUint16(SESSIONOP_DataChannel1);
        //newPacket->setSequence(mOutSequenceNext);
        newPacket->addUint16(htons(mOutSequenceNext));
        newPacket->addUint8(message->getPriority());
        newPacket->addUint8(0);//NOT routed
        newPacket->addData(message->getData(), message->getSize());  // -2 header, -2 sequence, -2 priority/routing, -5 routing, -2 crc

        // Data channels need compression and encryption
        // no compression in server server communication!
        newPacket->setIsCompressed(true);

        newPacket->setIsEncrypted(true);

        // Push the packet on our outgoing queue
        mNewWindowPacketList.push(newPacket);
    }
    message->setPendingDelete(true);
}


//======================================================================================================================
// Unreliable Packet Routed or server / client

void Session::_buildOutgoingUnreliablePackets(Message* message)
{
    Packet* newPacket = 0;

    // Create a new packet and push the data into it.
    newPacket = mPacketFactory->CreatePacket();
    newPacket->addUint8(message->getPriority());
    newPacket->addUint8(message->getRouted());

    // If we're routed, prepend the routing header...
    if (message->getRouted())
    {
        newPacket->addUint8(message->getDestinationId());
        newPacket->addUint32(message->getAccountId());
    }
    newPacket->addData(message->getData(), message->getSize());

    // dont compress unreliables
    newPacket->setIsCompressed(false);
    newPacket->setIsEncrypted(true);

    // Push the packet on our outgoing queue
    _addOutgoingUnreliablePacket(newPacket);
	message->setPendingDelete(true);    

}


//======================================================================================================================
void Session::_addOutgoingReliablePacket(Packet* packet)
{
	// Set our last packet sent time index
    packet->setTimeQueued(Anh_Utils::Clock::getSingleton()->getStoredTime());

    // Push the packet on our outgoing queue
    mOutgoingReliablePacketQueue.push(packet);
}


//======================================================================================================================
void Session::_addOutgoingUnreliablePacket(Packet* packet)
{
	// Set our last packet sent time index
    packet->setTimeQueued(Anh_Utils::Clock::getSingleton()->getStoredTime());

    // Push the packet on our outgoing queue
    mOutgoingUnreliablePacketQueue.push(packet);
}


//======================================================================================================================
int8* Session::getAddressString(void)
{
    return inet_ntoa(*(struct in_addr *)&mAddress);
}


//======================================================================================================================
uint16 Session::getPortHost(void)
{
    return ntohs(mPort);
}

//======================================================================================================================

uint32 Session::_buildPackets()
{

    // 2 things
    // 1st try to make as few packets as possible !!!
    // thats a must for server server communication - the soe protocol is extremely expensive in cpu cycles

    // 2nd are there any ways a session can have to generate routed and not routed packets ????? - No its either or

    uint32 packetsbuild = 0;

    //get our message

    Message* message;
    mReliableMessageQueue.pop(message);

	Message* frontMmessage = nullptr;
    bool front = mReliableMessageQueue.front(frontMmessage);


    //=================================
    // messages need to be of a certain size to make multimessages viable
    // so sort out the big ones or those which are alone in the queue and make a single packet if necessary

    if((!front) || ((message->getSize() + frontMmessage->getSize()) > mMaxPacketSize - 21))

    {
        packetsbuild++;

        if(message->getRouted())
            _buildOutgoingReliableRoutedPackets(message);
        else
            _buildOutgoingReliablePackets(message);

        message->setPendingDelete(true);
    }
    else
    {
        if(message->getRouted())        {
            mRoutedMultiMessageQueue.push(message);

            //cave we *might* have 2bytes for Size !!!!!!  (if size 255 or bigger)
            uint16 baseSize = 19 + message->getSize(); // 2 header, 2 sequence, 2 0019, 1(3) size,7 prio/routing, 3 comp/crc
            packetsbuild++;
            while(baseSize < mMaxPacketSize && mReliableMessageQueue.front(message))	{
                baseSize += (message->getSize() + 10); // size + prio + routing	 //thats supposed to be 8
                //cave size *might* be > 255  so using 3 (1 plus 2) for size as a standard!!

                if(baseSize >= mMaxPacketSize )
                    break;

                mReliableMessageQueue.pop(message);
                mRoutedMultiMessageQueue.push(message);
            }
            _buildRoutedMultiDataPacket();
        }
        else
        {
            mMultiMessageQueue.push(message);

            uint16 baseSize = 14 + message->getSize(); // 2 header, 2 sequence, 2 0019, 1 size(3) ,2 prio/routing, 3 comp/crc
            packetsbuild++;
            while(baseSize < mMaxPacketSize && mReliableMessageQueue.front(message))	{
                baseSize += (message->getSize() + 5); // size + prio + routing   cave size *might be > 255 so using 3 (1+2) for size as a standard!!

                if(baseSize >= mMaxPacketSize)
                    break;

                mReliableMessageQueue.pop(message);
                mMultiMessageQueue.push(message);

            }

            _buildMultiDataPacket();
        }

    }

    return(packetsbuild);
}

//======================================================================================================================
//
// this function should not be executed paralell in the session - the sessions need to be processed parallel!!!!
// this way we can keep accessing the front messages without having to worry for them being snapped away
uint32 Session::_buildPacketsUnreliable()
{

    uint32 packetsbuild = 0;

    Message* message;
	if(!mUnreliableMessageQueue.pop(message))	{
		assert(false);
		return 0;
	}

	Message* front_message;
	bool front = mUnreliableMessageQueue.front(front_message);// we need to test the front to check what we can add so please only paralelize the sessions!

    if(!front
            || message->getRouted() //dont pack unreliable in server server - the idea is it just costs unnecessary cpu time as they need to be repackaged
            || message->getSize() > 252 || front_message->getSize() > 252 //sizebyte so 255 is max including header
            || message->getSize() + front_message->getSize() > mMaxUnreliableSize - 16)
    {
        packetsbuild++;
        _buildOutgoingUnreliablePackets(message);

        message->setPendingDelete(true);
    }
    else
    {
        mMultiUnreliableQueue.push(message);

        uint16 baseSize = 12 + message->getSize(); // 2 header, 2 sequence, 2 0019, 1 size,2 prio/routing, 3 comp/crc
        packetsbuild++;
        while(baseSize < mMaxUnreliableSize && mUnreliableMessageQueue.front(message))
        {
            baseSize += message->getSize() + 3; // size + prio + routing

            if(baseSize >= mMaxPacketSize || message->getRouted() || message->getSize() > 252)
                break;

            mUnreliableMessageQueue.pop(message);
            mMultiUnreliableQueue.push(message);
        }

        _buildUnreliableMultiDataPacket();
    }

    return(packetsbuild);
}


void Session::_buildMultiDataPacket()
{
    Packet*		newPacket = mPacketFactory->CreatePacket();
    Message*	message = 0;

    newPacket->addUint16(SESSIONOP_DataChannel1);
    newPacket->addUint16(htons(mOutSequenceNext));
    newPacket->addUint16(0x1900);

    while(!mMultiMessageQueue.empty())
    {
        message = mMultiMessageQueue.front();
        mMultiMessageQueue.pop();

        if((message->getSize() + 2)> 254)
        {
            newPacket->addUint8(0xff);
            newPacket->addUint16(htons(message->getSize() + 2)); // count priority + routing flag;
        }
        else
            newPacket->addUint8(message->getSize() + 2); // count priority + routing flag

        newPacket->addUint8(message->getPriority());
        newPacket->addUint8(0);	//Routing byte -> zero for channel1
        newPacket->addData(message->getData(), message->getSize());

        message->setPendingDelete(true);
    }

    newPacket->setIsCompressed(true);
    newPacket->setIsEncrypted(true);

    mNewWindowPacketList.push(newPacket);

}

//======================================================================
// Routed multidatas for server server communication only !!!!


void Session::_buildRoutedMultiDataPacket()
{
    Packet*		newPacket = mPacketFactory->CreatePacket();
    Message*	message = 0;

    newPacket->addUint16(SESSIONOP_DataChannel2); //server server communication !!!!!
    newPacket->addUint16(htons(mOutSequenceNext));
    newPacket->addUint16(0x1900);
    //newPacket->addUint8(0);
    //newPacket->addUint8(0x19);

    while(!mRoutedMultiMessageQueue.empty())
    {
        message = mRoutedMultiMessageQueue.front();
        mRoutedMultiMessageQueue.pop();

        //assert((message->getSize() + 7)< 255);
        if((message->getSize() + 7) > 254)
        {
            newPacket->addUint8(0xff);
            newPacket->addUint16(htons(message->getSize() + 7)); // count priority + routing flag;
        }
        else
            newPacket->addUint8(message->getSize() + 7); // count priority + routing flag


        newPacket->addUint8(message->getPriority());
        newPacket->addUint8(1);		//routed

        newPacket->addUint8(message->getDestinationId());
        newPacket->addUint32(message->getAccountId());
        //dest = fragment->getUint8();
        //accountId = fragment->getUint32();


        newPacket->addData(message->getData(), message->getSize());

        message->setPendingDelete(true);

    }

    newPacket->setIsCompressed(false); //server server !!! save the cycles!!!
    newPacket->setIsEncrypted(true);

    mNewWindowPacketList.push(newPacket);

}

//======================================================================
// Unreliable Multi DataPacket
// thats server client communication
void Session::_buildUnreliableMultiDataPacket()
{
    Packet*		newPacket = mPacketFactory->CreatePacket();
    Message*	message = 0;

    newPacket->addUint16(SESSIONOP_MultiPacket);

    while(!mMultiUnreliableQueue.empty())
    {
        message = mMultiUnreliableQueue.front();
        mMultiUnreliableQueue.pop();

        newPacket->addUint8(message->getSize() + 2); // count priority + routing flag
        newPacket->addUint8(message->getPriority());
        newPacket->addUint8(0);
        newPacket->addData(message->getData(), message->getSize());

        message->setPendingDelete(true);
    }

    newPacket->setIsCompressed(true);
    newPacket->setIsEncrypted(true);
	
    _addOutgoingUnreliablePacket(newPacket);

}

//======================================================================================================================

