/**
 * vim: set ts=4 :
 * =============================================================================
 * SMZMQ Extension
 * Copyright (C) 2011 Reuben 'Seta00' Morais.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#ifndef _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_

#include "smsdk_ext.h"

#include <vector>

/**
 * @brief Sample implementation of the SDK Extension.
 * Note: Uncomment one of the pre-defined virtual functions in order to use it.
 */
class SMZMQ : public SDKExtension {
public:
    virtual bool SDK_OnLoad(char* error, size_t maxlength, bool late);
    virtual void SDK_OnUnload();
    
    void* GetContext();
    
    bool AddPoller(IThreadHandle* thread);
    bool RemovePoller(IThreadHandle* thread);
    
private:
    void* zmq_ctx;
    void* controller;
    static const sp_nativeinfo_t natives[];
    std::vector<IThreadHandle*> pollers;
};

struct Poller : public IThread {
    Poller(void* zmq_ctx);
    virtual ~Poller();
    void RunThread(IThreadHandle* pHandle);
    void OnTerminate(IThreadHandle* pHandle, bool cancel);
    
    IPluginFunction* callback;
    IPluginContext* context;
    void* socket;
    int timeout;
    void *controller;
};

struct SocketDispatch: public IHandleTypeDispatch {
    void OnHandleDestroy(HandleType_t type, void* object);
};

struct MessageDispatch: public IHandleTypeDispatch {
    void OnHandleDestroy(HandleType_t type, void* object);
};

extern SocketDispatch g_SocketDispatch;
extern MessageDispatch g_MessageDispatch;

extern HandleType_t g_SocketHandle;
extern HandleType_t g_MessageHandle;

#endif // _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
