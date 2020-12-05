#include <iostream>
#include <string>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <SDL.h>
#include <stdio.h>

#include "Platform.h"

extern bool EmuRunning;

namespace Platform
{

std::string ConfDirectory;

void Init(int argc, char**argv)
{
	// Be lazy for now
	char* confighome = std::getenv("XDG_CONFIG_HOME");
	if (!confighome)
		confighome = (char*)".";
	ConfDirectory = confighome;
	ConfDirectory.append("/melonDS/");
}

void DeInit()
{
}

void StopEmu()
{
	EmuRunning = false;
}

FILE* OpenFile(const char* path, const char* mode, bool mustexist)
{
	if (mustexist)
	{
		FILE* temp = fopen(path, "r");
		if (!temp)
			return nullptr;
		fclose(temp);
	}

	return fopen(path, mode);
}

FILE* OpenLocalFile(const char* path, const char* mode)
{
	if (path[0] == '/') // FIXME: Crappy Unixy Method for checking absolute paths
		return OpenFile(path, mode);

	// Search Config directory
	FILE* retval = OpenFile((ConfDirectory + std::string(path)).data(), mode);
	if (!retval) // Fallback to working directory
		retval = OpenFile(path, mode);

	return retval;
}

Thread* Thread_Create(void (* func)())
{
	return (Thread*) (new std::thread(func));
}

void Thread_Free(Thread* thread)
{
	delete ((std::thread*) thread);
}

void Thread_Wait(Thread* thread)
{
	try
	{
		((std::thread*) thread)->join();
	} catch (std::system_error& e) {
		return;
	}
}

Mutex* Mutex_Create()
{
	return (Mutex*) (new std::mutex());
}

void Mutex_Free(Mutex* mutex)
{
	delete ((std::mutex*) mutex);
}

void Mutex_Lock(Mutex* mutex)
{
	((std::mutex*) mutex)->lock();
}

void Mutex_Unlock(Mutex* mutex)
{
	((std::mutex*) mutex)->unlock();
}

bool Mutex_TryLock(Mutex* mutex)
{
	return ((std::mutex*) mutex)->try_lock();
}

Semaphore* Semaphore_Create()
{
	return ((Semaphore*) SDL_CreateSemaphore(0));
}

void Semaphore_Free(Semaphore* sema)
{
	SDL_DestroySemaphore((SDL_sem*)sema);
}

void Semaphore_Reset(Semaphore* sema)
{
	while (SDL_SemValue((SDL_sem*)sema) > 0)
		SDL_SemWait((SDL_sem*)sema);
}

void Semaphore_Wait(Semaphore* sema)
{
	SDL_SemWait((SDL_sem*)sema);
}

void Semaphore_Post(Semaphore* sema, int count)
{
	for (int i = 0; i < count; i++)
		SDL_SemPost((SDL_sem*)sema);
}

void* GL_GetProcAddress(const char* proc)
{
	std::cerr << "GL PANIC: " << proc << std::endl;
	return nullptr;
}

bool MP_Init()
{
	return false;
}

void MP_DeInit()
{
}

int MP_SendPacket(u8* data, int len)
{
	return len;
}

int MP_RecvPacket(u8* data, bool block)
{
	return 0;
}

bool LAN_Init()
{
	return false;
}

void LAN_DeInit()
{
}

int LAN_SendPacket(u8* data, int len)
{
	return len;
}

int LAN_RecvPacket(u8* data)
{
	return 0;
}

}
