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

#ifndef ANH_ZONESERVER_RESOURCECONTAINER_OBJECT_FACTORY_H
#define ANH_ZONESERVER_RESOURCECONTAINER_OBJECT_FACTORY_H

#include "FactoryBase.h"

#define 	gResourceContainerFactory	ResourceContainerFactory::getSingletonPtr()

//=============================================================================

class Database;
class DataBinding;
class DispatchClient;
class ObjectFactoryCallback;
class ResourceContainer;

//=============================================================================

enum RCFQuery
{
    RCFQuery_MainData	= 1,
    RCFQuery_Attributes	= 2
};

//=============================================================================

class ResourceContainerFactory : public FactoryBase
{
public:

    static ResourceContainerFactory*	getSingletonPtr() {
        return mSingleton;
    }
    static ResourceContainerFactory*	Init(Database* database);

    ~ResourceContainerFactory();

    void			handleDatabaseJobComplete(void* ref,DatabaseResult* result);
    void			requestObject(ObjectFactoryCallback* ofCallback,uint64 id,uint16 subGroup,uint16 subType,DispatchClient* client);

private:

    ResourceContainerFactory(Database* database);

    void				_setupDatabindings();
    void				_destroyDatabindings();

    ResourceContainer*	_createResourceContainer(DatabaseResult* result);

    static ResourceContainerFactory*	mSingleton;
    static bool						mInsFlag;

    DataBinding*						mResourceContainerBinding;
};

//=============================================================================


#endif

