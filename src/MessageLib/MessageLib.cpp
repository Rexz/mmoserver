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

#include "MessageLib.h"

#include <algorithm>

#include <boost/lexical_cast.hpp>

#ifdef _WIN32
#undef ERROR
#endif
#include <glog/logging.h>

#include "Common/atMacroString.h"
#include "Common/Crc.h"

#include "NetworkManager/DispatchClient.h"
#include "NetworkManager/Message.h"
#include "NetworkManager/MessageDispatch.h"
#include "NetworkManager/MessageFactory.h"
#include "NetworkManager/MessageOpcodes.h"

#include "ZoneServer/BuildingObject.h"
#include "ZoneServer/CellObject.h"
#include "ZoneServer/CharSheetManager.h"
#include "ZoneServer/ContainerManager.h"
#include "ZoneServer/Conversation.h"
#include "ZoneServer/CraftingTool.h"
#include "ZoneServer/CurrentResource.h"
#include "ZoneServer/Datapad.h"
#include "ZoneServer/HouseObject.h"
#include "ZoneServer/IntangibleObject.h"
#include "ZoneServer/HarvesterObject.h"
#include "ZoneServer/FactoryObject.h"
#include "ZoneServer/FactoryCrate.h"
#include "ZoneServer/Inventory.h"
#include "ZoneServer/ManufacturingSchematic.h"

#include "ZoneServer/NPCObject.h"
#include "ZoneServer/Object.h"
#include "ZoneServer/ObjectControllerOpcodes.h"
#include "ZoneServer/ObjectFactory.h"
#include "ZoneServer/PlayerObject.h"
#include "ZoneServer/ResourceContainer.h"
#include "ZoneServer/Tutorial.h"
#include "ZoneServer/UIOpcodes.h"
#include "ZoneServer/VehicleController.h"
#include "ZoneServer/Wearable.h"
#include "ZoneServer/WorldConfig.h"
#include "ZoneServer/WorldManager.h"

#include "ZoneServer/ZoneOpcodes.h"
#include "ZoneServer/Zmap.h"


//======================================================================================================================

bool					ThreadSafeMessageLib::mInsFlag    = false;
ThreadSafeMessageLib*	ThreadSafeMessageLib::mSingleton  = NULL;

//======================================================================================================================

ThreadSafeMessageLib::ThreadSafeMessageLib()
{
	//get our own Messagefactory so we do not have to worry about the mainthread accessing it
    mMessageFactory = new MessageFactory(gConfig->read<uint32>("GlobalMessageHeap")*1024);
}

//======================================================================================================================

ThreadSafeMessageLib*	ThreadSafeMessageLib::Init()
{
    if(!mInsFlag)
    {
        mSingleton = new ThreadSafeMessageLib();
        mInsFlag = true;

        return mSingleton;
    }
    else
        return mSingleton;
}

//======================================================================================================================

ThreadSafeMessageLib::~ThreadSafeMessageLib()
{
    mInsFlag = false;
    delete(mSingleton);
	delete(mMessageFactory);
}

void ThreadSafeMessageLib::Process()
{
		mMessageFactory->Process();

}

bool		MessageLib::mInsFlag    = false;
MessageLib*	MessageLib::mSingleton  = NULL;

//======================================================================================================================

MessageLib::MessageLib()
{
	mMessageFactory = gMessageFactory;
}

//======================================================================================================================

MessageLib*	MessageLib::Init()
{
    if(!mInsFlag)
    {
        mSingleton = new MessageLib();
        mInsFlag = true;

        return mSingleton;
    }
    else
        return mSingleton;
}

//======================================================================================================================

MessageLib::~MessageLib()
{
    mInsFlag = false;
    delete(mSingleton);
}

/*
//======================================================================================================================
//
// saves a bytebuffer from a threaded source CURRENTLY CLIENT ONLY
//
void MessageLib::threadChatMessage(ByteBuffer* buffer, Object* messageTarget, WireMode mode, uint32 crc)
{
	PlayerObjectSet registered_watchers;
	if((mode == wire_viewField) || (mode == wire_selfViewField))
	{
		registered_watchers = *messageTarget->getRegisteredWatchers();
	}

	ObjectListType		inRangePlayers;
	if((mode == wire_chatField) || (mode == wire_selfChatField))
	{
		mGrid->GetChatRangeCellContents(messageTarget->getGridBucket(), &inRangePlayers);
	}

	auto task = std::make_shared<boost::packaged_task<bool>>([=] {

		//save the bytebuffer in its wrap and put it on the queue for later sending
		messageWrapper* wrap = new(messageWrapper);
		wrap->buffer	= buffer;
		wrap->mode		= mode;
		wrap->target	= messageTarget;
		wrap->priority	= 5;
		wrap->reliable	= true;	
		wrap->crc		= crc;
		wrap->inRangePlayers	= inRangePlayers;
		wrap->watchers			= registered_watchers;

		mMessageQueue->push(wrap);
	}
	);
	//create it directly
	Process();
}

//======================================================================================================================
//
// saves a bytebuffer from a threaded source CURRENTLY CLIENT ONLY
//
void MessageLib::threadReliableMessage(ByteBuffer* buffer, Object* messageTarget, WireMode mode, uint8 priority)
{
	PlayerObjectSet registered_watchers;
	if((mode == wire_viewField) || (mode == wire_selfViewField))
	{
		registered_watchers = *messageTarget->getRegisteredWatchers();
	}

	ObjectListType		inRangePlayers;
	if((mode == wire_chatField) || (mode == wire_selfChatField))
	{
		mGrid->GetChatRangeCellContents(messageTarget->getGridBucket(), &inRangePlayers);
	}

	auto task = std::make_shared<boost::packaged_task<bool>>([=] {

		//save the bytebuffer in its wrap and put it on the queue for later sending
		messageWrapper* wrap = new(messageWrapper);
		wrap->buffer	= buffer;
		wrap->mode		= mode;
		wrap->target	= messageTarget;
		wrap->priority	= priority;
		wrap->reliable	= true;	
		wrap->inRangePlayers	= inRangePlayers;
		wrap->watchers			= registered_watchers;

		mMessageQueue->push(wrap);
	}
	);
	//create it directly
	Process();
}

//======================================================================================================================
//
// saves a bytebuffer from a threaded source CURRENTLY CLIENT ONLY
//
void MessageLib::threadUnreliableMessage(ByteBuffer* buffer, Object* messageTarget, WireMode mode, uint8 priority)
{
	PlayerObjectSet registered_watchers;
	if((mode == wire_viewField) || (mode == wire_selfViewField))
	{
		registered_watchers = *messageTarget->getRegisteredWatchers();
	}

	ObjectListType		inRangePlayers;
	if((mode == wire_chatField) || (mode == wire_selfChatField))
	{
		mGrid->GetChatRangeCellContents(messageTarget->getGridBucket(), &inRangePlayers);
	}

	auto task = std::make_shared<boost::packaged_task<bool>>([=] {

		//save the bytebuffer in its wrap and put it on the queue for later sending
		messageWrapper* wrap = new(messageWrapper);
		wrap->buffer			= buffer;
		wrap->mode				= mode;
		wrap->target			= messageTarget;
		wrap->priority			= priority;
		wrap->reliable			= false;	
		wrap->inRangePlayers	= inRangePlayers;
		wrap->watchers			= registered_watchers;

		mMessageQueue->push(wrap);
	}
	);

	//create it directly
	Process();
}
*/

