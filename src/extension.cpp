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
 */

#include "extension.h"

#include <algorithm>
#include <cstring>
#include <zmq.h>

#define ENSURE_ZMQ_SUCCESS(rc) if (rc == -1) return pCtx->ThrowNativeError("%s", zmq_strerror(zmq_errno()))
#if 0
#endif

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

SMZMQ g_SMZMQ;      /**< Global singleton for extension's main interface */

SocketDispatch g_SocketDispatch;
MessageDispatch g_MessageDispatch;

HandleType_t g_SocketHandle;
HandleType_t g_MessageHandle;

SMEXT_LINK(&g_SMZMQ);

// SocketDispatch
void SocketDispatch::OnHandleDestroy(HandleType_t type, void *object) {
    zmq_close(object);
}

// MessageDispatch
void MessageDispatch::OnHandleDestroy(HandleType_t type, void *object) {
    zmq_msg_close(static_cast<zmq_msg_t*>(object));
    delete static_cast<zmq_msg_t*>(object);
}

// Poller
Poller::Poller(void* zmq_ctx) {
    controller = zmq_socket(zmq_ctx, ZMQ_REP);
}

Poller::~Poller() {
    zmq_close(controller);
}

void Poller::RunThread(IThreadHandle *pHandle) {
    int rc = zmq_bind(controller, "inproc://smzmq-thread-controller");
    if (rc == -1) {
        smutils->LogError(myself, "%s", zmq_strerror(zmq_errno()));
        return;
    }
    
    zmq_pollitem_t items[] = {
        {socket, 0, ZMQ_POLLIN, 0},
        {controller, 0, ZMQ_POLLIN, 0}
    };
    
    cell_t params[2];
    
    rc = zmq_poll(items, 2, timeout);
    if (rc == -1) {
        smutils->LogError(myself, "%s", zmq_strerror(zmq_errno()));
    }
    
    if (rc == -1 || items[1].revents & ZMQ_POLLIN) {
        params[0] = params[1] = 0;
    } else if (items[0].revents & ZMQ_POLLIN) {
        zmq_msg_t* message;
        zmq_msg_init(message);
        zmq_recv(socket, message, 0);
        params[0] = 1;
        params[1] = handlesys->CreateHandle(g_MessageHandle, static_cast<void*>(message), context->GetIdentity(), myself->GetIdentity(), NULL);
    }
    
    context->Execute2(callback, params, 2, 0);
}

void Poller::OnTerminate(IThreadHandle* pHandle, bool cancel) {
    
}

// SMZMQ
bool SMZMQ::SDK_OnLoad(char *error, size_t err_max, bool late) {
    zmq_ctx = zmq_init(1);
    
    controller = zmq_socket(zmq_ctx, ZMQ_REQ);
    if (!controller) {
        snprintf(error, err_max, "%s", zmq_strerror(zmq_errno()));
        return false;
    }
    
    g_SocketHandle = handlesys->CreateType("ZMQSocket", &g_SocketDispatch, NULL, NULL, NULL, myself->GetIdentity(), NULL);
    g_MessageHandle = handlesys->CreateType("ZMQMessage", &g_MessageDispatch, NULL, NULL, NULL, myself->GetIdentity(), NULL);
    
    sharesys->AddNatives(myself, natives);
    
    return true;
}

void SMZMQ::SDK_OnUnload() {
    int rc = zmq_connect(controller, "inproc://smzmq-thread-controller");
    if (rc == -1) {
        smutils->LogError(myself, "%s", zmq_strerror(zmq_errno()));
    } else {
        zmq_msg_t die;
        zmq_msg_init_size(&die, 0);
        zmq_send(controller, &die, 0);
        
        std::vector<IThreadHandle*>::iterator it;
        for (it = pollers.begin(); it != pollers.end(); ++it) {
            (*it)->WaitForThread();
            (*it)->DestroyThis();
        }
    }
    
    rc = zmq_close(controller);
    if (rc == -1) {
        smutils->LogError(myself, "%s", zmq_strerror(zmq_errno()));
    }
    
    handlesys->RemoveType(g_SocketHandle, myself->GetIdentity());
    handlesys->RemoveType(g_MessageHandle, myself->GetIdentity());
    
    rc = zmq_term(zmq_ctx);
    if (rc == -1) {
        smutils->LogError(myself, "%s", zmq_strerror(zmq_errno()));
    }
}

void* SMZMQ::GetContext() {
    return zmq_ctx;
}

bool SMZMQ::AddPoller(IThreadHandle* thread) {
    if (std::find(pollers.begin(), pollers.end(), thread) == pollers.end()) {
        pollers.push_back(thread);
        return true;
    }
    
    return false;
}

bool SMZMQ::RemovePoller(IThreadHandle* thread) {
    std::vector<IThreadHandle*>::iterator it;
    it = std::find(pollers.begin(), pollers.end(), thread);
    if (it == pollers.end())
        return false;
    pollers.erase(it);
    return false;
}

