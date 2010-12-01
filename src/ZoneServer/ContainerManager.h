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

#ifndef ANH_ZONESERVER_CONTAINERMANAGER_H
#define ANH_ZONESERVER_cONTAINERMANAGER_H

#include "SpatialIndexManager.h"
#include "WorldManagerEnums.h"

#include "Object.h"

#include "DatabaseManager/DatabaseCallback.h"

#include "MathLib/Rectangle.h"
#include "MessageLib/MessageLib.h"

#include "Utils/TimerCallback.h"
#include "Utils/typedefs.h"

#include <boost/ptr_container/ptr_unordered_map.hpp>

#include <list>
#include <map>
#include <vector>

//======================================================================================================================

#define	 gContainerManager	ContainerManager::getSingletonPtr()

//======================================================================================================================

enum TangibleType;


class TangibleObject;
class FactoryCrate;
class CellObject;

namespace Anh_Utils
{
   // class Clock;
    class Scheduler;
    //class VariableTimeScheduler;
}

//======================================================================================================================


//======================================================================================================================
//
// ContainerManager
//
class ContainerManager :  public TimerCallback
{
	public:

		static ContainerManager*	getSingletonPtr() { return mSingleton; }
		static ContainerManager*	Init(Database* database);
		
		void					Shutdown();
		

		//sends the destroys for an object equipped by a creature / player that gets unequipped
		void					SendDestroyEquippedObject(Object *removeObject);	

		//deletes an object out of a container
		void					deleteObject(Object* data, TangibleObject* parent);
		void					removeObject(Object* data, TangibleObject* parent);

		void					removeStructureItemsForPlayer(PlayerObject* player, BuildingObject* building);
		
		//to other players only - use for movement
		void					sendToRegisteredPlayers(Object* container, std::function<void (PlayerObject* const player)> callback);

		//to all registered watchers including ourselves
		void					sendToRegisteredWatchers(Object* container, std::function<void (PlayerObject* const player)> callback);

		void					sendToGroupedRegisteredPlayers(PlayerObject* const player, std::function<void (PlayerObject*  const player)> callback, bool self);
		void					GetGroupedRegisteredPlayers(PlayerObject* const container, ObjectListType list, bool self);

		//registers player as watcher to a container based on si
		void					registerPlayerToContainer(Object* container, PlayerObject* const player) const;
		//static containers are not affected by si updates
		void					registerPlayerToStaticContainer(Object* container, PlayerObject* const player, bool playerCreate = false) const;

		void					unRegisterPlayerFromContainer(Object* container, PlayerObject* const player) const;
		void					createObjectToRegisteredPlayers(Object* container,Object* object);
	
		void					destroyObjectToRegisteredPlayers(Object* container,uint64 object, bool destroyForSelf = false);
		void					updateObjectPlayerRegistrations(Object* newContainer, Object* oldContainer, Object* object, uint32 containment);
		void					updateEquipListToRegisteredPlayers(PlayerObject* player);

		// removes an item from a structure
		void					removeObjectFromBuilding(Object* object, BuildingObject* building);

		//buildings are special containers as they always have their cells loaded even if otherwise unloaded
		void					registerPlayerToBuilding(BuildingObject* building,PlayerObject* player);
		void					unRegisterPlayerFromBuilding(BuildingObject* building,PlayerObject* player);

		//======================================================================================================================
		// when creating a player and the player is in a cell we need to create all the cells contents for the player
		// cellcontent is *NOT* in the grid
		void					initObjectsInRange(PlayerObject* playerObject);

		
	private:

		ContainerManager();

		


		static ContainerManager*		mSingleton;
		static bool						mInsFlag;

		
		
};



//======================================================================================================================

#endif // ANH_ZONESERVER_CONTAINERMANAGER_H