//======================================================================================================================
//
// Processes the message queue in the activethread to send the queued messages
// without upsetting the messagefactory
//
/*
void MessageLib::Process()
{
	
	auto task = std::make_shared<boost::packaged_task<bool>>([=] {
		mMessageFactory->Process();
	}
	);
	// Add the message to the active object's queue that runs the task
    active_.Send([task] {
        (*task)();
    });
	
	auto task = std::make_shared<boost::packaged_task<bool>>([=] {
		//uint32 count = 0;
		//bool further = mMessageQueue->empty();
		
		//while((count < 50) && further)
		//{
			messageWrapper* wrap;
			wrap = mMessageQueue->front();
			mMessageQueue->pop();
			//further = mMessageQueue->empty();
		
			Message* newMessage;
	
			mMessageFactory->StartMessage();
			mMessageFactory->addData(wrap->buffer->data(),wrap->buffer->size());
			newMessage = mMessageFactory->EndMessage();

			//now send it 

			switch(wrap->mode)
			{
				case wire_allReliable:
				{
					_sendToAll(newMessage, wrap->priority, false);
				}
				break;

				case wire_allUnreliable:
				{
					_sendToAll(newMessage, wrap->priority, true);
				}
				break;

				case  wire_singlePlayer:
				{
					if(PlayerObject* player = dynamic_cast<PlayerObject*>(wrap->target))
					{
						if(wrap->reliable)
							(player->getClient())->SendChannelAUnreliable(newMessage,player->getAccountId(),CR_Client,static_cast<uint8>(wrap->priority));	
						else
							(player->getClient())->SendChannelA(newMessage,player->getAccountId(),CR_Client,static_cast<uint8>(wrap->priority));	
					}
				}
				break;

				case wire_selfViewField:
				{
					if(wrap->reliable)
						_sendToInRange(newMessage, wrap->target, wrap->priority, true);
					else
						_sendToInRangeUnreliable(newMessage, wrap->target, wrap->priority, wrap->watchers, true);
				}
				break;

				case  wire_viewField:
				{
					if(wrap->reliable)
						_sendToInRange(newMessage, wrap->target, wrap->priority, false);
					else
						_sendToInRangeUnreliable(newMessage, wrap->target, wrap->priority, wrap->watchers, false);
				}
				break;

				case wire_chatField:
				{
					const CreatureObject* creature = dynamic_cast<const CreatureObject*>(wrap->target);
					if(creature)
					{
						_sendToInRangeUnreliableChat(newMessage, creature, wrap->priority, wrap->crc, wrap->inRangePlayers);
					}
				}
				break;

			}

			delete(wrap->buffer);
			delete(wrap);
		//}
	}
	);

	active_.Send([task] {
        (*task)();
    });

}
*/
//======================================================================================================================
//
// Checks the validity of the player in the global map
//
bool MessageLib::_checkPlayer(const PlayerObject* const player) const
{
    //player gets PlayerConnState_LinkDead when he disconnects but is still in the world
    //we in theory could still send updates
    //return((player->isConnected())&&(player->getClient()));

    //the idea is that this check gets useless when the SI / knownobjectscode is stable

    return((player)&&(player->getClient()));
}

//======================================================================================================================
//
// Checks the validity of the player in the global map
//
bool ThreadSafeMessageLib::_checkPlayer(const PlayerObject* const player) const
{
    //player gets PlayerConnState_LinkDead when he disconnects but is still in the world
    //we in theory could still send updates
    //return((player->isConnected())&&(player->getClient()));

    //the idea is that this check gets useless when the SI / knownobjectscode is stable

    return((player)&&(player->getClient()));
}