int ReadHandle(IPluginContext* pCtx, cell_t raw, HandleType_t type, const char* name, void* result) {
    Handle_t hndl = static_cast<Handle_t>(raw);
    HandleError err;
    HandleSecurity sec;
    sec.pOwner = NULL;
    sec.pIdentity = myself->GetIdentity();
    
    if ((err=g_pHandleSys->ReadHandle(hndl, type, &sec, &result)) != HandleError_None)
        return pCtx->ThrowNativeError("Invalid %s handle %x (error %d)", name, hndl, err);
    
    return 1;
}

static cell_t CreateSocket(IPluginContext* pCtx, const cell_t *params) {
    void* socket = zmq_socket(g_SMZMQ.GetContext(), params[1]);
    if (!socket) {
        return pCtx->ThrowNativeError("%s", zmq_strerror(zmq_errno()));
    }
    
    smutils->LogMessage(myself, "Created socket %x", socket);
    
    int linger = 0;
    int rc = zmq_setsockopt(socket, ZMQ_LINGER, &linger, sizeof(linger));
    ENSURE_ZMQ_SUCCESS(rc);
    
    return handlesys->CreateHandle(g_SocketHandle, socket, pCtx->GetIdentity(), myself->GetIdentity(), NULL);
}

static cell_t SocketConnect(IPluginContext* pCtx, const cell_t *params) {
    void* socket;
    int rc = ::ReadHandle(pCtx, params[1], g_SocketHandle, "socket", socket);
    if (rc)
        return rc;
    
    smutils->LogMessage(myself, "%s: socket = %x", __FUNCTION__, socket);
    
    char *endpoint;
    pCtx->LocalToString(params[2], &endpoint);
    
    rc = zmq_connect(socket, endpoint);
    ENSURE_ZMQ_SUCCESS(rc);
    
    return 1;
}

static cell_t SocketBind(IPluginContext* pCtx, const cell_t *params) {
    void* socket;
    int rc = ::ReadHandle(pCtx, params[1], g_SocketHandle, "socket", socket);
    if (rc)
        return rc;
    
    smutils->LogMessage(myself, "%s: socket = %x", __FUNCTION__, socket);
    
    char *endpoint;
    pCtx->LocalToString(params[2], &endpoint);
    
    rc = zmq_bind(socket, endpoint);
    ENSURE_ZMQ_SUCCESS(rc);
    
    return 1;
}

static cell_t SocketSubscribe(IPluginContext* pCtx, const cell_t *params) {
    void* socket;
    int rc = ::ReadHandle(pCtx, params[1], g_SocketHandle, "socket", socket);
    if (rc)
        return rc;
    
    char* filter;
    pCtx->LocalToString(params[2], &filter);
    
    smutils->LogMessage(myself, "%s: socket = %x | filter = %s", __FUNCTION__, socket, filter);
    
    rc = zmq_setsockopt(socket, ZMQ_SUBSCRIBE, filter, params[3]);
    ENSURE_ZMQ_SUCCESS(rc);
    
    return 1;
}

static cell_t SocketSetOpt(IPluginContext* pCtx, const cell_t *params) {
    void* socket;
    int rc = ::ReadHandle(pCtx, params[1], g_SocketHandle, "socket", socket);
    if (rc)
        return rc;
    
    smutils->LogMessage(myself, "%s: socket = %x", __FUNCTION__, socket);
    
    int opt = params[2];
    
    cell_t *res;
    pCtx->LocalToPhysAddr(params[3], &res);
    size_t len = 0;
    
    if (opt == ZMQ_IDENTITY || opt == ZMQ_SUBSCRIBE || opt == ZMQ_UNSUBSCRIBE)
        len = params[4];
    else
        len = sizeof(cell_t);
    
    rc = zmq_setsockopt(socket, opt, static_cast<void*>(res), len);
    ENSURE_ZMQ_SUCCESS(rc);
    
    return 1;
}

static cell_t SocketGetOpt(IPluginContext* pCtx, const cell_t *params) {
    void* socket;
    int rc = ::ReadHandle(pCtx, params[1], g_SocketHandle, "socket", socket);
    if (rc)
        return rc;
    
    smutils->LogMessage(myself, "%s: socket = %x", __FUNCTION__, socket);
    
    int opt = params[2];
    
    cell_t *res;
    pCtx->LocalToPhysAddr(params[3], &res);
    size_t len = 0;
    
    if (opt == ZMQ_IDENTITY || opt == ZMQ_SUBSCRIBE || opt == ZMQ_UNSUBSCRIBE)
        len = params[4];
    else
        len = sizeof(cell_t);
    
    rc = zmq_getsockopt(socket, opt, static_cast<void*>(res), &len);
    ENSURE_ZMQ_SUCCESS(rc);
    
    return 1;
}

