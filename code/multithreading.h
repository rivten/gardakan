// NOTE(hugo): The includes below are for the SDL/GL layer only
#ifdef _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "rivten.h"

struct platform_work_queue;
#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue* Queue, void* Data)
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

struct platform_work_queue_entry
{
	platform_work_queue_callback* Callback;
	void* Data;
};

struct platform_work_queue
{
	u32 volatile CompletionGoal;
	u32 volatile CompletionCount;

	u32 volatile NextEntryToRead;
	u32 volatile NextEntryToWrite;

	SDL_sem* SemaphoreHandle;
	platform_work_queue_entry Entries[256];
};

struct thread_startup
{
	platform_work_queue* Queue;
};

static void AddEntry(platform_work_queue* Queue, platform_work_queue_callback* Callback, void* Data)
{
	u32 NewNextEntryToWrite = (Queue->NextEntryToWrite + 1) % ArrayCount(Queue->Entries);
	Assert(NewNextEntryToWrite != Queue->NextEntryToRead);
	platform_work_queue_entry* Entry = Queue->Entries + Queue->NextEntryToWrite;
	Entry->Callback = Callback;
	Entry->Data = Data;
	++Queue->CompletionGoal;
	SDL_CompilerBarrier();
	Queue->NextEntryToWrite = NewNextEntryToWrite;
	SDL_SemPost(Queue->SemaphoreHandle);
}

static bool DoNextWorkQueueEntry(platform_work_queue* Queue)
{
	bool WeShouldSleep = false;

	u32 OriginalNextEntryToRead = Queue->NextEntryToRead;
	u32 NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);
	if(OriginalNextEntryToRead != Queue->NextEntryToWrite)
	{
		SDL_bool WasSet = SDL_AtomicCAS((SDL_atomic_t *)&Queue->NextEntryToRead,
				OriginalNextEntryToRead, NewNextEntryToRead);

		if(WasSet)
		{
			platform_work_queue_entry Entry = Queue->Entries[OriginalNextEntryToRead];
			Entry.Callback(Queue, Entry.Data);
			SDL_AtomicIncRef((SDL_atomic_t *)&Queue->CompletionCount);
		}
	}
	else
	{
		WeShouldSleep = true;
	}

	return(WeShouldSleep);
}

static void CompleteAllWork(platform_work_queue* Queue)
{
	while(Queue->CompletionGoal != Queue->CompletionCount)
	{
		DoNextWorkQueueEntry(Queue);
	}

	Queue->CompletionGoal = 0;
	Queue->CompletionCount = 0;
}

int ThreadProc(void* Parameter)
{
	thread_startup* Thread = (thread_startup *)Parameter;
	platform_work_queue* Queue = Thread->Queue;

	for(;;)
	{
		if(DoNextWorkQueueEntry(Queue))
		{
			SDL_SemWait(Queue->SemaphoreHandle);
		}
	}
}

static void MakeQueue(platform_work_queue* Queue, u32 ThreadCount,
		thread_startup* Startups)
{
	Queue->CompletionGoal = 0;
	Queue->CompletionCount = 0;

	Queue->NextEntryToWrite = 0;
	Queue->NextEntryToRead = 0;

	u32 InitialCount = 0;
	Queue->SemaphoreHandle = SDL_CreateSemaphore(InitialCount);

	for(u32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
	{
		thread_startup* Startup = Startups + ThreadIndex;
		Startup->Queue = Queue;

		SDL_Thread* ThreadHandle = SDL_CreateThread(ThreadProc, 0, Startup);
		SDL_DetachThread(ThreadHandle);
	}
}