//================================================================================================0
//send movement based on messageheap size and distance
bool MessageLib::_checkDistance(const glm::vec3& mPosition1, Object* object, uint32 heapWarningLevel)
{

    //just send everything we have
    if(heapWarningLevel < 4)
        return true;
    else if (heapWarningLevel < 6)
    {
        if(glm::distance(object->mPosition, mPosition1) < 96)
            return object->movementMessageToggle();
    }
    else if (heapWarningLevel < 8)
    {
        if(glm::distance(object->mPosition, mPosition1) < 64)
            return object->movementMessageToggle();
    }
    else if (heapWarningLevel < 10)
    {
        float distance = glm::distance(object->mPosition, mPosition1);
        if(distance <= 32)
            return true;
        else if(distance > 32)
            return object->movementMessageToggle();
        else if(distance > 64)
            return false;
    }
    else if (heapWarningLevel >= 10)
        return false;




    return false;
}

//================================================================================================0
//send movement based on messageheap size and distance
bool ThreadSafeMessageLib::_checkDistance(const glm::vec3& mPosition1, Object* object, uint32 heapWarningLevel)
{

    //just send everything we have
    if(heapWarningLevel < 4)
        return true;
    else if (heapWarningLevel < 6)
    {
        if(glm::distance(object->mPosition, mPosition1) < 96)
            return object->movementMessageToggle();
    }
    else if (heapWarningLevel < 8)
    {
        if(glm::distance(object->mPosition, mPosition1) < 64)
            return object->movementMessageToggle();
    }
    else if (heapWarningLevel < 10)
    {
        float distance = glm::distance(object->mPosition, mPosition1);
        if(distance <= 32)
            return true;
        else if(distance > 32)
            return object->movementMessageToggle();
        else if(distance > 64)
            return false;
    }
    else if (heapWarningLevel >= 10)
        return false;




    return false;
}


// sends given function to all of the containers registered watchers
void MessageLib::_sendToRegisteredWatchers(PlayerObjectSet registered_watchers, Object* const object, std::function<void (PlayerObject* const player)> callback, bool toSelf) const
{
	PlayerObjectSet::const_iterator it		= registered_watchers.begin();
		
	while(it != registered_watchers.end())
	{
		//create it for the registered Players
		PlayerObject* const player = *it;
		if(player && _checkPlayer(player))//use _checkPlayer for debug only
		{
			callback(player);
		}
		else
		{
			//an invalid player at this point is like armageddon and Ultymas birthday combined at one time
			//if this happens we need to know about it
			assert(false && "Invalid Player in sendtoInrange");
		}
        it++;
	}

	if(toSelf)
	{
		PlayerObject* player = dynamic_cast<PlayerObject*>(object);
		if(player && _checkPlayer(player))//use _checkPlayer for debug only
		{
			callback(player);
		}
	}

}

// sends given function to all of the containers registered watchers
void ThreadSafeMessageLib::_sendToRegisteredWatchers(PlayerObjectSet registered_watchers, Object* const object, std::function<void (PlayerObject* const player)> callback, bool toSelf)const
{
	PlayerObjectSet::const_iterator it		= registered_watchers.begin();
		
	while(it != registered_watchers.end())
	{
		//create it for the registered Players
		PlayerObject* const player = *it;
		if(player && _checkPlayer(player))//use _checkPlayer for debug only
		{
			callback(player);
		}
		else
		{
			//an invalid player at this point is like armageddon and Ultymas birthday combined at one time
			//if this happens we need to know about it
			assert(false && "Invalid Player in sendtoInrange");
		}
        it++;
	}

	if(toSelf)
	{
		PlayerObject* const player = dynamic_cast<PlayerObject*>(object);
		if(player && _checkPlayer(player))//use _checkPlayer for debug only
		{
			callback(player);
		}
		//just bail out silently if the player here is invalid, as many NPC movement updates end up here
	}

}

// sends given function to all of the containers registered watchers
void ThreadSafeMessageLib::_sendToList(ObjectListType listeners, Object* object, std::function<void (PlayerObject* const player)> callback, bool toSelf)
{
	ObjectListType::const_iterator it		= listeners.begin();
		
	while(it != listeners.end())
	{
		//create it for the registered Players
		PlayerObject* player = dynamic_cast<PlayerObject*>(*it);
		if(player && _checkPlayer(player))//use _checkPlayer for debug only
		{
			callback(player);
		}
		else
		{
			//an invalid player at this point is like armageddon and Ultymas birthday combined at one time
			//if this happens we need to know about it
			assert(false && "Invalid Player in sendtoInrange");
		}
        it++;
	}

	if(toSelf)
	{
		PlayerObject* player = dynamic_cast<PlayerObject*>(object);
		if(player && _checkPlayer(player))//use _checkPlayer for debug only
		{
			callback(player);
		}
		else
		{
			//an invalid player at this point is like armageddon and Ultymas birthday combined at one time
			//if this happens we need to know about it
			assert(false && "Invalid Player in sendtoInrange");
		}
	}

}


// sends given function to all of the containers registered watchers
void MessageLib::_sendToList(ObjectListType listeners, Object* object, std::function<void (PlayerObject* const player)> callback, bool toSelf)
{
	ObjectListType::const_iterator it		= listeners.begin();
		
	while(it != listeners.end())
	{
		//create it for the registered Players
		PlayerObject* player = dynamic_cast<PlayerObject*>(*it);
		if(player && _checkPlayer(player))//use _checkPlayer for debug only
		{
			callback(player);
		}
		else
		{
			//an invalid player at this point is like armageddon and Ultymas birthday combined at one time
			//if this happens we need to know about it
			assert(false && "Invalid Player in sendtoInrange");
		}
        it++;
	}

	if(toSelf)
	{
		PlayerObject* player = dynamic_cast<PlayerObject*>(object);
		if(player && _checkPlayer(player))//use _checkPlayer for debug only
		{
			callback(player);
		}
		else
		{
			//an invalid player at this point is like armageddon and Ultymas birthday combined at one time
			//if this happens we need to know about it
			assert(false && "Invalid Player in sendtoInrange");
		}
	}

}

