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
#include "Message.h"
#include "ThreadedMessageFactory.h"
#include "NetworkManager/Session.h"//mainly for debugging purposes
#include "Utils/clock.h"


#include <algorithm>
#include <cassert>
#include <cstring>

// Fix for issues with glog redefining this constant
#ifdef ERROR
#undef ERROR
#endif

#include <glog/logging.h>

//======================================================================================================================

ThreadedMessageFactory* ThreadedMessageFactory::mSingleton = 0;

//======================================================================================================================

ThreadedMessageFactory::ThreadedMessageFactory(uint32 heapSize,int chunks) :
mChunkCount(2)
{
    if(chunks > CHUNK_COUNT)
		chunks = CHUNK_COUNT;

	mChunkCount = chunks;

	for(int i = 0; i < chunks;i++)	{
		mChunk = new MessageFactoryChunk((heapSize/chunks), i, this);
		mChunkQueue.push(mChunk);
	}

}

//======================================================================================================================

bool ThreadedMessageFactory::getBusy(void)
{
	//TODO implement ways to deal with high load
	if(mChunkQueue.unsafe_size() == 0)	{
		MessageFactoryChunk* busy_chunk;
		if(mBusyChunkQueue.try_pop(busy_chunk))	{
			busy_chunk->Process();
			if(busy_chunk->getHeapsize() > 99.0)
				mBusyChunkQueue.push(busy_chunk);
			else
				mChunkQueue.push(busy_chunk);
		}
		return ((mChunkQueue.unsafe_size() == 0) && (mBusyChunkQueue.unsafe_size() == mChunkCount));
	}

	return false;
}

ThreadedMessageFactory::~ThreadedMessageFactory()
{
	int i = 0;
	MessageFactoryChunk* c;

	while(i < mChunkCount)	{
		if(mChunkQueue.try_pop(c))	{
			i++;
			delete c;
		}

	}

    // mSingleton = 0;
    // Actually, we can't null mSingleton since network manager calls this code directly,
    // using instances created by new(), bypassing the singleton concept,
    // and assign null to mSingleton would invalidate the possibility to delete the "singleton-version" used by zoneserver.

    // mSingleton = 0; IS executed, but in destroySingleton();

    // Important to notice is please that message factory isnt threadsafe
    // thus we need several instances of it !!!!!!
}

//======================================================================================================================

 MessageFactoryChunk* ThreadedMessageFactory::getChunk()
{
	MessageFactoryChunk* chunk;
	while(!mChunkQueue.try_pop(chunk))	{
		LOG(WARNING) <<  "ThreadedMessageFactory::No free chunk!";
		MessageFactoryChunk* busy_chunk;
		if(mBusyChunkQueue.try_pop(busy_chunk))	{
			busy_chunk->Process();
			if(busy_chunk->getHeapsize() > 99.0)
				mBusyChunkQueue.push(busy_chunk);
			else
				mChunkQueue.push(busy_chunk);
		}
	}
	return chunk;
}

 void ThreadedMessageFactory::returnChunk(MessageFactoryChunk* chunk)
 {
	 mChunkQueue.push(chunk);
 }

 void ThreadedMessageFactory::returnBusyChunk(MessageFactoryChunk* chunk)
 {
	 mBusyChunkQueue.push(chunk);
 }


MessageFactoryChunk::MessageFactoryChunk(uint32 heapSize, uint8 id, ThreadedMessageFactory* factory)
 : mCurrentMessage(0)
    , mCurrentMessageEnd(0)
    , mCurrentMessageStart(0)
    , mHeapStart(0)
    , mHeapEnd(0)
    , mHeapRollover(0)
    , mMessageHeap(NULL)
    , mHeapTotalSize(heapSize)
    , mMessagesCreated(0)
    , mMessagesDestroyed(0)
    , mHeapWarnLevel(80.0)
    , mMaxHeapUsedPercent(0)
{
	mID = id;
	mFactory =	factory;
	// Allocate our message heap.
    mMessageHeap = new int8[mHeapTotalSize];
    memset(mMessageHeap, 0xed, mHeapTotalSize);
    mHeapStart = mMessageHeap;
    mHeapEnd = mMessageHeap;

    mLastHeapLevel = 0;
    mLastHeapLevelTime = gClock->getSingleton()->getStoredTime();

    mLastTime = Anh_Utils::Clock::getSingleton()->getLocalTime();
}

MessageFactoryChunk::~MessageFactoryChunk()
{
	delete[] mMessageHeap;
}

