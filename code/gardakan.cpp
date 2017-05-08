#define SDL_MAIN_HANDLED

#ifdef _WIN32
#include <SDL.h>
#include <SDL_net.h>
#else
#include <SDL2/SDL.h>
#include <SDL/SDL_net.h>
#endif

#include <stdio.h>

#include "rivten.h"

#include "multithreading.h"

void Server(void)
{
	IPaddress Address;
	s32 ResolveHostResult = SDLNet_ResolveHost(&Address, 0, 4000);
	if(ResolveHostResult == 0)
	{
		TCPsocket ServerSocket = SDLNet_TCP_Open(&Address);
		if(ServerSocket)
		{
			while(true)
			{
				TCPsocket ClientSocket = SDLNet_TCP_Accept(ServerSocket);
				if(ClientSocket)
				{
#if 1
					IPaddress* ClientIP = SDLNet_TCP_GetPeerAddress(ClientSocket);
					Assert(ClientIP);
					printf("A connection was established from client IP %d on port %d.\n", 
							ClientIP->host, ClientIP->port);
#endif

					char* Text = "Hello, sailor!";
					u32 Length = (u32)(strlen(Text) + 1);
					SDLNet_TCP_Send(ClientSocket, Text, Length);
					SDLNet_TCP_Close(ClientSocket);
				}
			}
			SDLNet_TCP_Close(ServerSocket);
		}
		else
		{
			printf("TCP could not open: %s", SDLNet_GetError());
		}
	}
	else
	{
		printf("Host was not resolved: %s", SDLNet_GetError());
	}
}

struct irc_message
{
	rvtn_string Prefix;
	rvtn_string Command;
	rvtn_string Params;
};

struct irc_state
{
	rvtn_string PartialLastMessage;
	bool ReceivedUntreatedPartialMessage;
};

char* ClearIRCMessageString(char* Text)
{
	return(Text);
}

#if 0
irc_message IRCParseMessage(char* MessageText)
{
	irc_message Result = {};
	char* ClearedMessageText = ClearIRCMessageString(MessageText);
	Assert(StringLength(ClearedMessageText) != 0);
	bool HasPrefix = (ClearedMessageText[0] == ':');

	//char* Token = 0;
	char Token[512];
	ConsumeToken(Token, &ClearedMessageText, " :");
	Assert(Token);
	if(HasPrefix)
	{
		// NOTE(hugo) : Parse prefix
		sprintf(Result.Prefix, "%s", Token);

		// NOTE(hugo) : Parse command
		ConsumeToken(Token, &ClearedMessageText, " ");
		Assert(Token);
		sprintf(Result.Command, "%s", Token);

		// NOTE(hugo) : Parse params
		ConsumeToken(Token, &ClearedMessageText, " ");
		if(Token)
		{
			sprintf(Result.Params, "%s", Token);
		}
	}
	else
	{
		// NOTE(hugo) : Parse command
		sprintf(Result.Command, "%s", Token);

		// NOTE(hugo) : Parse params
		ConsumeToken(Token, &ClearedMessageText, " ");
		if(Token)
		{
			sprintf(Result.Params, "%s", Token);
		}
	}

	printf("%s -- %s\n", Result.Command, Result.Params);

	return(Result);
}

void IRCParsePacket(irc_state* IRCState, char* Text)
{
	u32 TextLength = StringLength(Text);
	Assert(TextLength > 2);
	bool FullMessage = ((Text[TextLength - 1] == '\n') && 
			(Text[TextLength - 2] == '\r'));

	char Token[512];
	ConsumeToken(Token, &Text, "\r\n");
	Assert(Token);

	char NextToken[512];
	ConsumeToken(NextToken, &Text, "\r\n");

	while(StringLength(Token) > 0)
	{
		char MessageText[512];
		if(IRCState->ReceivedUntreatedPartialMessage)
		{
			Assert(StringLength(IRCState->PartialLastMessage) + 
					StringLength(Token) <= ArrayCount(MessageText));
			sprintf(MessageText, "%s%s", IRCState->PartialLastMessage,
					Token);
			IRCState->ReceivedUntreatedPartialMessage = false;
		}
		else
		{
			Assert(StringLength(Token) <= ArrayCount(MessageText));
			sprintf(MessageText, "%s", Token);
		}

		if(!NextToken && !FullMessage)
		{
			// NOTE(hugo) : The MessageText is the last one, and it is not a full message
			Assert(StringLength(MessageText) <= ArrayCount(IRCState->PartialLastMessage));
			sprintf(IRCState->PartialLastMessage, "%s", MessageText);
			IRCState->ReceivedUntreatedPartialMessage = true;

			//CopyArray(Token, NextToken, u8, ArrayCount(Token));
		}
		else
		{
			irc_message Message = IRCParseMessage(MessageText);

			if(StringLength(NextToken) > 0)
			{
				Assert(ArrayCount(NextToken) <= ArrayCount(Token));
				CopyArray(Token, NextToken, char, ArrayCount(Token));
				ConsumeToken(NextToken, &Text, "\r\n");
			}
			else
			{
				// TODO(hugo) : This is a ugly hack
				Token[0] = '\0';
			}
		}
	}
}
#endif

irc_message IRCParseMessage(rvtn_string Message)
{
	irc_message Result = {};
	Print(Message);
	return(Result);
}