//======================================================================================================================
//
// broadcasts a message to all players in range of the given player 
// we use our registered watchers list here so it will be pretty fast :)
// 
void ThreadSafeMessageLib::_sendToInRangeUnreliable(Message* message, Object* const object,uint16 priority, PlayerObjectSet registered_watchers,bool toSelf)
{
	
	_sendToRegisteredWatchers(registered_watchers, object, [this, priority, message, object, toSelf] (PlayerObject* const recipient)
	{
		//save us some cycles if traffic is low
		if(mMessageFactory->HeapWarningLevel() <= 4)
		{	
			// clone our message
 			mMessageFactory->StartMessage();
 			mMessageFactory->addData(message->getData(),message->getSize());
 
 			(recipient->getClient())->SendChannelAUnreliable(mMessageFactory->EndMessage(),recipient->getAccountId(),CR_Client,static_cast<uint8>(priority));	
	
		}
		else
		{	
			bool yn = _checkDistance(recipient->mPosition,object,mMessageFactory->HeapWarningLevel());
			if(yn)
			{
				// clone our message
				mMessageFactory->StartMessage();
				mMessageFactory->addData(message->getData(),message->getSize());
	
				(recipient->getClient())->SendChannelAUnreliable(mMessageFactory->EndMessage(),recipient->getAccountId(),CR_Client,static_cast<uint8>(priority));
			}
		}
	}
	, toSelf);
		
	mMessageFactory->DestroyMessage(message);

}

void ThreadSafeMessageLib::_sendToInRangeUnreliableChat(Message* message, const CreatureObject* object,uint16 priority, uint32 crc, ObjectListType inRangePlayers)
{	

	Message* clonedMessage;

	for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
	{		
		PlayerObject* player = dynamic_cast<PlayerObject*>((*playerIt));
		if(_checkPlayer(player) && (!player->checkIgnoreList(crc)))
		{
 			// clone our message
 			mMessageFactory->StartMessage();
 			mMessageFactory->addData(message->getData(),message->getSize());
 			clonedMessage = mMessageFactory->EndMessage();
		
			// replace the target id
			int8* data = clonedMessage->getData() + 12;
			*((uint64*)data) = player->getId();
			(player->getClient())->SendChannelAUnreliable(clonedMessage,player->getAccountId(),CR_Client,5);
		} 		
	}
}

void ThreadSafeMessageLib::_sendToInRangeUnreliableChatGroup(Message* message, const CreatureObject* object,uint16 priority, uint32 crc, ObjectListType inRangePlayers, uint32 group)
{	

	Message* clonedMessage;

	for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
	{		
		PlayerObject* player = dynamic_cast<PlayerObject*>((*playerIt));
		if(_checkPlayer(player) && (!player->checkIgnoreList(crc)) && (group == player->getGroupId()))
		{
 			// clone our message
 			mMessageFactory->StartMessage();
 			mMessageFactory->addData(message->getData(),message->getSize());
 			clonedMessage = mMessageFactory->EndMessage();
		
			// replace the target id
			int8* data = clonedMessage->getData() + 12;
			*((uint64*)data) = player->getId();
			(player->getClient())->SendChannelAUnreliable(clonedMessage,player->getAccountId(),CR_Client,5);
		} 		
	}
}

void MessageLib::_sendToInRangeUnreliableChat(Message* message, const CreatureObject* object,uint16 priority, uint32 crc, ObjectListType inRangePlayers)
{	

	Message* clonedMessage;

	for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
	{		
		PlayerObject* player = dynamic_cast<PlayerObject*>((*playerIt));
		if(_checkPlayer(player) && (!player->checkIgnoreList(crc)))
		{
 			// clone our message
 			mMessageFactory->StartMessage();
 			mMessageFactory->addData(message->getData(),message->getSize());
 			clonedMessage = mMessageFactory->EndMessage();
		
			// replace the target id
			int8* data = clonedMessage->getData() + 12;
			*((uint64*)data) = player->getId();
			(player->getClient())->SendChannelAUnreliable(clonedMessage,player->getAccountId(),CR_Client,5);
		} 		
	}
}