static cell_t SocketSend(IPluginContext* pCtx, const cell_t *params) {
    void* socket;
    int rc = ::ReadHandle(pCtx, params[1], g_SocketHandle, "socket", socket);
    if (rc)
        return rc;
    
    smutils->LogMessage(myself, "%s: socket = %x", __FUNCTION__, socket);
    
    zmq_msg_t* message;
    rc = ::ReadHandle(pCtx, params[2], g_MessageHandle, "message", message);
    if (rc)
        return rc;
    
    smutils->LogMessage(myself, "%s: message = %x", __FUNCTION__, message);
    
    rc = zmq_send(socket, message, params[3]);
    ENSURE_ZMQ_SUCCESS(rc);
    
    return 1;
}

static cell_t SocketRecv(IPluginContext* pCtx, const cell_t *params) {
    void* socket;
    int rc = ::ReadHandle(pCtx, params[1], g_SocketHandle, "socket", socket);
    if (rc)
        return rc;
    
    smutils->LogMessage(myself, "%s: socket = %x", __FUNCTION__, socket);
    
    zmq_msg_t* message = new zmq_msg_t;
    
    rc = zmq_msg_init(message);
    ENSURE_ZMQ_SUCCESS(rc);
    
    rc = zmq_recv(socket, message, params[2]);
    ENSURE_ZMQ_SUCCESS(rc);
    
    return handlesys->CreateHandle(g_MessageHandle, static_cast<void*>(message), pCtx->GetIdentity(), myself->GetIdentity(), NULL);
}

static cell_t CreateMessage(IPluginContext* pCtx, const cell_t *params) {
    cell_t *data;
    pCtx->LocalToPhysAddr(params[1], &data);
    
    zmq_msg_t* message = new zmq_msg_t;
    int rc = zmq_msg_init_size(message, params[2]);
    ENSURE_ZMQ_SUCCESS(rc);
    
    smutils->LogMessage(myself, "Created message %x", message);
    
    memcpy(zmq_msg_data(message), data, params[2]);
    
    return handlesys->CreateHandle(g_MessageHandle, message, pCtx->GetIdentity(), myself->GetIdentity(), NULL);
}

static cell_t GetMessageSize(IPluginContext* pCtx, const cell_t *params) {
    zmq_msg_t *message;
    int rc = ::ReadHandle(pCtx, params[2], g_MessageHandle, "message", static_cast<void*>(message));
    if (rc)
        return rc;
    
    smutils->LogMessage(myself, "%s: message = %x", __FUNCTION__, message);
    
    return zmq_msg_size(message);
}

static cell_t GetMessageData(IPluginContext* pCtx, const cell_t *params) {
    zmq_msg_t *message;
    int rc = ::ReadHandle(pCtx, params[1], g_MessageHandle, "message", static_cast<void*>(message));
    if (rc)
        return rc;
    
    smutils->LogMessage(myself, "%s: message = %x", __FUNCTION__, message);
    
    cell_t *data;
    pCtx->LocalToPhysAddr(params[2], &data);
    
    memcpy(static_cast<void*>(data), zmq_msg_data(message), params[3]);
    
    return 1;
}

static cell_t SocketPoll(IPluginContext* pCtx, const cell_t *params) {
    void* socket;
    int rc = ::ReadHandle(pCtx, params[1], g_SocketHandle, "socket", socket);
    if (rc)
        return rc;
    
    smutils->LogMessage(myself, "%s: socket = %x", __FUNCTION__, socket);
    
    IPluginFunction* callback = pCtx->GetFunctionById(static_cast<funcid_t>(params[2]));
    if (!callback)
        return pCtx->ThrowNativeError("Invalid function id (%X)", params[2]);
    
    Poller* poller = new Poller(g_SMZMQ.GetContext());
    poller->socket = socket;
    poller->callback = callback;
    poller->context = pCtx;
    poller->timeout = params[3];
    
    ThreadParams threadParams;
    threadParams.flags = Thread_Default;
    threadParams.prio = ThreadPrio_Normal;
    IThreadHandle* thread = threader->MakeThread(poller, &threadParams);
    
    if (thread == NULL) {
        return pCtx->ThrowNativeError("Failed to create poller thread.");
    }
    
    g_SMZMQ.AddPoller(thread);
    
    return 1;
}

const sp_nativeinfo_t SMZMQ::natives[] = {
    {"CreateSocket",        ::CreateSocket},
    {"SocketConnect",       ::SocketConnect},
    {"SocketBind",          ::SocketBind},
    {"SocketSubscribe",     ::SocketSubscribe},
    {"SocketSetOpt",        ::SocketSetOpt},
    {"SocketGetOpt",        ::SocketGetOpt},
    {"SocketSend",          ::SocketSend},
    {"SocketRecv",          ::SocketRecv},
    
    {"CreateMessage",       ::CreateMessage},
    {"GetMessageSize",      ::GetMessageSize},
    {"GetMessageData",      ::GetMessageData},
    
    {"SocketPoll",          ::SocketPoll},
    
    {NULL,                  NULL}
};
