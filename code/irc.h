#pragma once

#include "network_tcp.h"

enum irc_message_type
{
	IRCMessageType_Nick,
	IRCMessageType_User,
	IRCMessageType_Join,
	IRCMessageType_Pong,
	IRCMessageType_Ping,

	IRCMessageType_Count,
};

struct irc_message
{
	irc_message_type Type;
	char* ServerFrom;
	u32 ResponseID;
	char* Content[512];
}

void IRCSendMessage(irc_message Message, TCPSocket ServerSocket)
{
	SDLNet_TCP_Send(ServerSocket, 
			Message.Content, 
			ArrayCount(Message.Content));
}
