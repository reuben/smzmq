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

#if defined _zmq_included
 #endinput
#endif
#define _zmq_included

enum SocketType {
    ZMQ_PAIR,
    ZMQ_PUB,
    ZMQ_SUB,
    ZMQ_REQ,
    ZMQ_REP,
    ZMQ_DEALER,
    ZMQ_ROUTER,
    ZMQ_PULL,
    ZMQ_PUSH,
    ZMQ_XPUB,
    ZMQ_XSUB
};

enum SocketOption {
    ZMQ_HWM = 1,
    ZMQ_SWAP = 3,
    ZMQ_AFFINITY = 4,
    ZMQ_IDENTITY = 5,
    ZMQ_SUBSCRIBE = 6,
    ZMQ_UNSUBSCRIBE = 7,
    ZMQ_RATE = 8,
    ZMQ_RECOVERY_IVL = 9,
    ZMQ_MCAST_LOOP = 10,
    ZMQ_SNDBUF = 11,
    ZMQ_RCVBUF = 12,
    ZMQ_RCVMORE = 13,
    ZMQ_FD = 14,
    ZMQ_EVENTS = 15,
    ZMQ_TYPE = 16,
    ZMQ_LINGER = 17,
    ZMQ_RECONNECT_IVL = 18,
    ZMQ_BACKLOG = 19,
    ZMQ_RECOVERY_IVL_MSEC = 20, /*  opt. recovery time, reconcile in 3.x   */
    ZMQ_RECONNECT_IVL_MAX = 21
};

/*  Send/recv options. */
#define ZMQ_NOBLOCK 1
#define ZMQ_SNDMORE 2

/**
 * The CreateSocket() native shall create a ØMQ socket within the specified context and return a Handle to the newly created socket.
 * The type argument specifies the socket type, which determines the semantics of communication over the socket.
 *
 * The newly created socket is initially unbound, and not associated with any endpoints.
 * In order to establish a message flow a socket must first be connected to at least one endpoint with SocketConnect(),
 * or at least one endpoint must be created for accepting incoming connections with SocketBind().
 */
native Handle:CreateSocket(SocketType:type);

native SocketConnect(Handle:socket, String:endpoint[]);
native SocketBind(Handle:socket, String:endpoint[]);
native SocketSubscribe(Handle:socket, filter[], len);
native SocketSetOpt(Handle:socket, SocketOption:opt, ...);
native SocketGetOpt(Handle:socket, SocketOption:opt, ...);
native SocketSend(Handle:socket, Handle:message, flags=0);
native Handle:SocketRecv(Handle:socket, flags=0);

native Handle:CreateMessage(data[], len);
native GetMessageSize(Handle:message);
native GetMessageData(Handle:message, data[], maxlen);

functag public PollCallback(bool:success, Handle:message);
native SocketPoll(Handle:socket, PollCallback:callback, timeout=-1);

stock Handle:CreateMessageStr(String:data[], len) {
    new array[len];
    for (new i = 0; i < len; ++i)
        array[i] = data[i];
    return CreateMessage(array, len);
}

stock GetMessageDataStr(Handle:message, String:str[], maxlen) {
    new len = GetMessageSize(message);
    new result[len];
    GetMessageData(message, result, len);
    for (new i = 0; i < maxlen && i < len; ++i)
        str[i] = result[i];
}

stock SocketSubscribeStr(Handle:socket, String:filter[], len) {
    new array[len];
    for (new i = 0; i < len; ++i)
        array[i] = filter[i];
    return SocketSubscribe(socket, array, len);
}

/**
 * Do not edit below this line!
 */
public Extension:__ext_zmq = {
    name = "smzmq",
    file = "smzmq.ext",
    autoload = 0,
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};

#if !defined REQUIRE_EXTENSIONS
public __ext_cstrike_SetNTVOptional() {
    MarkNativeAsOptional("CreateSocket");
    MarkNativeAsOptional("SocketConnect");
    MarkNativeAsOptional("SocketBind");
    MarkNativeAsOptional("SocketSubscribe");
    MarkNativeAsOptional("SocketSetOpt");
    MarkNativeAsOptional("SocketGetOpt");
    MarkNativeAsOptional("SocketSend");
    MarkNativeAsOptional("SocketRecv");
    MarkNativeAsOptional("CreateMessage");
    MarkNativeAsOptional("GetMessageSize");
    MarkNativeAsOptional("GetMessageData");
    MarkNativeAsOptional("SocketPoll");
}
#endif