void MessageFactoryChunk::Process(void)
{
    // Do some garbage collection if we can.
    _processGarbageCollection();

    //maintain a 1sec resolution clock to timestamp messages
    //gClock->process();
}

//======================================================================================================================
// important is, that the size of the message class is not nuking our heap bounds!
void MessageFactoryChunk::StartMessage(void)
{
    // Do some garbage collection if we can.
    _processGarbageCollection();

    // Initialize the message start and end.
    mCurrentMessageStart = mHeapStart;

    assert(mCurrentMessage==0 && "Can't handle more than one message at once.");

    mCurrentMessageEnd = mCurrentMessageStart;

    // Adjust start bounds if necessary.
    _adjustMessageStart(sizeof(Message));

    mCurrentMessage = new(mCurrentMessageStart) Message();
    mCurrentMessageEnd = mCurrentMessageStart + sizeof(Message);
}

//======================================================================================================================

uint32 MessageFactoryChunk::HeapWarningLevel(void)
{
    uint64 now = gClock->getSingleton()->getStoredTime();

    uint32 warnLevel = (uint32)(mCurrentUsed/10);
    if((mCurrentUsed > mLastHeapLevel)&&(mCurrentUsed - mLastHeapLevel) > 10.0)
        warnLevel += 2;

    if((mCurrentUsed > mLastHeapLevel)&&(mCurrentUsed - mLastHeapLevel) > 20.0)
        warnLevel += 4;

    if((now - mLastHeapLevelTime) > 1000)
    {
        mLastHeapLevelTime =  now;
        mLastHeapLevel = mCurrentUsed;
    }


    return warnLevel;

}

//======================================================================================================================

Message* MessageFactoryChunk::EndMessage(void)
{
    assert(mCurrentMessage && "Must call StartMessage before EndMessage.");

    // Do some garbage collection if we can.
    //_processGarbageCollection();

    // Just cast the message start
    Message* message = mCurrentMessage;

    message->setData(mCurrentMessageStart + sizeof(Message));
    message->setSize((uint16)(mCurrentMessageEnd - mCurrentMessageStart) - sizeof(Message));
    message->setCreateTime(gClock->getSingleton()->getStoredTime());
    mCurrentMessageStart = mCurrentMessageEnd;

    // Zero out our mCurrentMessage so we know we're not working on one.
    mCurrentMessage = 0;

    //adjust heapstart to past our new message
    mHeapStart += message->getSize() + sizeof(Message);

    //Update our stats.
    mMessagesCreated++;
    mCurrentUsed = ((float)_getHeapSize() / (float)mHeapTotalSize)* 100.0f;
    mMaxHeapUsedPercent = std::max<float>(mMaxHeapUsedPercent,  mCurrentUsed);

	/*
    // warn if we get near our boundaries
    if(mCurrentUsed > mHeapWarnLevel)
    {
        mHeapWarnLevel = static_cast<float>(mCurrentUsed+1.2);
        LOG(WARNING) << "MessageFactory Heap at " << mCurrentUsed;
    } else if (((mCurrentUsed+2.2) < mHeapWarnLevel) && mHeapWarnLevel > 80.0)
        mHeapWarnLevel = mCurrentUsed;
	*/
	if(mCurrentUsed <= 99.0)	{
		mFactory->returnChunk(this);
	}
	else	{
		mFactory->returnBusyChunk(this);
	}
    return message;
}

//======================================================================================================================

void MessageFactoryChunk::DestroyMessage(Message* message)
{
    // Just flag the message for deletion
    message->setPendingDelete(true);
}

//======================================================================================================================

void MessageFactoryChunk::addInt8(int8 data)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage && "Must call StartMessage before adding data");

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(sizeof(data));

    // Insert our data and move our end pointer.
    *mCurrentMessageEnd = data;
    mCurrentMessageEnd += sizeof(int8);
}

//======================================================================================================================

void MessageFactoryChunk::addUint8(uint8 data)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage && "Must call StartMessage before adding data");

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(sizeof(data));

    // Insert our data and move our end pointer.
    *mCurrentMessageEnd = (uint8)data;
    mCurrentMessageEnd += sizeof(uint8);
}

//======================================================================================================================

void MessageFactoryChunk::addInt16(int16 data)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage && "Must call StartMessage before adding data");

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(sizeof(data));

    // Insert our data and move our end pointer.
    *((int16*)mCurrentMessageEnd) = data;
    mCurrentMessageEnd += sizeof(uint16);
}

