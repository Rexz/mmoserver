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

#include "ZoneServer/Bank.h"
#include "ZoneServer/CellObject.h"
#include "ZoneServer/CharSheetManager.h"
#include "ZoneServer/Conversation.h"
#include "ZoneServer/CraftingTool.h"
#include "ZoneServer/CurrentResource.h"
#include "ZoneServer/ManufacturingSchematic.h"
#include "ZoneServer/NPCObject.h"
#include "ZoneServer/ObjectControllerOpcodes.h"
#include "ZoneServer/ObjectFactory.h"
#include "ZoneServer/PlayerObject.h"
#include "ZoneServer/ArtisanManager.h"
#include "ZoneServer/TravelTerminal.h"
#include "ZoneServer/UIOpcodes.h"
#include "ZoneServer/Wearable.h"
#include "ZoneServer/WorldManager.h"
#include "ZoneServer/ZoneOpcodes.h"



#include "Common/byte_buffer.h"
#include "Common/atMacroString.h"
#include "NetworkManager/DispatchClient.h"
#include "NetworkManager/Message.h"
#include "NetworkManager/MessageDispatch.h"
#include "NetworkManager/MessageFactory.h"
#include "NetworkManager/MessageOpcodes.h"

#include <boost/lexical_cast.hpp>

#ifdef _MSC_VER
#include <regex>  // NOLINT
#else
#include <boost/regex.hpp>  // NOLINT
#endif

#ifdef WIN32
using ::std::regex;
using ::std::smatch;
using ::std::regex_search;
#else
using ::boost::regex;
using ::boost::smatch;
using ::boost::regex_search;
#endif

using ::common::ByteBuffer;
using ::common::OutOfBand;

//======================================================================================================================
//
// create function, used for all objects
//
bool MessageLib::sendCreateObjectByCRC(Object* object,const PlayerObject* const targetObject,bool player) const
{
    if(!object || !(_checkPlayer(targetObject)))
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opSceneCreateObjectByCrc);

    if(!player)
        mMessageFactory->addUint64(object->getId());
    else
        mMessageFactory->addUint64(dynamic_cast<PlayerObject*>(object)->getPlayerObjId());

    // direction
    mMessageFactory->addFloat(object->mDirection.x);
    mMessageFactory->addFloat(object->mDirection.y);
    mMessageFactory->addFloat(object->mDirection.z);
    mMessageFactory->addFloat(object->mDirection.w);

    // position
    mMessageFactory->addFloat(object->mPosition.x);
    mMessageFactory->addFloat(object->mPosition.y);
    mMessageFactory->addFloat(object->mPosition.z);

    if(!player)
        mMessageFactory->addUint32(object->getModelString().getCrc());
    else
        mMessageFactory->addUint32(0x619bae21); // shared_player.iff

    mMessageFactory->addUint8(0);

    (targetObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), targetObject->getAccountId(), CR_Client, 5);

    return(true);
}

//======================================================================================================================
//
// End Baselines
//
bool MessageLib::sendEndBaselines(uint64 objectId,const PlayerObject* const targetObject) const
{
    if(!targetObject || !targetObject->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opSceneEndBaselines);
    mMessageFactory->addUint64(objectId);

    (targetObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), targetObject->getAccountId(), CR_Client, 2);

    return(true);
}

//======================================================================================================================
//
// Scene Destroy Object
//
//
bool MessageLib::sendDestroyObject(uint64 objectId, PlayerObject* const targetObject) const
{
    if(!targetObject || !targetObject->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opSceneDestroyObject);
    mMessageFactory->addUint64(objectId);
    mMessageFactory->addUint8(0);

    (targetObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), targetObject->getAccountId(), CR_Client, 3);

    return(true);
}

//======================================================================================================================

void ThreadSafeMessageLib::sendDestroyObject(uint64 objectId, CreatureObject* const owner)
{
	if(!owner)    {
        return;
    }

	PlayerObjectSet		listeners = *owner ->getRegisteredWatchers();

	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opSceneDestroyObject);
		mMessageFactory->addUint64(objectId);
		mMessageFactory->addUint8(0);

		_sendToInRange(mMessageFactory->EndMessage(), owner, 3, listeners, false);
	}
	);
}

//==============================================================================================================
//
// this deletes an object from the client
//
void ThreadSafeMessageLib::sendDestroyObject_InRangeofObject(Object* object)
{
    if(!object)    {
        return;
    }
	
	PlayerObjectSet		listeners = *object->getRegisteredWatchers();

	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opSceneDestroyObject);
		mMessageFactory->addUint64(object->getId());
		mMessageFactory->addUint8(0);

		_sendToInRange(mMessageFactory->EndMessage(), object, 3, listeners, false);
	}
	);
}

