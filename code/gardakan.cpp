#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_net.h>
#include <stdio.h>

#include "rivten.h"

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
			// NOTE(hugo) : Server path
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
							printf("A connection was established from client IP %#10x on port %d.\n", 
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
		else
		{
			if(ArgumentCount >= 2)
			{
				// NOTE(hugo) : Client path
				char* Host = Arguments[1];
				IPaddress HostAddress;
				s32 ResolveHostResult = SDLNet_ResolveHost(&HostAddress, Host, 4000);
				if(ResolveHostResult == 0)
				{
					TCPsocket ServerSocket = SDLNet_TCP_Open(&HostAddress);
					if(ServerSocket)
					{
						printf("Connection to server was successful!\n");
						char Text[1000];
						while(SDLNet_TCP_Recv(ServerSocket, Text, ArrayCount(Text)))
						{
							printf("%s", Text);
						}
						SDLNet_TCP_Close(ServerSocket);
					}
					else
					{
						printf("Connection to server was unsuccessful...\n");
					}
				}
				else
				{
					printf("Host was not resolved: %s", SDLNet_GetError());
				}
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