//======================================================================================================================

void MessageFactoryChunk::addUint16(uint16 data)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage);

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(sizeof(data));

    // Insert our data and move our end pointer.
    *((uint16*)mCurrentMessageEnd) = data;
    mCurrentMessageEnd += sizeof(uint16);
}

//======================================================================================================================

void MessageFactoryChunk::addInt32(int32 data)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage && "Must call StartMessage before adding data");

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(sizeof(data));

    // Insert our data and move our end pointer.
    *((int32*)mCurrentMessageEnd) = data;
    mCurrentMessageEnd += sizeof(int32);
}

//======================================================================================================================

void MessageFactoryChunk::addUint32(uint32 data)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage && "Must call StartMessage before adding data");

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(sizeof(data));

    // Insert our data and move our end pointer.
    *((uint32*)mCurrentMessageEnd) = data;
    mCurrentMessageEnd += sizeof(uint32);
}

//======================================================================================================================

void MessageFactoryChunk::addInt64(int64 data)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage && "Must call StartMessage before adding data");

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(sizeof(data));

    // Insert our data and move our end pointer.
    *((int64*)mCurrentMessageEnd) = data;
    mCurrentMessageEnd += sizeof(int64);
}

//======================================================================================================================

void MessageFactoryChunk::addUint64(uint64 data)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage && "Must call StartMessage before adding data");

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(sizeof(data));

    // Insert our data and move our end pointer.
    *((uint64*)mCurrentMessageEnd) = data;
    mCurrentMessageEnd += sizeof(uint64);
}

//======================================================================================================================

void MessageFactoryChunk::addFloat(float data)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage && "Must call StartMessage before adding data");

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(sizeof(data));

    // Insert our data and move our end pointer.
    *((float*)mCurrentMessageEnd) = data;
    mCurrentMessageEnd += sizeof(float);
}

//======================================================================================================================

void MessageFactoryChunk::addDouble(double data)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage && "Must call StartMessage before adding data");

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(sizeof(data));

    // Insert our data and move our end pointer.
    *((double*)mCurrentMessageEnd) = data;
    mCurrentMessageEnd += sizeof(double);
}

//======================================================================================================================

void MessageFactoryChunk::addString(const std::string& string)
{
    BString str(string.c_str());
    addString(str);
//return;
}

//======================================================================================================================

void MessageFactoryChunk::addString(const std::wstring& string)
{
    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(string.size());

    // First insert the string length
    *((uint32*)mCurrentMessageEnd) = string.length();
    mCurrentMessageEnd += 4;

    std::copy(string.begin(), string.end(), reinterpret_cast<uint16_t*>(mCurrentMessageEnd));
    mCurrentMessageEnd += string.length() * 2;
//return;
}

//======================================================================================================================

void MessageFactoryChunk::addString(const char* cstring)
{
    BString str;
    str = cstring;
    addString(str);
//return;
}
//======================================================================================================================

void MessageFactoryChunk::addString(const unsigned short* ustring)
{
    BString ustr;
    ustr = ustring;
    addString(ustr);
//return;
}
//======================================================================================================================

void MessageFactoryChunk::addString(const wchar_t* ustring)
{
    BString ustr;
    ustr = ustring;
    addString(ustr);
}
//======================================================================================================================

void MessageFactoryChunk::addString(const BString& data)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage && "Must call StartMessage before adding data");

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(data.getDataLength());

    // Insert our data and move our end pointer.
    switch(data.getType())
    {
    case BSTRType_UTF8:
    case BSTRType_ANSI:
    {
        // First insert the string length
        *((uint16*)mCurrentMessageEnd) = data.getLength();
        mCurrentMessageEnd += 2;

        memcpy(mCurrentMessageEnd, data.getAnsi(), data.getLength());
        mCurrentMessageEnd += data.getLength();
    }
    break;

    case BSTRType_Unicode16:
    {
        // First insert the string length
        *((uint32*)mCurrentMessageEnd) = data.getLength();
        mCurrentMessageEnd += 4;

        memcpy(mCurrentMessageEnd, data.getUnicode16(), data.getLength() * 2);
        mCurrentMessageEnd += data.getLength() * 2;
    }
    break;
    }
}

//======================================================================================================================

void MessageFactoryChunk::addData(const int8* data, uint16 len)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage && "Must call StartMessage before adding data");

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(len);

    // Insert our data and move our end pointer.
    memcpy(mCurrentMessageEnd, data, len);
    mCurrentMessageEnd += len;
}