void ThreadSafeMessageLib::SendSpatialToInRangeUnreliable_(Message* message, Object* const object, ObjectListType listeners, PlayerObject* const player_object) {
    uint32_t senders_name_crc = 0;
    PlayerObject* source_player = NULL;

    // Is this a player object sending the message? If so we need a crc of their name
    // for checking recipient's ignore lists.
    if (object->getType() == ObjType_Player) {
        if ((source_player = dynamic_cast<PlayerObject*>(object))) {
            // Make sure the player is valid and online.
            if (!_checkPlayer(source_player) || !source_player->isConnected()) {
                // This is an invalid player, clean up the message and exit.
                assert(false && "MessageLib::SendSpatialToInRangeUnreliable Message sent from an invalid player, this should never happen");
                mMessageFactory->DestroyMessage(message);
                return;
            }

            std::string tmp = source_player->getFirstName().getAnsi();
            std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
            senders_name_crc = common::memcrc(tmp);

            //@todo: This check for the tutorial is a hack and shouldn't be here.
            if (gWorldConfig->isTutorial()) {
                source_player->getTutorial()->tutorialResponse("chatActive");
            }
        }
    }

    // Create a container for cloned messages.
    // @todo: We shouldn't have to create whole copies of messages that are (with exception of the
    // target id) the exact same. This is a waste of memory especially with denser populations.
    Message* cloned_message = NULL;

    // Create our lambda that we'll use to handle the inrange sending
    auto send_rule = [=, &cloned_message ] (PlayerObject* const recipient) {
        // If the player is not online, or if the sender is in the player's ignore list
        // then pass over this iteration.
        if (!_checkPlayer(recipient) || (senders_name_crc && recipient->checkIgnoreList(senders_name_crc))) {
            //mMessageFactory->DestroyMessage(message);
            return;
        }

        // Clone the message and send it out to this player.
        mMessageFactory->StartMessage();
        mMessageFactory->addData(message->getData(), message->getSize());
        cloned_message = mMessageFactory->EndMessage();

        // Replace the target id.
        char* data = cloned_message->getData() + 12;
        *(reinterpret_cast<uint64_t*>(data)) = recipient->getId();

        recipient->getClient()->SendChannelAUnreliable(cloned_message, recipient->getAccountId(), CR_Client, 5);
    };

    // If no player_object is passed it means this is not an instance, send to the known
    // players of the object initiating the spatial message.
    //
    
    // Loop through the in range players and send them the message.
	//TODO use chatrange players at some point
	_sendToList(listeners,NULL,[object, message, senders_name_crc, this, &cloned_message ] (PlayerObject* const recipient)
	//gSpatialIndexManager->sendToChatRange(object,[object, message, senders_name_crc, this, &cloned_message ] (PlayerObject* const recipient)
	//gContainerManager->sendToRegisteredWatchers(object,[object, message, senders_name_crc, this, &cloned_message ] (PlayerObject* const recipient) 
	{
		// If the player is not online, or if the sender is in the player's ignore list
		// then pass over this iteration.
		if (!_checkPlayer(recipient) || (senders_name_crc && recipient->checkIgnoreList(senders_name_crc))) 
		{
			mMessageFactory->DestroyMessage(message);
			return;
		}

		// Clone the message and send it out to this player.
		mMessageFactory->StartMessage();
		mMessageFactory->addData(message->getData(), message->getSize());
		cloned_message = mMessageFactory->EndMessage();

		// Replace the target id.
		int8* data = cloned_message->getData() + 12;
		*(reinterpret_cast<uint64_t*>(data)) = recipient->getId();

		recipient->getClient()->SendChannelAUnreliable(cloned_message, recipient->getAccountId(), CR_Client, 5);
	}, false);
  
    // If the sender isn't a player or the player isn't still connected we need to destroy the message
    // and exit out at this point, otherwise send the message to the source player.
    // this shouldnt be necessary anymore
	//if (!source_player || !_checkPlayer(source_player)) {
    
	mMessageFactory->DestroyMessage(message);
    //    return;
    //}

    //source_player->getClient()->SendChannelAUnreliable(message, source_player->getAccountId(), CR_Client, 5);
}


void MessageLib::SendSpatialToInRangeUnreliable_(Message* message, Object* const object, ObjectListType listeners, PlayerObject* const player_object) {
    uint32_t senders_name_crc = 0;
    PlayerObject* source_player = NULL;

    // Is this a player object sending the message? If so we need a crc of their name
    // for checking recipient's ignore lists.
    if (object->getType() == ObjType_Player) {
        if ((source_player = dynamic_cast<PlayerObject*>(object))) {
            // Make sure the player is valid and online.
            if (!_checkPlayer(source_player) || !source_player->isConnected()) {
                // This is an invalid player, clean up the message and exit.
                assert(false && "MessageLib::SendSpatialToInRangeUnreliable Message sent from an invalid player, this should never happen");
                mMessageFactory->DestroyMessage(message);
                return;
            }

            BString lowercase_firstname = source_player->getFirstName().getAnsi();
            lowercase_firstname.toLower();
            senders_name_crc = lowercase_firstname.getCrc();

            //@todo: This check for the tutorial is a hack and shouldn't be here.
            if (gWorldConfig->isTutorial()) {
                source_player->getTutorial()->tutorialResponse("chatActive");
            }
        }
    }

    // Create a container for cloned messages.
    // @todo: We shouldn't have to create whole copies of messages that are (with exception of the
    // target id) the exact same. This is a waste of memory especially with denser populations.
    Message* cloned_message = NULL;

    // If no player_object is passed it means this is not an instance, send to the known
    // players of the object initiating the spatial message.
    //
    
    // Loop through the in range players and send them the message.
	//TODO use chatrange players at some point
	_sendToList(listeners,NULL,[object, message, senders_name_crc, this, &cloned_message ] (PlayerObject* const recipient)
	{
		// If the player is not online, or if the sender is in the player's ignore list
		// then pass over this iteration.
		if (!_checkPlayer(recipient) || (senders_name_crc && recipient->checkIgnoreList(senders_name_crc))) 		{
			mMessageFactory->DestroyMessage(message);
			return;
		}

		// Clone the message and send it out to this player.
		mMessageFactory->StartMessage();
		mMessageFactory->addData(message->getData(), message->getSize());
		cloned_message = mMessageFactory->EndMessage();

		// Replace the target id.
		int8* data = cloned_message->getData() + 12;
		*(reinterpret_cast<uint64_t*>(data)) = recipient->getId();

		recipient->getClient()->SendChannelAUnreliable(cloned_message, recipient->getAccountId(), CR_Client, 5);
	}, false);
  
    // If the sender isn't a player or the player isn't still connected we need to destroy the message
    // and exit out at this point, otherwise send the message to the source player.
    // this shouldnt be necessary anymore
    
	mMessageFactory->DestroyMessage(message);
    
}


//======================================================================================================================