//======================================================================================================================
//
// updates an object parent<->child relationship
//
void ThreadSafeMessageLib::sendContainmentMessage(uint64 objectId,uint64 parentId,uint32 linkType,const PlayerObject* const targetObject) 
{
    if(!_checkPlayer(targetObject))    {
        return;
    }
	
	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opUpdateContainmentMessage);

		mMessageFactory->addUint64(objectId);
		mMessageFactory->addUint64(parentId);
		mMessageFactory->addUint32(linkType);

		(targetObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), targetObject->getAccountId(), CR_Client, 4);
	}
	);

    return;
}

//======================================================================================================================
//
// updates an object parent<->child relationship
//
void MessageLib::sendContainmentMessage(uint64 objectId,uint64 parentId,uint32 linkType,const PlayerObject* const targetObject) 
{
    if(!_checkPlayer(targetObject))    {
        return;
    }
	
	//active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opUpdateContainmentMessage);

		mMessageFactory->addUint64(objectId);
		mMessageFactory->addUint64(parentId);
		mMessageFactory->addUint32(linkType);

		(targetObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), targetObject->getAccountId(), CR_Client, 4);
	//}
	//);

    return;
}



//======================================================================================================================
//
// updates an object parent<->child relationship
//

void ThreadSafeMessageLib::sendContainmentMessage_InRange(uint64 objectId,uint64 parentId,uint32 linkType,CreatureObject* targetObject)
{
    if(!targetObject)    {
        return;
    }

	PlayerObjectSet		listeners = *targetObject->getRegisteredWatchers();
	

	//auto task = std::make_shared<boost::packaged_task<void>>([=] () {
	active_.Send([=] {
		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opUpdateContainmentMessage);

		mMessageFactory->addUint64(objectId);
		mMessageFactory->addUint64(parentId);
		mMessageFactory->addUint32(linkType);


		_sendToInRange(mMessageFactory->EndMessage(), targetObject, 5, listeners);
	}
	);


	//active_.Send([task]{
      // (*task)();
	//});
}

//======================================================================================================================
//
// updates an object parent<->child relationship
//

void MessageLib::sendContainmentMessage_InRange(uint64 objectId,uint64 parentId,uint32 linkType,CreatureObject* targetObject)
{
    if(!targetObject)    {
        return;
    }

	//PlayerObjectSet		listeners = *targetObject->getRegisteredWatchers();
	
	//active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opUpdateContainmentMessage);

		mMessageFactory->addUint64(objectId);
		mMessageFactory->addUint64(parentId);
		mMessageFactory->addUint32(linkType);

		_sendToInRange(mMessageFactory->EndMessage(), targetObject, 5);
	//}
	//);

}

//======================================================================================================================
//
// Heartbeat, simple keep alive
//
void ThreadSafeMessageLib::sendHeartBeat(DispatchClient* client)
{
    if(!client)    {
        return;
    }
	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opHeartBeat);

		client->SendChannelAUnreliable(mMessageFactory->EndMessage(), client->getAccountId(), CR_Client, 1);

	}
	);
}

//======================================================================================================================
//
// opened container
//
void ThreadSafeMessageLib::sendOpenedContainer(uint64 objectId, PlayerObject* targetObject)
{
   
	 if(!_checkPlayer(targetObject))    {
        return;
    }
	
	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opOpenedContainer);
		mMessageFactory->addUint32(0xFFFFFFFF);
		mMessageFactory->addUint64(objectId);
		mMessageFactory->addUint16(0);
		mMessageFactory->addUint8(0);

		(targetObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), targetObject->getAccountId(), CR_Client, 2);
	}
	);

}

//======================================================================================================================
//
// world position update
//
void ThreadSafeMessageLib::sendUpdateTransformMessage(MovingObject* object)
{
	PlayerObjectSet registered_watchers = *object->getRegisteredWatchers();

	uint32		moveCount	= object->incInMoveCount();
	float		angle		= object->rotation_angle();
    glm::vec3   position	= object->mPosition;
	uint64		id			= object->getId();


	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opUpdateTransformMessage);
		mMessageFactory->addUint64(id);
		mMessageFactory->addUint16(static_cast<uint16>(position.x * 4.0f + 0.5f));
		mMessageFactory->addUint16(static_cast<uint16>(position.y * 4.0f + 0.5f));
		mMessageFactory->addUint16(static_cast<uint16>(position.z * 4.0f + 0.5f));
		mMessageFactory->addUint32(moveCount);

		mMessageFactory->addUint8(static_cast<uint8>(glm::length(position) * 4.0f + 0.5f));
		mMessageFactory->addUint8(static_cast<uint8>(angle / 0.0625f));

		_sendToInRangeUnreliable(mMessageFactory->EndMessage(),object,8, registered_watchers, true);
	}
	);
}