//======================================================================================================================

void MessageFactoryChunk::addData(const uint8_t* data, uint16 len)
{
    // Make sure we've called StartMessage()
    assert(mCurrentMessage && "Must call StartMessage before adding data");

    // Adjust start bounds if necessary.
    _adjustHeapStartBounds(len);

    // Insert our data and move our end pointer.
    memcpy(reinterpret_cast<uint8_t*>(mCurrentMessageEnd), data, len);
    mCurrentMessageEnd += len;
}

//======================================================================================================================

void MessageFactoryChunk::_processGarbageCollection(void)
{
	// warn if we get near our boundaries
    if(mCurrentUsed > mHeapWarnLevel)
    {
        mHeapWarnLevel = static_cast<float>(mCurrentUsed+1.2);
        LOG(WARNING) << "ThreadedMessageFactory :: MessageFactory Heap at " << mCurrentUsed << "Chunk ID : " << mID;
    } else if (((mCurrentUsed+2.2) < mHeapWarnLevel) && mHeapWarnLevel > 80.0)
        mHeapWarnLevel = mCurrentUsed;

    // Just check to see if the oldest message is ready to be deleted yet.
    //start with the oldest message
    assert(mHeapEnd < mMessageHeap + mHeapTotalSize && "mHeapEnd not within mMessageHeap bounds");
    Message* message = reinterpret_cast<Message*>(mHeapEnd);

    //when the oldest Message wont get deleted No other messages get deleted from the heap !!!!!!!!


    uint32 count = 0;
    bool further = true;

    while( (count <= 50) && further)    {
        if (mHeapEnd != mHeapStart)        {
            if (message->getPendingDelete())	{
                message->~Message();
                //memset(mHeapEnd, 0xed, size + sizeof(Message));
                mHeapEnd += message->getSize() + sizeof(Message);

                mMessagesDestroyed++;

                // If we're at the end of the queue, rollover to the front again.
                if (mHeapEnd == mHeapRollover)	{
                    if (mHeapEnd == mHeapStart)                    {
                        mHeapStart = mMessageHeap;
                    }

                    mHeapEnd		= mMessageHeap;
                    mHeapRollover	= 0;
                }

                message = reinterpret_cast<Message*>(mHeapEnd);

				// we could have hit the end
                if(!message)
                    return;

                assert(mHeapEnd < mMessageHeap + mHeapTotalSize  && "mHeapEnd not within mMessageHeap bounds");

                further = (mHeapEnd != mHeapStart) && message->getPendingDelete();

            }//pending delete

            else if(Anh_Utils::Clock::getSingleton()->getStoredTime() - message->getCreateTime() > MESSAGE_MAX_LIFE_TIME)
            {
                further = false;
                if (!message->mLogged)
                {
                    LOG(WARNING) <<  "Garbage Collection found a new stuck message!"
                        << " : " << ( uint32((Anh_Utils::Clock::getSingleton()->getStoredTime() - message->getCreateTime())/1000));

                    message->mLogged = true;
                    message->mLogTime = Anh_Utils::Clock::getSingleton()->getStoredTime();

                    Session* session = (Session*)message->mSession;

                    if(!session)
                    {
                        LOG(INFO) << "Packet is Sessionless.";
                        message->setPendingDelete(true);
                    }
                    else if(session->getStatus() > SSTAT_Disconnected || session->getStatus() == SSTAT_Disconnecting)
                    {
                        LOG(INFO) << "Session is about to be destroyed.";
                    }
                }

                Session* session = (Session*)message->mSession;

                if(!session)
                {
                    LOG(INFO) << "Garbage Collection found sessionless packet";
                    message->setPendingDelete(true);
                }
                else if(Anh_Utils::Clock::getSingleton()->getStoredTime() >(message->mLogTime +10000))
                {
                    LOG(WARNING) << "Garbage Collection found a old stuck message!"
                    << "age : "<< (uint32((Anh_Utils::Clock::getSingleton()->getStoredTime() - message->getCreateTime())/1000))
                    << "Session status : " << session->getStatus();
                    message->mLogTime  = Anh_Utils::Clock::getSingleton()->getStoredTime();
                    return;
                }
                else if(Anh_Utils::Clock::getSingleton()->getStoredTime() - message->getCreateTime() > MESSAGE_MAX_LIFE_TIME)
                {
                    if(session)
                    {
                        // make sure that the status is not set again from Destroy to Disconnecting
                        // otherwise we wont ever get rid of that session
                        if(session->getStatus() < SSTAT_Disconnecting)
                        {
                            session->setCommand(SCOM_Disconnect);
                            LOG(WARNING) << "Garbage Collection Message Heap Time out. Destroying Session";
                        }
                        if(session->getStatus() == SSTAT_Destroy)
                        {
                            LOG(WARNING) << "Garbage Collection Message Heap Time out. Session about to Destroyed.";
                        }
                        return;
                    }
                    else
                    {
                        message->setPendingDelete(true);
                        LOG(WARNING) << "Garbage Collection Message Heap Time out. Session Already Destroyed. Tagged Message as Deletable.";
                        return;
                    }


                }

            }
            else
                return;

        }//Heap start != Heapend
        else        {
            return;
        }

        //we need to be accurate here
        count++;
    }

}