void IRCParsePacket(irc_state* IRCState, rvtn_string Packet)
{
	Assert(Packet.Size > 2);

	rvtn_string IRCMessageDelimiter = CreateString("\r\n");
	rvtn_string Message = {};

	bool FullPacket = StringEndsWith(Packet, IRCMessageDelimiter);
	bool IsLastMessage = false;
	do
	{
		consume_token_result TokenConsumed = ConsumeToken(Packet, IRCMessageDelimiter);
		FreeString(&Message);
		Message = TokenConsumed.Token;
		FreeString(&Packet);
		Packet = TokenConsumed.Remain;

		if(IRCState->PartialLastMessage.Size > 0)
		{
			rvtn_string TempMessage = ConcatString(IRCState->PartialLastMessage, Message);
			FreeString(&IRCState->PartialLastMessage);
			FreeString(&Message);
			Message = TempMessage;
		}
		IsLastMessage = !IsSubstring(IRCMessageDelimiter, Packet);
		if((!IsLastMessage) || FullPacket)
		{
			IRCParseMessage(Message);
		}
		else // NOTE(hugo) : IsLastMessage && (!FullPacket)
		{
			IRCState->PartialLastMessage = CreateString(Packet);
			FreeString(&Packet);
		}
		FreeString(&Message);
	} while(Packet.Size > 0);

}

void Client(char* Host, u32 Port)
{
	IPaddress HostAddress;
	s32 ResolveHostResult = SDLNet_ResolveHost(&HostAddress, Host, Port);
	if(ResolveHostResult == 0)
	{
		TCPsocket ServerSocket = SDLNet_TCP_Open(&HostAddress);
		if(ServerSocket)
		{
			printf("Connection to server was successful!\n");
			SDLNet_SocketSet Sockets = SDLNet_AllocSocketSet(16);
			if(Sockets)
			{
				SDLNet_TCP_AddSocket(Sockets, ServerSocket);
				bool AuthentificationDone = false;
				bool ChannelJoined = false;
				u32 AuthTick = 0;
				irc_state IRCState = {};
				while(true)
				{
					// TODO(hugo) : Fix infinite loop ?
					u32 SocketReadyCount = SDLNet_CheckSockets(Sockets, 1000); // NOTE(hugo) : We wait 1s max
					Assert(SocketReadyCount != -1);

					// NOTE(hugo) : Receive messages
					if(SocketReadyCount > 0)
					{
						if(SDLNet_SocketReady(ServerSocket))
						{
							// NOTE(hugo): 512 was dictated by the maximum size of a IRC message
							// as specified in RFC 2812
							char Text[512];

							SDLNet_TCP_Recv(ServerSocket, Text, ArrayCount(Text));
							rvtn_string Packet = CreateString(Text);
							IRCParsePacket(&IRCState, Packet);
						}
					}

					// NOTE(hugo) : Send messages
					if(!AuthentificationDone)
					{
						printf("Authentification...\r\n");
						char* Message = "NICK Goo\r\nUSER Goo localhost 0 :Goo\r\n";
						u32 Length = (u32)(StringLength(Message) + 1);
						SDLNet_TCP_Send(ServerSocket, Message, Length);

						AuthentificationDone = true;
						AuthTick = SDL_GetTicks();
					}
					else
					{
						u32 TicksSinceAuth = SDL_GetTicks() - AuthTick;
						if((TicksSinceAuth > 1000) && (!ChannelJoined))
						{
							printf("--------Joining...\r\n");
							printf("--------Joining...\r\n");
							printf("--------Joining...\r\n");
							printf("--------Joining...\r\n");
							//char* Message = "JOIN #random\r\n";
							char* Message = "LIST";
							u32 Length = StringLength(Message) + 1;
							SDLNet_TCP_Send(ServerSocket, Message, Length);

							ChannelJoined = true;
						}
					}
				}
			}
			else
			{
				printf("Connection to server was unsuccessful...\n");
			}
			SDLNet_FreeSocketSet(Sockets);
		}
		else
		{
			printf("Socket Sets could not be created.\n");
		}
		SDLNet_TCP_Close(ServerSocket);
	}
	else
	{
		printf("Host was not resolved: %s", SDLNet_GetError());
	}
}

int main(int ArgumentCount, char** Arguments)
{
	SDL_version CompileVersion;
	const SDL_version* LinkVersion = SDLNet_Linked_Version();
	SDL_NET_VERSION(&CompileVersion);
	printf("Gardakan is compiled with SDL_Net version : %d.%d.%d\n",
			CompileVersion.major,
			CompileVersion.minor,
			CompileVersion.patch);

	printf("Gardakan is runnning witch SDL_Net version : %d.%d.%d\n", 
			LinkVersion->major,
			LinkVersion->minor,
			LinkVersion->patch);

	u32 InitResult = SDLNet_Init();
	if(InitResult == 0)
	{
		printf("SDL_Net is properly initialized.\n");

		if((ArgumentCount >= 2) && (strcmp(Arguments[1], "-s") == 0))
		{
			Server();
		}
		else
		{
			if(ArgumentCount >= 3)
			{
				char* Host = Arguments[1];
				u32 Port = atoi(Arguments[2]);
				Client(Host, Port);
			}
			else
			{
				printf("Gardakan requires more parameters.\nEither '-s' for becoming a server\nor the server IP for connecting to it.\n");
			}
		}

		SDLNet_Quit();
	}
	else
	{
		printf("SDL_Net encountered an error on init. Aborting Gardakan.\nReason: %s", SDLNet_GetError());
	}

	return(0);
}