void ThreadSafeMessageLib::_sendToInRange(Message* message, Object* const object,uint16 priority, PlayerObjectSet	registered_watchers,bool toSelf) const
{
	
	_sendToRegisteredWatchers(registered_watchers, object, [this, message, object, priority] (PlayerObject* const recipient){
		
		// clone our message
		mMessageFactory->StartMessage();
		mMessageFactory->addData(message->getData(),message->getSize());

		(recipient->getClient())->SendChannelA(mMessageFactory->EndMessage(),recipient->getAccountId(),CR_Client,static_cast<uint8>(priority));

	}
	, toSelf);

	mMessageFactory->DestroyMessage(message);
}

void MessageLib::_sendToInRange(Message* message, Object* const object,uint16 priority, bool toSelf) const
{
	PlayerObjectSet		listeners = object->getRegisteredWatchersCopy();

	_sendToRegisteredWatchers(listeners, object, [this, message, object, priority] (PlayerObject* const recipient){
		
		// clone our message
		mMessageFactory->StartMessage();
		mMessageFactory->addData(message->getData(),message->getSize());

		(recipient->getClient())->SendChannelA(mMessageFactory->EndMessage(),recipient->getAccountId(),CR_Client,static_cast<uint8>(priority));

	}
	, toSelf);
}

//======================================================================================================================
//
// Broadcasts a message to players in group and in range of the given object, used by tutorial and other instances
//
/*
void MessageLib::_sendToInstancedPlayers(Message* message, unsigned char priority, PlayerObject* const player) const {
    if (!_checkPlayer(player)) {
        mMessageFactory->DestroyMessage(message);
        return;
    }

    glm::vec3 position = player->getWorldPosition();

    ObjectListType in_range_players;
    mGrid->GetPlayerViewingRangeCellContents(mGrid->getCellId(position.x, position.z), &in_range_players);

    std::for_each(in_range_players.begin(), in_range_players.end(), [=] (Object* object) {
        PlayerObject* player = dynamic_cast<PlayerObject*>(object);
        if (_checkPlayer(player)) {
            // Clone the message.
            mMessageFactory->StartMessage();
            mMessageFactory->addData(message->getData(),message->getSize());

            (player->getClient())->SendChannelA(mMessageFactory->EndMessage(), player->getAccountId(), CR_Client, priority);
        }
    });

    mMessageFactory->DestroyMessage(message);
}
*/

//======================================================================================================================
//
// Broadcasts a message to players in group and in range of the given object, used by tutorial and other instances
//
void ThreadSafeMessageLib::_sendToInstancedPlayersUnreliable(Message* message,uint16 priority, uint32 groupId, ObjectListType	inRangePlayers) const
{

	if(groupId == 0)	{
		return;
	}

	for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
	{
		PlayerObject* player = dynamic_cast<PlayerObject*>(*playerIt);
		
		if((groupId != player->getGroupId()))
			continue;
		
		if (_checkPlayer(player))		{
			// Clone the message.
			mMessageFactory->StartMessage();
			mMessageFactory->addData(message->getData(),message->getSize());

			(player->getClient())->SendChannelAUnreliable(mMessageFactory->EndMessage(),player->getAccountId(),CR_Client,static_cast<uint8>(priority));
		}

	}
}