//======================================================================================================================

void MessageFactoryChunk::_adjustHeapStartBounds(uint32 size)
{
    // Are we going to overflow our heap?
    uint32 heapSize = _getHeapSize();

    // _getHeapSize() returns the size of already constructed packets,
    // but NOT the parts we already have been added for THIS message under construction.

    uint32 messageSize = (mCurrentMessageEnd - mCurrentMessageStart);

    //assert(mHeapTotalSize > messageSize + heapSize + size);
    assert(mHeapTotalSize > messageSize + heapSize && "Message heap overflow.");

    // Check to see if this add is going to push past the heap boundry.
    if(mCurrentMessageEnd + size > mMessageHeap + mHeapTotalSize)
    {
        // We've gone past the end of our heap, copy this message to the front of the heap and continue

        memcpy(mMessageHeap, mCurrentMessageStart, messageSize);

        // Reset our main heap pointer(s)
        if(mHeapStart == mHeapEnd)
        {
            mHeapEnd = mMessageHeap;
        }

        mHeapStart		= mMessageHeap;
        mHeapRollover	= mCurrentMessageStart;

        // Reinit our message pointers.
        mCurrentMessage			= (Message*)mMessageHeap;
        mCurrentMessageStart	= mMessageHeap;
        mCurrentMessageEnd		= mMessageHeap + messageSize;

        mCurrentMessage->setData(mMessageHeap + sizeof(Message));

       LOG(WARNING)<< "Heap Rollover chunk: " << mID << "STATS: MessageHeap - size:"
        << heapSize << " maxUsed: " << mMaxHeapUsedPercent <<", created: " << mMessagesCreated <<", destroyed: " << mMessagesDestroyed;
    }
}

// the trouble is, that if we start a new Message we need to check whether the size of the message class is still inside the heap bounds.
// However mCurrentMessage is 0 at that point
void MessageFactoryChunk::_adjustMessageStart(uint32 size)
{
    // Are we going to overflow our heap?
    uint32 heapSize = _getHeapSize();

    // _getHeapSize() returns the size of already constructed packets,
    // but NOT the parts we already have been added for THIS message under construction.

    uint32 messageSize = (mCurrentMessageEnd - mCurrentMessageStart);

    //assert(mHeapTotalSize > messageSize + heapSize + size);
    assert(mHeapTotalSize > messageSize + heapSize && "Message heap overflow.");

    // Check to see if this add is going to push past the heap boundry.
    if(mCurrentMessageEnd + size > mMessageHeap + mHeapTotalSize)
    {
        // We've gone past the end of our heap, copy this message to the front of the heap and continue

        memcpy(mMessageHeap, mCurrentMessageStart, messageSize);

        // Reset our main heap pointer(s)
        if(mHeapStart == mHeapEnd)
        {
            mHeapEnd = mMessageHeap;
        }

        mHeapStart		= mMessageHeap;
        mHeapRollover	= mCurrentMessageStart;

        // Reinit our message pointers.
        mCurrentMessage			= (Message*)mMessageHeap;
        mCurrentMessageStart	= mMessageHeap;
        mCurrentMessageEnd		= mMessageHeap + messageSize;

        //mCurrentMessage->setData(mMessageHeap + sizeof(Message));

        LOG(WARNING)<< "Heap Rollover hunk id " << mID << "STATS: MessageHeap - size:"
        << heapSize << " maxUsed: " << mMaxHeapUsedPercent <<", created: " << mMessagesCreated <<", destroyed: " << mMessagesDestroyed;
    }
}

//======================================================================================================================