//======================================================================================================================
//
// cell position update
//
void ThreadSafeMessageLib::sendUpdateTransformMessageWithParent(MovingObject* object)
{
    if(!object){
        return;
    }

	//get our members
	PlayerObjectSet registered_watchers = *object->getRegisteredWatchers();

	uint32		moveCount	= object->incInMoveCount();
	float		angle		= object->rotation_angle();
    glm::vec3   position	= object->mPosition;
	uint64		parent		= object->getParentId();
	uint64		id			= object->getId();

	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opUpdateTransformMessageWithParent);
		mMessageFactory->addUint64(parent);
		mMessageFactory->addUint64(id);
		mMessageFactory->addUint16(static_cast<uint16>(position.x * 8.0f + 0.5f));
		mMessageFactory->addUint16(static_cast<uint16>(position.y * 8.0f + 0.5f));
		mMessageFactory->addUint16(static_cast<uint16>(position.z * 8.0f + 0.5f));
		mMessageFactory->addUint32(moveCount);

		mMessageFactory->addUint8(static_cast<uint8>(glm::length(position) * 8.0f + 0.5f));
		mMessageFactory->addUint8(static_cast<uint8>( angle / 0.0625f));

		_sendToInRangeUnreliable(mMessageFactory->EndMessage(), object, 8, registered_watchers);
	}
	);
}

//======================================================================================================================
//
// world position update, to be used with Tutorial
//
void ThreadSafeMessageLib::sendUpdateTransformMessage(MovingObject* object, PlayerObject* player)
{
	if(!object || (!_checkPlayer(player)))    {
        return;
    }

    //get our members
	ObjectListType		inRangePlayers;
	mGrid->GetPlayerViewingRangeCellContents(player->getGridBucket(), &inRangePlayers);


	uint32		moveCount	= object->incInMoveCount();
	float		angle		= object->rotation_angle();
    glm::vec3   position	= object->mPosition;
	uint32		group		= player->getGroupId();
	uint64		id			= object->getId();

	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opUpdateTransformMessage);
		mMessageFactory->addUint64(id);
		mMessageFactory->addUint16(static_cast<uint16>(position.x * 4.0f + 0.5f));
		mMessageFactory->addUint16(static_cast<uint16>(position.y * 4.0f + 0.5f));
		mMessageFactory->addUint16(static_cast<uint16>(position.z * 4.0f + 0.5f));
		mMessageFactory->addUint32(moveCount);

		mMessageFactory->addUint8(static_cast<uint8>(glm::length(position) * 4.0f + 0.5f));
		mMessageFactory->addUint8(static_cast<uint8>(angle / 0.0625f));

		_sendToInstancedPlayersUnreliable(mMessageFactory->EndMessage(), 8, group, inRangePlayers);
	}
	);
}

//======================================================================================================================
//
// cell position update, to be used with Tutorial
//
void ThreadSafeMessageLib::sendUpdateTransformMessageWithParent(MovingObject* object, PlayerObject* player)
{
    if(!object || !(_checkPlayer(player)))    {
        return;
    }

	//get our members
	ObjectListType		inRangePlayers;
	mGrid->GetPlayerViewingRangeCellContents(player->getGridBucket(), &inRangePlayers);

	uint32		moveCount	= object->incInMoveCount();
	float		angle		= object->rotation_angle();
    glm::vec3   position	= object->mPosition;
	uint32		group		= player->getGroupId();
	uint64		id			= object->getId();
	uint64		parent		= object->getParentId();

	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opUpdateTransformMessageWithParent);
		mMessageFactory->addUint64(parent);
		mMessageFactory->addUint64(id);
		mMessageFactory->addUint16(static_cast<uint16>(position.x * 8.0f + 0.5f));
		mMessageFactory->addUint16(static_cast<uint16>(position.y * 8.0f + 0.5f));
		mMessageFactory->addUint16(static_cast<uint16>(position.z * 8.0f + 0.5f));
		mMessageFactory->addUint32(object->getInMoveCount());

		mMessageFactory->addUint8(static_cast<uint8>(glm::length(position) * 8.0f + 0.5f));
		mMessageFactory->addUint8(static_cast<uint8>(angle / 0.0625f));

		_sendToInstancedPlayersUnreliable(mMessageFactory->EndMessage(), 8, group, inRangePlayers);
	}
	);
}

//======================================================================================================================
//
// TODO: figure out unknown values
//
bool MessageLib::sendChatServerStatus(uint8 unk1,uint8 unk2,DispatchClient* client)
{
    if(!client)
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opChatServerStatus);
    mMessageFactory->addUint8(unk1);
    mMessageFactory->addUint8(unk2);

    client->SendChannelA(mMessageFactory->EndMessage(),client->getAccountId(),CR_Client,2);

    return(true);
}

//======================================================================================================================
//
// parameters, unknown
//
bool MessageLib::sendParameters(uint32 parameters,DispatchClient* client)
{
    if(!client)
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opParametersMessage);
    mMessageFactory->addUint32(parameters);

    client->SendChannelA(mMessageFactory->EndMessage(), client->getAccountId(), CR_Client, 2);

    return(true);
}

