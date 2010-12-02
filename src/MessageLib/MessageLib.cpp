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

#ifdef _WIN32
#undef ERROR
#endif
#include <glog/logging.h>

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

#include "Common/atMacroString.h"
#include "NetworkManager/DispatchClient.h"
#include "NetworkManager/Message.h"
#include "NetworkManager/MessageDispatch.h"
#include "NetworkManager/MessageFactory.h"
#include "NetworkManager/MessageOpcodes.h"

#include <boost/lexical_cast.hpp>

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
	//get our own Messagefactory so we do not have to worry about the mainthread accessing it
    mMessageFactory = new MessageFactory(gConfig->read<uint32>("GlobalMessageHeap")*1024);
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
		else
		{
			//an invalid player at this point is like armageddon and Ultymas birthday combined at one time
			//if this happens we need to know about it
			assert(false && "Invalid Player in sendtoInrange");
		}
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

	mMessageFactory->DestroyMessage(message);
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
void MessageLib::_sendToInRangeUnreliableChatGroup(Message* message, const CreatureObject* object,uint16 priority, uint32 crc)
{
	
	glm::vec3   position;
	
	//cater for players in cells
	if (object->getParentId())
	{
		position = object->getWorldPosition(); 
	}
	else
	{
		position = object->mPosition;
	}
	
	ObjectListType		inRangePlayers;
	mGrid->GetPlayerViewingRangeCellContents(mGrid->getCellId(position.x, position.z), &inRangePlayers);

	Message* clonedMessage;
	bool failed = false;

	for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
	{		
		PlayerObject* player = dynamic_cast<PlayerObject*>((*playerIt));
		if ((_checkPlayer(player)) && (object->getGroupId()) &&(player->getGroupId() == object->getGroupId())&&(!player->checkIgnoreList(crc)))
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

	mMessageFactory->DestroyMessage(message);
}

//======================================================================================================================
//
// Broadcasts a message to players in group and in range of the given object, used by tutorial and other instances
//
void MessageLib::_sendToInstancedPlayers(Message* message,uint16 priority, PlayerObject* const playerObject) const
{
	if (!_checkPlayer(playerObject))
	{
		mMessageFactory->DestroyMessage(message);
		return;
	}

	glm::vec3   position;
	
	//cater for players in cells
	if (playerObject->getParentId())
	{
		position = playerObject->getWorldPosition(); 
	}
	else
	{
		position = playerObject->mPosition;
	}

	ObjectListType		inRangePlayers;
	mGrid->GetPlayerViewingRangeCellContents(mGrid->getCellId(position.x, position.z), &inRangePlayers);

	for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
	{
		PlayerObject* player = dynamic_cast<PlayerObject*>(*playerIt);
		if (_checkPlayer(player))
		{
			// Clone the message.
			mMessageFactory->StartMessage();
			mMessageFactory->addData(message->getData(),message->getSize());

			(player->getClient())->SendChannelA(mMessageFactory->EndMessage(),player->getAccountId(),CR_Client,static_cast<uint8>(priority));
		}

	}

	mMessageFactory->DestroyMessage(message);
}


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

	mMessageFactory->DestroyMessage(message);
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


//======================================================================================================================
//
// creates all items childobjects
//
/*
bool MessageLib::sendItemChildren(TangibleObject* srcObject,PlayerObject* targetObject)
{
    if(!_checkPlayer(targetObject))
        return(false);

    ObjectIDList*			childObjects		= srcObject->getObjects();
    ObjectIDList::iterator	childObjectsIt		= childObjects->begin();

    while(childObjectsIt != childObjects->end())
    {
        // items
        if(TangibleObject* to = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById((*childObjectsIt))))
        {
            gMessageLib->sendCreateTangible(to,targetObject);
        }

        ++childObjectsIt;
    }

    return(true);
}
*///======================================================================================================================
//
// create player
//
bool MessageLib::sendCreatePlayer(PlayerObject* playerObject,PlayerObject* targetObject)
{
	if(!_checkPlayer(targetObject))
		return(false);

	sendCreateObjectByCRC(playerObject,targetObject,false);

	if(targetObject == playerObject)
	{
		sendBaselinesCREO_1(playerObject);
		sendBaselinesCREO_4(playerObject);
	}

	sendBaselinesCREO_3(playerObject,targetObject);
	sendBaselinesCREO_6(playerObject,targetObject);

	sendCreateObjectByCRC(playerObject,targetObject,true);
	gMessageLib->sendContainmentMessage(playerObject->getPlayerObjId(),playerObject->getId(),4,targetObject);

	sendBaselinesPLAY_3(playerObject,targetObject);
	sendBaselinesPLAY_6(playerObject,targetObject);

	if(targetObject == playerObject)
	{
		sendBaselinesPLAY_8(playerObject,targetObject);
		sendBaselinesPLAY_9(playerObject,targetObject);
	}

	//close the yalp
	sendEndBaselines(playerObject->getPlayerObjId(),targetObject);

	sendPostureMessage(playerObject,targetObject);


    if(playerObject->getParentId())
    {
        gMessageLib->sendContainmentMessage(playerObject->getId(),playerObject->getParentId(),4,targetObject);
    }

	//===================================================================================
	// create inventory, datapad, hair, MissionBag and equipped items get created for the player only !!
	// equipped items for other watchers are handled via the equiplists

    

        
    //equipped items are already in the creo6 so only send them for ourselves

    sendEndBaselines(playerObject->getId(),targetObject);

	sendUpdatePvpStatus(playerObject,targetObject);

    if(targetObject == playerObject)
    {
        // We are actually sending this info from CharacterLoginHandler::handleDispatchMessage at the opCmdSceneReady event.
        // sendFriendListPlay9(playerObject);
        // sendIgnoreListPlay9(playerObject);

        //request the GRUP baselines from chatserver if grouped
        if(playerObject->getGroupId() != 0)
        {
            gMessageLib->sendIsmGroupBaselineRequest(playerObject);
        }
    }
	/*
    //Player mounts
    if(playerObject->checkIfMountCalled())
    {
        if(playerObject->getMount())
        {
            gMessageLib->sendCreateObject(playerObject->getMount(),targetObject);
            if(playerObject->checkIfMounted())
            {
                gMessageLib->sendContainmentMessage(playerObject->getId(), playerObject->getMount()->getId(), 0xffffffff, targetObject);
            }
        }
    }
	*/
    return(true);
}

//======================================================================================================================
//
// create creature
//
bool MessageLib::sendCreateCreature(CreatureObject* creatureObject,PlayerObject* targetObject)
{
	if(!_checkPlayer(targetObject))
		return(false);

	sendCreateObjectByCRC(creatureObject,targetObject,false);

	sendBaselinesCREO_3(creatureObject,targetObject);
	sendBaselinesCREO_6(creatureObject,targetObject);

	if(creatureObject->getParentId() && creatureObject->getCreoGroup() != CreoGroup_Vehicle)
	{
		gMessageLib->sendContainmentMessage(creatureObject->getId(),creatureObject->getParentId(),0xffffffff,targetObject);
	}

	sendEndBaselines(creatureObject->getId(),targetObject);

	sendUpdatePvpStatus(creatureObject,targetObject);

	sendPostureMessage(creatureObject,targetObject);

	return(true);
}
//======================================================================================================================

bool MessageLib::sendCreateStaticObject(TangibleObject* tangibleObject,PlayerObject* targetObject)
{
	if(!_checkPlayer(targetObject) || !tangibleObject)
	{
		DLOG(INFO) << "MessageLib::sendCreateStaticObject No valid player";
		return(false);
	}
	
	sendCreateObjectByCRC(tangibleObject,targetObject,false);
	sendBaselinesSTAO_3(tangibleObject,targetObject);
	sendBaselinesSTAO_6(tangibleObject,targetObject);
	sendEndBaselines(tangibleObject->getId(),targetObject);

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
bool MessageLib::sendCreateTano(TangibleObject* tangibleObject,PlayerObject* targetObject) 
{
	if(!_checkPlayer(targetObject))
	{
		DLOG(INFO) << "MessageLib::sendCreateTangible No valid player";
		return(false);
	}

	
	uint64 parentId = tangibleObject->getParentId();

	sendCreateObjectByCRC(tangibleObject,targetObject,false);

	if(parentId != 0)
	{
		// its in a cell, container, inventory
		if(parentId != targetObject->getId())
		{
			// could be inside a crafting tool
			Object* parent = gWorldManager->getObjectById(parentId);
			CreatureObject* creatureObject = dynamic_cast<CreatureObject*>(parent);

			if(parent && dynamic_cast<CraftingTool*>(parent))
			{
				gMessageLib->sendContainmentMessage(tangibleObject->getId(),parentId,0,targetObject);
			}
			// if equipped, also tie it to the object
			else if(creatureObject)
			{
				Item* item = dynamic_cast<Item*>(tangibleObject);
				gMessageLib->sendContainmentMessage(tangibleObject->getId(),creatureObject->getId(),4,targetObject);				
			}
			else
			{
				gMessageLib->sendContainmentMessage(tangibleObject->getId(),tangibleObject->getParentId(),0xffffffff,targetObject);
			}
		}
		// or tied directly to an object
		else
		{
			gMessageLib->sendContainmentMessage(tangibleObject->getId(),tangibleObject->getParentId(),4,targetObject);
		}
	}
	else
	{
		gMessageLib->sendContainmentMessage(tangibleObject->getId(),tangibleObject->getParentId(),0xffffffff,targetObject);
	}

	sendBaselinesTANO_3(tangibleObject,targetObject);
	sendBaselinesTANO_6(tangibleObject,targetObject);

	sendEndBaselines(tangibleObject->getId(),targetObject);

	return(true);
}

//======================================================================================================================
//
// create resource container
//
bool MessageLib::sendCreateResourceContainer(ResourceContainer* resourceContainer,PlayerObject* targetObject)
{
	if(!_checkPlayer(targetObject))
		return(false);

	sendCreateObjectByCRC(resourceContainer,targetObject,false);

	uint64 parentId = resourceContainer->getParentId();

	gMessageLib->sendContainmentMessage(resourceContainer->getId(),parentId,0xffffffff,targetObject);	
	
	sendBaselinesRCNO_3(resourceContainer,targetObject);
	sendBaselinesRCNO_6(resourceContainer,targetObject);

	sendBaselinesRCNO_8(resourceContainer,targetObject);
	sendBaselinesRCNO_9(resourceContainer,targetObject);

	sendEndBaselines(resourceContainer->getId(),targetObject);

	return(true);
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
bool MessageLib::sendCreateFactory(FactoryObject* factory,PlayerObject* player)
{
	if(!_checkPlayer(player))
		return(false);

	sendCreateObjectByCRC(factory,player,false);

	sendBaselinesINSO_3(factory,player);
	sendBaselinesINSO_6(factory,player);

	TangibleObject* InHopper = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById(factory->getIngredientHopper()));
	sendCreateTano(InHopper,player);

	TangibleObject* OutHopper = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById(factory->getOutputHopper()));
	sendCreateTano(OutHopper,player);


	sendEndBaselines(factory->getId(),player);

	//int8 effectStr[400];
	//sprintf(effectStr,"clienteffect/lair_med_damage_smoke.cef");
	//sendPlayClientEffectObjectMessage(effectStr,"",harvester,player);

	return(true);
}

//======================================================================================================================
//
// create a structure
//
bool MessageLib::sendCreateStructure(PlayerStructure* structure,PlayerObject* player)
{
	if(!_checkPlayer(player))
		return(false);

	if(HarvesterObject* harvester = dynamic_cast<HarvesterObject*>(structure))
	{
		return(sendCreateHarvester(harvester, player));
	}
	else if(HouseObject* house = dynamic_cast<HouseObject*>(structure))
	{
		return(sendCreateBuilding(house, player));
	}
	else if(FactoryObject* factory = dynamic_cast<FactoryObject*>(structure))
	{
		return(sendCreateFactory(factory, player));
	}

	if(structure->getPlayerStructureFamily() == PlayerStructure_Fence)
	{
		return(sendCreateInstallation(structure, player));
	}

	DLOG(INFO) << "MessageLib::sendCreateStructure:ID  : couldnt cast structure" << structure->getId();

	return(false);
}

//======================================================================================================================
//
// create camp
//
bool MessageLib::sendCreateCamp(TangibleObject* camp,PlayerObject* player)
{
	if(!_checkPlayer(player))
		return(false);

	sendCreateObjectByCRC(camp,player,false);

	sendBaselinesBUIO_3(camp,player);
	sendBaselinesBUIO_6(camp,player);

	uint64 campId = camp->getId();

	sendEndBaselines(campId,player);

	return(true);
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

//======================================================================================================================



