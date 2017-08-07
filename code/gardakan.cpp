#define SDL_MAIN_HANDLED

#ifdef _WIN32
#include <SDL.h>
#include <SDL_net.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#endif

#include <stdio.h>

#include "rivten.h"

#include "multithreading.h"

/*
  TODO(hugo) : 
	* better architecture for the main loop of the client program / refacto
	* fix registration on some servers
	* properly do the parsing of commands
	* types/enum for command
	* ability to send command to the client (and he should respond to these commands, such as !time)
	* UI (imgui ?)
 */

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

	memory_arena Arena;
};

irc_message IRCParseMessage(rvtn_string Message, memory_arena* Arena = 0)
{
	irc_message Result = {};

	rvtn_string Delimiter = CreateString(" ", Arena);
	consume_token_result Consumed = ConsumeToken(Message, Delimiter, Arena);

	Result.Prefix = Consumed.Token;
	Message = Consumed.Remain;

	Consumed = ConsumeToken(Message, Delimiter, Arena);
	Result.Command = Consumed.Token;
	Result.Params = Consumed.Remain;

	Print(Result.Params);

	return(Result);
}

void IRCParsePacket(irc_state* IRC, rvtn_string Packet)
{
	Assert(Packet.Size > 2);

	rvtn_string IRCMessageDelimiter = CreateString("\r\n", &IRC->Arena);
	rvtn_string Message = {};

	bool FullPacket = StringEndsWith(Packet, IRCMessageDelimiter);
	bool IsLastMessage = false;
	do
	{
		consume_token_result TokenConsumed = ConsumeToken(Packet, IRCMessageDelimiter, &IRC->Arena);
		Message = TokenConsumed.Token;
		Packet = TokenConsumed.Remain;

		if(IRC->PartialLastMessage.Size > 0)
		{
			Message = ConcatString(IRC->PartialLastMessage, Message, &IRC->Arena);
			FreeString(&IRC->PartialLastMessage);
		}

		IRCParseMessage(Message, &IRC->Arena);

		IsLastMessage = !IsSubstring(IRCMessageDelimiter, Packet);
		if(IsLastMessage && (!FullPacket))
		{
			IRC->PartialLastMessage = CreateString(Packet);
		}

	} while((Packet.Size > 0) && (IRC->PartialLastMessage.Size == 0));
}

void SendMessageToSocket(TCPsocket Socket, const char* Message)
{
	u32 Length = (u32)(StringLength(Message));
	SDLNet_TCP_Send(Socket, Message, Length);
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
				bool PingSent = false;
				bool ModeSent = false;
				u32 AuthTick = 0;

				// NOTE(hugo) : Initializing memory for arena
				u64 MemoryStorageSize = Megabytes(512);
				void* MemoryStorage = Allocate_(MemoryStorageSize);

				irc_state IRCState = {};
				InitializeArena(&IRCState.Arena, MemoryStorageSize, MemoryStorage);
				while(true)
				{
					// TODO(hugo) : Fix infinite loop ?
					u32 SocketReadyCount = SDLNet_CheckSockets(Sockets, 1000); // NOTE(hugo) : We wait 1s max
					Assert(SocketReadyCount != -1);

					// NOTE(hugo) : Receive messages
					if(SocketReadyCount > 0)
					{
						Assert(SDLNet_SocketReady(ServerSocket));

						// NOTE(hugo): 512 was dictated by the maximum size of a IRC message
						// as specified in RFC 2812
						char Text[512];

						SDLNet_TCP_Recv(ServerSocket, Text, ArrayCount(Text));

						temporary_memory TempMemory = BeginTemporaryMemory(&IRCState.Arena);
						rvtn_string Packet = CreateString(Text, &IRCState.Arena);
						IRCParsePacket(&IRCState, Packet);
						EndTemporaryMemory(TempMemory);
					}

					// NOTE(hugo) : Send messages
					if(!AuthentificationDone)
					{
						SendMessageToSocket(ServerSocket, "CAP LS\r\n");
						printf("Authentification...\r\n");
						SendMessageToSocket(ServerSocket, "NICK rivten2\r\nUSER hugo hugo irc.handmade.network :rivten_grey\r\n");
						AuthentificationDone = true;
						AuthTick = SDL_GetTicks();
					}
					else
					{
						u32 MSSinceAuth = SDL_GetTicks() - AuthTick;
						u32 MSToJoin = 2 * 2 * 5000;
						if((MSSinceAuth > (u32)(MSToJoin / 4)) && (!ModeSent))
						{
							printf("Sending MODE\n");
							SendMessageToSocket(ServerSocket, "MODE rivten2 +i\r\n");
							ModeSent = true;
						}
						if((MSSinceAuth > (u32)(MSToJoin / 2)) && (!PingSent))
						{
							printf("Sending PING\n");
							SendMessageToSocket(ServerSocket, "PING irc.handmade.network\r\n");
							PingSent = true;
						}
						if((MSSinceAuth > MSToJoin) && (!ChannelJoined))
						{
							printf("--------Joining...\r\n");
							SendMessageToSocket(ServerSocket, "JOIN #random\r\n");
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