//=======================================================================================================================
//
// start scene
//
bool MessageLib::sendStartScene(uint64 zoneId,PlayerObject* player)
{
    if(!player || !player->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opCmdStartScene);
    mMessageFactory->addUint8(0);
    mMessageFactory->addUint64(player->getId());

    BString mapName = gWorldManager->getTrnFileThis();
    mMessageFactory->addString(mapName);

    mMessageFactory->addFloat(player->mPosition.x);
    mMessageFactory->addFloat(player->mPosition.y);
    mMessageFactory->addFloat(player->mPosition.z);

    mMessageFactory->addString(player->getModelString());
    mMessageFactory->addUint64(zoneId);

    (player->getClient())->SendChannelA(mMessageFactory->EndMessage(), player->getAccountId(), CR_Client, 9);

    return(true);
}

//=======================================================================================================================
//
// planet time update
//
bool MessageLib::sendServerTime(uint64 time,DispatchClient* client)
{
    if(!client)
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opServerTimeMessage);
    mMessageFactory->addUint64(time);

    client->SendChannelA(mMessageFactory->EndMessage(), client->getAccountId(), CR_Client, 2);

    return(true);
}

//=======================================================================================================================
//
// scene ready acknowledge
//
bool MessageLib::sendSceneReady(DispatchClient* client)
{
    if(!client)
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opCmdSceneReady);

    client->SendChannelA(mMessageFactory->EndMessage(), client->getAccountId(), CR_Client, 1);

    return(true);
}

//=======================================================================================================================
//
// scene ready acknowledge to chatseerver
//
bool MessageLib::sendSceneReadyToChat(DispatchClient* client)
{
    if(!client)
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opChatNotifySceneReady);

    client->SendChannelA(mMessageFactory->EndMessage(), client->getAccountId(), CR_Chat, 4);

    return(true);
}
//=======================================================================================================================
//
// open the ticket terminal ui
//
bool MessageLib::sendEnterTicketPurchaseModeMessage(TravelTerminal* terminal,PlayerObject* targetObject)
{
    if(!terminal || !targetObject || !targetObject->isConnected())
    {
        return(false);
    }

    BString planet = gWorldManager->getPlanetNameThis();

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opEnterTicketPurchaseModeMessage);
    mMessageFactory->addString(planet);
    mMessageFactory->addString(terminal->getPosDescriptor());
    mMessageFactory->addUint8(0);

    (targetObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), targetObject->getAccountId(), CR_Client, 4);

    return(true);
}


//=======================================================================================================================
//
// system message
//

void ThreadSafeMessageLib::SendSystemMessage(const std::wstring custom_message,const PlayerObject* const player, bool chatbox_only, bool send_to_inrange) {

	PlayerObjectSet  listeners;
	if(player)    {
		listeners = player->getRegisteredWatchersCopy();   
    }

	active_.Send([=] {
		// Use regex to check if the chat string matches the stf string format.
		static const regex pattern("@([a-zA-Z0-9/_]+):([a-zA-Z0-9_]+)");
		smatch result;

		std::string stf_string(custom_message.begin(), custom_message.end());

		// If it's an exact match (2 sub-patterns + the full string = 3 elements) it's an stf string.
		// Reroute the call to the appropriate overload.
		if (regex_search(stf_string, result, pattern))
		{
			std::string file(result[1].str());
			std::string string(result[2].str());

			// @todo: Because of dependency on other non-const functions that should be const we need to cast away the constness here.
			// Remove this in the future as the other const correctness problems are dealt with.
			return SendSystemMessage_(L"", OutOfBand(file, string), const_cast<PlayerObject*>(player), chatbox_only, send_to_inrange, listeners);
		}

		// @todo: Because of dependency on other non-const functions that should be const we need to cast away the constness here.
		// Remove this in the future as the other const correctness problems are dealt with.
		SendSystemMessage_(custom_message, OutOfBand(), const_cast<PlayerObject*>(player), chatbox_only, send_to_inrange, listeners);
	}
	);
}