void ThreadSafeMessageLib::_sendToInstancedPlayers(Message* message,uint16 priority, uint32 groupId, PlayerObjectSet	inRangePlayers) const
{

	if(groupId == 0)	{
		return;
	}

	for(PlayerObjectSet::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
	{
		PlayerObject* player = dynamic_cast<PlayerObject*>(*playerIt);
		
		if((groupId != player->getGroupId()))
			continue;
		
		if (_checkPlayer(player))		{
			// Clone the message.
			mMessageFactory->StartMessage();
			mMessageFactory->addData(message->getData(),message->getSize());

			(player->getClient())->SendChannelA(mMessageFactory->EndMessage(),player->getAccountId(),CR_Client,static_cast<uint8>(priority));
		}

	}
}


//======================================================================================================================
//
// broadcasts a message to all players on the current zone
//
void ThreadSafeMessageLib::_sendToAll(Message* message,uint16 priority,bool unreliable) const
{
	const PlayerAccMap* const		players		= gWorldManager->getPlayerAccMap();
	PlayerAccMap::const_iterator	playerIt	= players->begin();

	while(playerIt != players->end())
	{
		const PlayerObject* const player = (*playerIt).second;

		if(_checkPlayer(player))
		{
			mMessageFactory->StartMessage();
			mMessageFactory->addData(message->getData(),message->getSize());

			if(unreliable)
			{
				(player->getClient())->SendChannelAUnreliable(mMessageFactory->EndMessage(),player->getAccountId(),CR_Client,static_cast<uint8>(priority));
			}
			else
			{
				(player->getClient())->SendChannelA(mMessageFactory->EndMessage(),player->getAccountId(),CR_Client,static_cast<uint8>(priority));
			}
		}

		++playerIt;
	}

	mMessageFactory->DestroyMessage(message);
}


bool MessageLib::sendCreatePlayer(PlayerObject* player, PlayerObject* target) {
    if (!_checkPlayer(player) || !_checkPlayer(target)) {
        return false;
    }

    sendCreateObjectByCRC(player, target, false);

    if (player == target) {
        sendBaselinesCREO_1(player);
        sendBaselinesCREO_4(player);
    }

    sendBaselinesCREO_3(player, target);
    sendBaselinesCREO_6(player, target);


    sendCreateObjectByCRC(player, target, true);
    sendContainmentMessage(player->getPlayerObjId(), player->getId(), 4, target);

    sendBaselinesPLAY_3(player, target);
    sendBaselinesPLAY_6(player, target);

    if (player == target) {
        sendBaselinesPLAY_8(player, target);
        sendBaselinesPLAY_9(player, target);
    }

    //close the yalp
    sendEndBaselines(player->getPlayerObjId(), target);

    sendPostureMessage(player, target);


    if (player->getParentId()) {
        sendContainmentMessage(player->getId(), player->getParentId(), 4, target);

    }

    //===================================================================================
    // create inventory, datapad, hair, MissionBag and equipped items get created for the player only !!
    // equipped items for other watchers are handled via the equiplists

    //equipped items are already in the creo6 so only send them for ourselves

    sendEndBaselines(player->getId(), target);

    sendUpdatePvpStatus(player, target);

    if (player == target) {
        //request the GRUP baselines from chatserver if grouped
        if (player->getGroupId() != 0) {
            gMessageLib->sendIsmGroupBaselineRequest(player);
        }
    }

    return true;
}

//======================================================================================================================
//
// create creature
//
bool MessageLib::sendCreateCreature(CreatureObject* creature, PlayerObject* target) {
    if (!_checkPlayer(target)) {
        return false;
    }

    sendCreateObjectByCRC(creature, target, false);

    sendBaselinesCREO_3(creature, target);
    sendBaselinesCREO_6(creature, target);

    if(creature->getParentId() && creature->getCreoGroup() != CreoGroup_Vehicle)    {
        sendContainmentMessage(creature->getId(), creature->getParentId(), 0xffffffff, target);
    }


    sendEndBaselines(creature->getId(), target);

    sendUpdatePvpStatus(creature, target);

    sendPostureMessage(creature, target);

    return true;
}
//======================================================================================================================

bool MessageLib::sendCreateStaticObject(TangibleObject* tangible, PlayerObject* target) {
    if(!_checkPlayer(target) || !tangible) {
        DLOG(INFO) << "MessageLib::sendCreateStaticObject No valid player";
        return(false);
    }

    sendCreateObjectByCRC(tangible, target, false);
    sendBaselinesSTAO_3(tangible, target);
    sendBaselinesSTAO_6(tangible, target);
    sendEndBaselines(tangible->getId(), target);

    return true;
}

//======================================================================================================================
//
// create intangible
//
bool MessageLib::sendCreateInTangible(IntangibleObject* intangibleObject,uint64 containmentId,PlayerObject* targetObject)
{
    if(!_checkPlayer(targetObject) || !intangibleObject)
    {
        DLOG(WARNING) << "MessageLib::sendCreateInTangible No valid player";
        return(false);
    }

    sendCreateObjectByCRC(intangibleObject,targetObject,false);
    sendBaselinesITNO_3(intangibleObject,targetObject);
    sendBaselinesITNO_6(intangibleObject,targetObject);
    sendBaselinesITNO_8(intangibleObject,targetObject);
    sendBaselinesITNO_9(intangibleObject,targetObject);
    gMessageLib->sendContainmentMessage(intangibleObject->getId(), containmentId, 0xffffffff, targetObject);
    sendEndBaselines(intangibleObject->getId(),targetObject);

    return true;
}

//======================================================================================================================
//
// create tangible Object in the world
//

bool MessageLib::sendCreateTano(TangibleObject* tangible, PlayerObject* target) {
    if (!_checkPlayer(target))	{
        DLOG(INFO) << "MessageLib::sendCreateTano No valid player";
        return false;
    }


    uint64 parentId = tangible->getParentId();

    sendCreateObjectByCRC(tangible, target, false);

    if(parentId != 0) {
        // its in a cell, container, inventory
        if (parentId != target->getId()) {
            // could be inside a crafting tool
            Object* parent = gWorldManager->getObjectById(parentId);

            if (parent && dynamic_cast<CraftingTool*>(parent)) {
                sendContainmentMessage(tangible->getId(), parentId, 0, target);
            }
            // if equipped, also tie it to the object
            else if (CreatureObject* creature = dynamic_cast<CreatureObject*>(parent)) {
                sendContainmentMessage(tangible->getId(), creature->getId(), 4, target);
            } else {
                sendContainmentMessage(tangible->getId(), tangible->getParentId(), 0xffffffff, target);
            }
        }
        // or tied directly to an object
        else {
            sendContainmentMessage(tangible->getId(), tangible->getParentId(), 4, target);
        }
    } else {
        sendContainmentMessage(tangible->getId(), tangible->getParentId(), 0xffffffff, target);
    }

    sendBaselinesTANO_3(tangible, target);
    sendBaselinesTANO_6(tangible, target);

    sendEndBaselines(tangible->getId(), target);

    return true;
}

//======================================================================================================================
//
// create resource container
//
bool MessageLib::sendCreateResourceContainer(ResourceContainer* resource_container, PlayerObject* target) {
    if(!_checkPlayer(target)) {
        return false;
    }

    sendCreateObjectByCRC(resource_container, target, false);

    uint64_t parent_id = resource_container->getParentId();

    sendContainmentMessage(resource_container->getId(), parent_id, 0xffffffff, target);


    sendBaselinesRCNO_3(resource_container, target);
    sendBaselinesRCNO_6(resource_container, target);


    sendBaselinesRCNO_8(resource_container, target);
    sendBaselinesRCNO_9(resource_container, target);

    sendEndBaselines(resource_container->getId(), target);

    return true;
}

//======================================================================================================================
//
// create building
//
bool MessageLib::sendCreateBuilding(BuildingObject* buildingObject,PlayerObject* playerObject)
{
    if(!_checkPlayer(playerObject))
        return(false);

    bool publicBuilding = true;

    //test buildings on house basis here
    //perhaps move to on cell basis sometime ?
    if(HouseObject* house = dynamic_cast<HouseObject*>(buildingObject))
    {
        house->checkCellPermission(playerObject);
        publicBuilding = buildingObject->getPublic();
    }

    sendCreateObjectByCRC(buildingObject,playerObject,false);

    sendBaselinesBUIO_3(buildingObject,playerObject);
    sendBaselinesBUIO_6(buildingObject,playerObject);

    uint64 buildingId = buildingObject->getId();

    CellObjectList*				cellList	= buildingObject->getCellList();
    CellObjectList::iterator	cellIt		= cellList->begin();

    while(cellIt != cellList->end())
    {
        CellObject* cell = (*cellIt);
        uint64 cellId = cell->getId();

        uint64 count = buildingObject->getMinCellId()-1;
        sendCreateObjectByCRC(cell,playerObject,false);
        gMessageLib->sendContainmentMessage(cellId,buildingId,0xffffffff,playerObject);

        //cell ids are id based for tutorial cells!
        if(cell->getId() <= 2203318222975)
        {
            sendBaselinesSCLT_3(cell,cellId - buildingId,playerObject);
        }
        else
        {
            sendBaselinesSCLT_3(cell,cellId - count,playerObject);
        }
        sendBaselinesSCLT_6(cell,playerObject);

        gThreadSafeMessageLib->sendUpdateCellPermissionMessage(cell,publicBuilding,playerObject);	 //cellpermissions get checked by datatransform
        sendEndBaselines(cellId,playerObject);

        ++cellIt;
    }

    sendEndBaselines(buildingId,playerObject);

    return(true);
}

//======================================================================================================================
//
// create a harvester
//
bool MessageLib::sendCreateHarvester(HarvesterObject* harvester,PlayerObject* player)
{
    if(!_checkPlayer(player))
        return(false);

    sendCreateObjectByCRC(harvester,player,false);

    sendBaselinesHINO_3(harvester,player);
    sendBaselinesHINO_6(harvester,player);
    sendBaselinesHINO_7(harvester,player);


    sendEndBaselines(harvester->getId(),player);

    //int8 effectStr[400];
    //sprintf(effectStr,"clienteffect/lair_med_damage_smoke.cef");
    //sendPlayClientEffectObjectMessage(effectStr,"",harvester,player);

    return(true);
}

//======================================================================================================================
//
// create a factory
//
bool MessageLib::sendCreateFactory(FactoryObject* factory, PlayerObject* target) {
    if (!_checkPlayer(target))
        return false;

    sendCreateObjectByCRC(factory, target, false);

    sendBaselinesINSO_3(factory, target);
    sendBaselinesINSO_6(factory, target);

    TangibleObject* ingredient_hopper = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById(factory->getIngredientHopper()));
    sendCreateTano(ingredient_hopper, target);

    TangibleObject* output_hopper = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById(factory->getOutputHopper()));
    sendCreateTano(output_hopper, target);


    sendEndBaselines(factory->getId(), target);

    return true;
}


