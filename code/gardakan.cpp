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
	}
	else if(InitResult == -1)
	{
		printf("SDL_Net encountered an error on init. Aborting Gardakan.");
	}
	else
	{
		InvalidCodePath;
	}

	return(0);
}