void ThreadSafeMessageLib::SendSystemMessage(const OutOfBand prose, const PlayerObject* const player, bool chatbox_only, bool send_to_inrange) {
    
	PlayerObjectSet		listeners;
	if(player)    {
		listeners = player->getRegisteredWatchersCopy();   
    }
	
	auto my_copy = std::make_shared<OutOfBand>(prose);

	// @todo: Because of dependency on other non-const functions that should be const we need to cast away the constness here.
    // Remove this in the future as the other const correctness problems are dealt with.
	auto task = std::make_shared<boost::packaged_task<void>>([=, &my_copy](){
		SendSystemMessage_(L"", *my_copy, const_cast<PlayerObject*>(player), chatbox_only, send_to_inrange, listeners);
	}
	);
	
}
//=======================================================================================================================
//
// is called by messages in the activeThread
//
void ThreadSafeMessageLib::SendSystemMessage_(const std::wstring& custom_message, const OutOfBand& prose, PlayerObject* player, bool chatbox_only, bool send_to_inrange, PlayerObjectSet listeners) {

	const ByteBuffer* attachment = prose.Pack();

	//auto task = std::make_shared<boost::packaged_task<void>>([=](){

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opChatSystemMessage);

		// This determines the bitmask switch for where a message is displayed.
		if (chatbox_only) {
			mMessageFactory->addUint8(2);
		} else {
			mMessageFactory->addUint8(0);
		}

		if (custom_message.length()) {
			mMessageFactory->addString(custom_message);
			mMessageFactory->addUint32(0);
		} else {
			mMessageFactory->addUint32(0);

		
			mMessageFactory->addData(attachment->data(), attachment->size());
		}
	
		// If a player was passed in then only send out the message to the appropriate parties.
		if (player) {
			// If the send_to_inrange flag was set then send out to everyone in-range of the player.
			if (send_to_inrange) {
			
				//threadReliableMessage(mMessageFactory->EndMessage(), player, wire_chatField, 8);
            
				_sendToInRange(mMessageFactory->EndMessage(), player, 8, listeners, true);
			} else {
				//threadReliableMessage(mMessageFactory->EndMessage(), player, wire_singlePlayer, 8);
				(player->getClient())->SendChannelA(mMessageFactory->EndMessage(), player->getAccountId(), CR_Client, 5);
			}
		} else {
			// If no player was passed send the system message to everyone.
			//threadReliableMessage(mMessageFactory->EndMessage(), player, wire_allReliable, 8);
			_sendToAll(mMessageFactory->EndMessage(), 8, true);
		}
	//}
	//);
		
}


//======================================================================================================================
//
// error message
//
void ThreadSafeMessageLib::sendErrorMessage(PlayerObject* playerObject,BString errType,BString errMsg,uint8 fatal)
{
    if(!_checkPlayer(playerObject))    {
        return;
    }
	
	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opErrorMessage);
		mMessageFactory->addString(errType);
		mMessageFactory->addString(errMsg);
		mMessageFactory->addUint8(fatal);

		(playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 3);

	}
	);
}

//======================================================================================================================
//
// weather update
//
void ThreadSafeMessageLib::sendWeatherUpdate(const glm::vec3 cloudVec, uint32 weatherType, PlayerObject* player)
{

	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opServerWeatherMessage);
		mMessageFactory->addUint32(weatherType);
		mMessageFactory->addFloat(cloudVec.x);
		mMessageFactory->addFloat(cloudVec.y);
		mMessageFactory->addFloat(cloudVec.z);

		Message* message = mMessageFactory->EndMessage();
		if(player)
		{
			if(player->isConnected())
			{
				(player->getClient())->SendChannelA(message,player->getAccountId(),CR_Client,3);
			}
			else
			{
				//never ever leave a message either undestroyed or unfinished!!!!
				message->setPendingDelete(true);
			}
		}
		else
		{
			_sendToAll(message,3);
		}
	}
	);
}

//======================================================================================================================
//
// update cell permissions
//
void ThreadSafeMessageLib::sendUpdateCellPermissionMessage(CellObject* cellObject,uint8 permission,PlayerObject* playerObject)
{
	uint64 cellId = cellObject->getId();

	active_.Send([=] {
		if(!cellObject || (!_checkPlayer(playerObject)))	{
			return;
		}

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opUpdateCellPermissionMessage);
		mMessageFactory->addUint8(permission);
		mMessageFactory->addUint64(cellId);

		(playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 3);	
	}
	);
}

//======================================================================================================================
//
// play a clienteffect, if a player is given it will be sent to him only, otherwise to everyone in range of the effectObject
//

void ThreadSafeMessageLib::sendPlayClientEffectObjectMessage(std::string effect, BString location,Object* effectObject,PlayerObject* playerObject){

	if(!_checkPlayer(playerObject))    {
        return;
    }

	uint64 effectId = effectObject->getId();

	PlayerObjectSet		listeners = *effectObject->getRegisteredWatchers();

	//add to the active thread for processing
	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opPlayClientEffectObjectMessage);
		mMessageFactory->addString(effect);
		mMessageFactory->addString(location);
		mMessageFactory->addUint64(effectId);
		mMessageFactory->addUint16(0);

		if(playerObject)
		{
			(playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 5);
		}
		else
		{
			if(PlayerObject* playerTargetObject = dynamic_cast<PlayerObject*>(effectObject))
			{
				_sendToInRange(mMessageFactory->EndMessage(), playerTargetObject, 5, listeners);
			}
			else
			{
				_sendToInRange(mMessageFactory->EndMessage(), effectObject, 5, listeners, false);
			}
		}

		return;
	}
	);
}

//======================================================================================================================
//
// play a clienteffect at location
//

void ThreadSafeMessageLib::sendPlayClientEffectLocMessage(std::string effect, const glm::vec3& pos, PlayerObject* targetObject)