bool MessageLib::sendCreateStructure(PlayerStructure* structure, PlayerObject* target) {
    if (!_checkPlayer(target)) {
        return false;
    }

    if (HarvesterObject* harvester = dynamic_cast<HarvesterObject*>(structure)) {
        return sendCreateHarvester(harvester, target);
    }

    else if (HouseObject* house = dynamic_cast<HouseObject*>(structure)) {
        return sendCreateBuilding(house, target);
    }

    else if (FactoryObject* factory = dynamic_cast<FactoryObject*>(structure)) {
        return sendCreateFactory(factory, target);
    }

    else if (structure->getPlayerStructureFamily() == PlayerStructure_Fence) {
        return sendCreateInstallation(structure, target);
    }

    DLOG(INFO) << "MessageLib::sendCreateStructure:ID  : couldnt cast structure" << structure->getId();

    return false;
}


bool MessageLib::sendCreateCamp(TangibleObject* camp, PlayerObject* target) {
    if(!_checkPlayer(target)) {
        return false;
    }

    sendCreateObjectByCRC(camp, target, false);

    sendBaselinesBUIO_3(camp, target);
    sendBaselinesBUIO_6(camp, target);

    sendEndBaselines(camp->getId(), target);

    return true;
}

//======================================================================================================================
//
// create Installation
//
bool MessageLib::sendCreateInstallation(PlayerStructure* structure,PlayerObject* player)
{
    if(!_checkPlayer(player))
        return(false);

    sendCreateObjectByCRC(structure,player,false);

    sendBaselinesINSO_3(structure,player);
    sendBaselinesINSO_6(structure,player);

    uint64 structureId = structure->getId();

    sendEndBaselines(structureId,player);

    return(true);
}

//======================================================================================================================
//
// create manufacturing schematic
// the attributes bool makes the MSCO 3 send the attribute list
//
bool MessageLib::sendCreateManufacturingSchematic(ManufacturingSchematic* manSchem,PlayerObject* playerObject,bool attributes)
{
    if(!_checkPlayer(playerObject))
        return(false);

    sendCreateObjectByCRC(manSchem,playerObject,false);

    // parent should always be a crafting tool for now
    gMessageLib->sendContainmentMessage(manSchem->getId(),manSchem->getParentId(),4,playerObject);

    sendBaselinesMSCO_3(manSchem,playerObject,attributes);
    sendBaselinesMSCO_6(manSchem,playerObject);

    sendBaselinesMSCO_8(manSchem,playerObject);
    sendBaselinesMSCO_9(manSchem,playerObject);

    sendEndBaselines(manSchem->getId(),playerObject);

    return(true);
}