{
    if(!_checkPlayer(targetObject))    {
        return;
    }

	active_.Send([=] {
		BString		planet = gWorldManager->getPlanetNameThis();


		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opPlayClientEffectLocMessage);
		mMessageFactory->addString(effect);
		mMessageFactory->addString(planet);
		mMessageFactory->addFloat(pos.x);
		mMessageFactory->addFloat(pos.y);
		mMessageFactory->addFloat(pos.z);
		mMessageFactory->addUint64(0);
		mMessageFactory->addUint32(0);

		(targetObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), targetObject->getAccountId(), CR_Client, 8);
	}
	);
}

//======================================================================================================================
//
// survey response
//
ResourceLocation MessageLib::sendSurveyMessage(uint16 range,uint16 points,CurrentResource* resource,PlayerObject* targetObject)
{
    if(!resource || !(_checkPlayer(targetObject)))    {
        return(ResourceLocation());
    }

	float				posX,posZ,ratio;
	ResourceLocation	highestDist;

	// init to lowest possible value
	uint8		 step		= range / (points - 1);
	highestDist.ratio		= -1.0f;

	// using mY of highest ratio vector, to determine if resource actually was found
	highestDist.position.y = 0.0f;

	range = (range >> 1);

	mMessageFactory->StartMessage();
	mMessageFactory->addUint32(opSurveyMessage);

	mMessageFactory->addUint32(points*points);

	for(int16 i = -range; i <= range; i+=step)
	{
		for(int16 j = -range; j <= range; j+=step)
		{
			posX	= targetObject->mPosition.x + (float)i;
			posZ	= targetObject->mPosition.z + (float)j;
			ratio	= resource->getDistribution((int)posX + 8192,(int)posZ + 8192);

			if(ratio > highestDist.ratio)
			{
				highestDist.position.x = posX;
				highestDist.position.z = posZ;
				highestDist.ratio = ratio;
			}

			mMessageFactory->addFloat(posX);
			mMessageFactory->addFloat(0.0f);
			mMessageFactory->addFloat(posZ);
			mMessageFactory->addFloat(ratio);
		}
	}

	(targetObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), targetObject->getAccountId(), CR_Client, 2);

	if(highestDist.ratio >= 0.1f)
	{
		highestDist.position.y = 5.0f;
	}

	return highestDist;
}

//======================================================================================================================
//
// send current badges
//
void ThreadSafeMessageLib::sendBadges(PlayerObject* srcObject,PlayerObject* targetObject)
{
    if(!srcObject ||!(_checkPlayer(targetObject)) )   {
        return;
    }

	uint32		badgeMap[15];
	for(uint32 i = 0; i < 15; i++)
	{
		badgeMap[i] = 0;
	}

	BadgesList badges		= *srcObject->getBadges();
	BadgesList::iterator it = badges.begin();

	while(it != badges.end())
	{
		uint32 index	= (uint32)floor((double)((*it)/32));
		badgeMap[index] = badgeMap[index] ^ (1 << ((*it)%32));

		++it;
	}

	auto task = std::make_shared<boost::packaged_task<void>>([=, &badgeMap] {
		
		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opBadgesResponseMessage);
		mMessageFactory	->addUint64(srcObject->getId());
		mMessageFactory->addUint32(15);

		for(uint32 i = 0; i < 15; i++)
		{
			mMessageFactory->addUint32(badgeMap[i]);
		}

		// unknown
		mMessageFactory->addUint8(0);

		(targetObject->getClient())->SendChannelA(mMessageFactory->EndMessage(),targetObject->getAccountId(),CR_Client,3);

		return;
	}
	);
}

//======================================================================================================================
//
// play music message
//
void ThreadSafeMessageLib::sendPlayMusicMessage(uint32 soundId,PlayerObject* targetObject)
{
	if(!_checkPlayer(targetObject))		{
		return;
	}

	std::string sound = gWorldManager->getSound(soundId);

	active_.Send([=] {
		

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opPlayMusicMessage);
		mMessageFactory->addString(sound);
		mMessageFactory->addUint64(0);
		mMessageFactory->addUint32(1);
		mMessageFactory->addUint8(0);

		(targetObject->getClient())->SendChannelA(mMessageFactory->EndMessage(),targetObject->getAccountId(),CR_Client,5);

		return;;
	}
	);
}

//======================================================================================================================
//
// play music message, used by non-player objects.
//
void ThreadSafeMessageLib::sendPlayMusicMessage(uint32 soundId, Object* creatureObject)
{
	if(!creatureObject)	{
		return;
	}
	
	PlayerObjectSet		listeners = *creatureObject->getRegisteredWatchers();

	std::string sound = gWorldManager->getSound(soundId);

	active_.Send([=] {
		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opPlayMusicMessage);
		mMessageFactory->addString(sound);
		mMessageFactory->addUint64(0);
		mMessageFactory->addUint32(1);
		mMessageFactory->addUint8(0);

		_sendToInRange(mMessageFactory->EndMessage(), creatureObject,5, listeners, false);

		return;
	}
	);
}

//======================================================================================================================
//
// character sheet
//
bool MessageLib::sendCharacterSheetResponse(PlayerObject* playerObject)
{
    if(!_checkPlayer(playerObject))    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opCharacterSheetResponseMessage);

    mMessageFactory->addUint64(0);

    if(playerObject->getBindPlanet() == -1)
    {
        mMessageFactory->addFloat(0);
        mMessageFactory->addFloat(0);
        mMessageFactory->addFloat(0);
        mMessageFactory->addUint16(0);
    }
    else
    {
        glm::vec3 bindLoc = playerObject->getBindCoords();

        mMessageFactory->addFloat(bindLoc.x);
        mMessageFactory->addFloat(bindLoc.y);
        mMessageFactory->addFloat(bindLoc.z);
        BString bindPlanet(gWorldManager->getPlanetNameById(playerObject->getBindPlanet()));
        mMessageFactory->addString(bindPlanet);
    }

    // unknown (the unused bank position)
    mMessageFactory->addUint64(0);
    mMessageFactory->addUint32(0);

    // bank
    Bank* bank = dynamic_cast<Bank*>(playerObject->getEquipManager()->getEquippedObject(CreatureEquipSlot_Bank));

    if(!bank || bank->planet() == -1)
    {
        mMessageFactory->addString(BString("unknown"));
    }
    else
    {
        mMessageFactory->addString(BString(gWorldManager->getPlanetNameById(bank->planet())));
    }

    if(playerObject->getHomePlanet() == -1)
    {
        mMessageFactory->addFloat(0);
        mMessageFactory->addFloat(0);
        mMessageFactory->addFloat(0);
        mMessageFactory->addUint16(0);
    }
    else
    {
        glm::vec3 homeLoc = playerObject->getHomeCoords();

        mMessageFactory->addFloat(homeLoc.x);
        mMessageFactory->addFloat(homeLoc.y);
        mMessageFactory->addFloat(homeLoc.z);
        mMessageFactory->addString(BString(gWorldManager->getPlanetNameById(playerObject->getHomePlanet())));
    }

    mMessageFactory->addString(playerObject->getMarriage());
    mMessageFactory->addUint32(playerObject->getLots());

    // neutral
    if(playerObject->getFaction().getCrc() == 0x1fdc3051)
        mMessageFactory->addUint32(0);
    else
        mMessageFactory->addUint32(playerObject->getFaction().getCrc());

    // Faction State see wiki for details
    mMessageFactory->addUint32(0);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(),playerObject->getAccountId(),CR_Client,5);

    return(true);
}

//======================================================================================================================
//
// trade, remove item from window
//
bool MessageLib::sendDeleteItemMessage(PlayerObject* playerObject,uint64 ItemId)
{
    if(!playerObject || !playerObject->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opRemoveItemMessage);
    mMessageFactory->addUint64(ItemId);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 4);

    return(true);
}

//======================================================================================================================
//
// bid auction response
//
bool MessageLib::sendBidAuctionResponseMessage(PlayerObject* playerObject, uint64 AuctionId, uint32 error)
{
    if(!playerObject || !playerObject->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opBidAuctionResponseMessage);
    mMessageFactory->addUint64(AuctionId);
    mMessageFactory->addUint32(error);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 4);

    return(true);
}

//======================================================================================================================
//
// trade, accept
//
bool MessageLib::sendAcceptTradeMessage(PlayerObject* playerObject)
{
    if(!playerObject || !playerObject->isConnected())
    {
        return(false);
    }

    //sets the accepted flag
    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opAcceptTransactionMessage);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 4);

    return(true);
}

//======================================================================================================================
//
// trade, give money
//
bool MessageLib::sendGiveMoneyMessage(PlayerObject* playerObject,uint32 Money)
{
    if(!playerObject || !playerObject->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opGiveMoneyMessage);
    mMessageFactory->addUint32(Money);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 4);

    return(true);
}

//======================================================================================================================
//
// trade, cancel, sets the accepted flag
//
bool MessageLib::sendUnacceptTradeMessage(PlayerObject* playerObject)
{
    if(!playerObject || !playerObject->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opUnacceptTransactionMessage);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 4);

    return(true);
}

//======================================================================================================================
//
// trade, verify
//
bool MessageLib::sendBeginVerificationMessage(PlayerObject* playerObject)
{
    if(!playerObject || !playerObject->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opBeginVerificationMessage);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 4);

    return(true);
}

//======================================================================================================================
//
// trade, complete
//
bool MessageLib::sendTradeCompleteMessage(PlayerObject* playerObject)
{
    if(!_checkPlayer(playerObject))    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opTradeCompleteMessage);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 4);

    return(true);
}

//======================================================================================================================
//
// trade, verify
//
bool MessageLib::sendVerifyTradeMessage(PlayerObject* playerObject)
{
    if(!_checkPlayer(playerObject))    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opVerifyTradeMessage);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 4);

    return(true);
}

//======================================================================================================================
//
// trade, abort
//
bool MessageLib::sendAbortTradeMessage(PlayerObject* playerObject)
{
    if(!_checkPlayer(playerObject))    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opAbortTradeMessage);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 4);

    return(true);
}

//======================================================================================================================
//
// trade, begin
//
bool MessageLib::sendBeginTradeMessage(PlayerObject* targetPlayer,PlayerObject* srcObject)
{
    if(!srcObject || !targetPlayer || !targetPlayer->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opBeginTradeMessage);
    mMessageFactory->addUint64(srcObject->getId());

    (targetPlayer->getClient())->SendChannelA(mMessageFactory->EndMessage(), targetPlayer->getAccountId(), CR_Client, 4);

    return(true);
}

//======================================================================================================================
//
// trade, add item
//
bool MessageLib::sendAddItemMessage(PlayerObject* targetPlayer,TangibleObject* object)
{
    if(!object || !targetPlayer || !targetPlayer->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opAddItemMessage);
    mMessageFactory->addUint64(object->getId());

    (targetPlayer->getClient())->SendChannelA(mMessageFactory->EndMessage(), targetPlayer->getAccountId(), CR_Client, 5);

    return(true);
}

//======================================================================================================================
//
// auction, item response
//
bool MessageLib::sendCreateAuctionItemResponseMessage(PlayerObject* targetPlayer,uint64 AuctionId,uint32 error)
{
    if(!_checkPlayer(targetPlayer))    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opCreateAuctionMessageResponseMessage);
    mMessageFactory->addUint64(AuctionId);
    mMessageFactory->addUint32(error);

    (targetPlayer->getClient())->SendChannelA(mMessageFactory->EndMessage(), targetPlayer->getAccountId(),  CR_Client, 6);

    return(true);
}

//======================================================================================================================
//
// updates an object parent<->child relationship
//
void ThreadSafeMessageLib::broadcastContainmentMessage(uint64 objectId,uint64 parentId,uint32 linkType,PlayerObject* player)
{
	if(!_checkPlayer(player))    {
        return;
    }

	PlayerObjectSet		listeners = *player->getRegisteredWatchers();

	//add to the active thread for processing
	active_.Send([=] {
		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opUpdateContainmentMessage);

		mMessageFactory->addUint64(objectId);
		mMessageFactory->addUint64(parentId);
		mMessageFactory->addUint32(linkType);

		_sendToInRange(mMessageFactory->EndMessage(), player, 4, listeners, true);
	}
	);

    return;
}

//======================================================================================================================
//
// updates an object parent<->child relationship
// Used when Creatures updates their cell positions.
//
void ThreadSafeMessageLib::broadcastContainmentMessage(Object* targetObject,uint64 parentId,uint32 linkType)
{
	if(!targetObject)    {
        return;
    }

	PlayerObjectSet		listeners = *targetObject->getRegisteredWatchers();

	uint32 id = targetObject->getId();

	//add to the active thread for processing
	active_.Send([=] {

		mMessageFactory->StartMessage();
		mMessageFactory->addUint32(opUpdateContainmentMessage);

		mMessageFactory->addUint64(id);
		mMessageFactory->addUint64(parentId);
		mMessageFactory->addUint32(linkType);

		_sendToInRange(mMessageFactory->EndMessage(), targetObject, 4, listeners, false);

		return;
	}
	);
}

//======================================================================================================================
//
// Tutorial: update tutorial trigger
//
bool MessageLib::sendUpdateTutorialRequest(PlayerObject* playerObject, BString request)
{
    if(!playerObject || !playerObject->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opNewbieTutorialRequest);
    mMessageFactory->addString(request);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 5);

    return(true);
}

//======================================================================================================================
//
// Open Holocron
//
bool MessageLib::sendOpenHolocron(PlayerObject* playerObject)
{
    if(!playerObject || !playerObject->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opOpenHolocronToPageMessage);
    mMessageFactory->addUint16(0);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 5);

    return(true);
}

//======================================================================================================================
//
// enable hud element
//
bool MessageLib::sendEnableHudElement(PlayerObject* playerObject, BString hudElement)
{
    if(!playerObject || !playerObject->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(0xCA375124);
    mMessageFactory->addString(hudElement);
    mMessageFactory->addUint8(1);
    mMessageFactory->addUint32(0);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 5);

    return(true);
}

//======================================================================================================================
//
// disable hud element
//
bool MessageLib::sendDisableHudElement(PlayerObject* playerObject, BString hudElement)
{
    if(!playerObject || !playerObject->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(0xCA375124);
    mMessageFactory->addString(hudElement);
    mMessageFactory->addUint8(0);
    mMessageFactory->addUint32(0);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 5);

    return(true);
}

//======================================================================================================================
//
// Logout
//
bool MessageLib::sendLogout(PlayerObject* playerObject)
{
    if(!playerObject || !playerObject->isConnected())
    {
        return(false);
    }

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opLogoutMessage);

    (playerObject->getClient())->SendChannelA(mMessageFactory->EndMessage(), playerObject->getAccountId(), CR_Client, 5);

    return(true);
}
