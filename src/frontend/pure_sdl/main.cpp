#include <iostream>
#include <cstring>
#include <SDL.h>

#include "version.h"
#include "Config.h"
#include "Platform.h"
#include "FrontendUtil.h"
#include "SPU.h"
#include "NDS.h"
#include "GPU.h"

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 768

struct audioCallbackContext {
	SDL_cond* audioSync;
	SDL_mutex* audioSyncLock;
};

bool EmuRunning = true;

void audioCallback(void* data, Uint8* stream, int len)
{
	audioCallbackContext* ctx = (audioCallbackContext*) data;
	len /= sizeof(s16) * 2;

	// resample incoming audio to match the output sample rate

	int len_in = Frontend::AudioOut_GetNumSamples(len);
	s16 buf_in[1024*2];

	SDL_LockMutex(ctx->audioSyncLock);
	int num_in = SPU::ReadOutput(buf_in, len_in);
	SDL_CondSignal(ctx->audioSync);
	SDL_UnlockMutex(ctx->audioSyncLock);

	if (num_in < 1)
	{
		memset(stream, 0, len*sizeof(s16)*2);
		return;
	}

	int margin = 6;
	if (num_in < len_in-margin)
	{
		int last = num_in-1;

		for (int i = num_in; i < len_in-margin; i++)
			((u32*)buf_in)[i] = ((u32*)buf_in)[last];

		num_in = len_in-margin;
	}

	Frontend::AudioOut_Resample(buf_in, num_in, (s16*)stream, len, 256);
}

inline void MTransform(float* m, float& x, float& y)
{
	x = (x * m[0]) + (y * m[2]) + m[4];
	y = (x * m[1]) + (y * m[3]) + m[5];
}

void calculateRects(float* mat, SDL_Rect* rect, double& rot)
{
	float x1 = 0;
	float y1 = 0;
	float x2 = 256;
	float y2 = 192;
	MTransform(mat, x1, y1);
	MTransform(mat, x2, y2);
	if (x1 < x2)
	{
		if (y1 < y2)
		{
			rot = 0;
			rect->x = x1;
			rect->y = y1;
			rect->w = x2 - x1;
			rect->h = y2 - y1;
		} else {
			rot = 90;
			rect->x = x1;
			rect->y = y2;
			rect->w = x2 - x1;
			rect->h = y1 - y2;
		}
	} else {
		if (y1 < y2)
		{
			rot = 180;
			rect->x = x2;
			rect->y = y2;
			rect->w = x1 - x2;
			rect->h = y1 - y2;
		} else {
			rot = 270;
			rect->x = x2;
			rect->y = y1;
			rect->w = x1 - x2;
			rect->h = y2 - y1;
		}
	}
}

int main(int argc, char** argv)
{
	srand(time(NULL));

	std::cerr << "melonDS " MELONDS_VERSION " - Pure SDL frontend\n";
	std::cerr << MELONDS_URL "\n";

	Platform::Init(argc, argv);

	if (argc < 2)
	{
		std::cerr << "Usage: melonDS_sdl <.nds ROM>\n";
		return 1;
	}

	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
	{
		std::cerr << "SDL shat itself :(\n";
		return 1;
	}

	Config::Load();

	SDL_Scancode keys[12]; // Bitfield reference at ../qt_sdl/PlatformConfig.cpp (L[81;92])
	keys [0] = SDL_GetScancodeFromKey(SDLK_a); // A
	keys [1] = SDL_GetScancodeFromKey(SDLK_y); // B
	keys [2] = SDL_GetScancodeFromKey(SDLK_SPACE); // SELECT
	keys [3] = SDL_GetScancodeFromKey(SDLK_RETURN); // START
	keys [4] = SDL_GetScancodeFromKey(SDLK_RIGHT); // >
	keys [5] = SDL_GetScancodeFromKey(SDLK_LEFT); // <	
	keys [6] = SDL_GetScancodeFromKey(SDLK_UP); // ^
	keys [7] = SDL_GetScancodeFromKey(SDLK_DOWN); // v
	keys [8] = SDL_GetScancodeFromKey(SDLK_c); // R
	keys [9] = SDL_GetScancodeFromKey(SDLK_d); // L
	keys [10] = SDL_GetScancodeFromKey(SDLK_s); // X
	keys [11] = SDL_GetScancodeFromKey(SDLK_x); // Y

	SDL_Window* mainWindow = SDL_CreateWindow("melonDS " MELONDS_VERSION " [SDL]", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
	if (!mainWindow)
	{
		std::cerr << "Window could not be created: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_Renderer* mainRenderer = SDL_CreateRenderer(mainWindow, -1, 0);
	if (!mainRenderer)
	{
		std::cerr << "Renderer could not be created: " << SDL_GetError() << std::endl;
		return 1;
	}
	SDL_Texture* topTexture = SDL_CreateTexture(mainRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 192);
	if (!topTexture)
	{
		std::cerr << "Top Screen Texture could not be created: " << SDL_GetError() << std::endl;
		return 1;
	}
	SDL_Texture* botTexture = SDL_CreateTexture(mainRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 192);
	if (!botTexture)
	{
		std::cerr << "Bottom Screen Texture could not be created: " << SDL_GetError() << std::endl;
		return 1;
	}

	audioCallbackContext acctx;
	acctx.audioSync = SDL_CreateCond();
	acctx.audioSyncLock = SDL_CreateMutex();

	int audioFreq = 48000;
	SDL_AudioSpec audioWant, audioGet;
	memset(&audioWant, 0, sizeof(SDL_AudioSpec));
	audioWant.freq = audioFreq;
	audioWant.format = AUDIO_S16LSB;
	audioWant.channels = 2;
	audioWant.samples = 1024;
	audioWant.userdata = (void*) &acctx;
	audioWant.callback = audioCallback;
	SDL_AudioDeviceID audioDev = SDL_OpenAudioDevice(nullptr, 0, &audioWant, &audioGet, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (!audioDev)
	{
		std::cerr << "Audio init failed: " << SDL_GetError() << std::endl;
		return 1;
	}
	audioFreq = audioGet.freq;
	std::cerr << "Audio output frequency: " << audioFreq << " Hz\n";
	SDL_PauseAudioDevice(audioDev, 1);

	Frontend::Init_ROM();
	Frontend::EnableCheats(false);

	Frontend::Init_Audio(audioFreq);

	NDS::Init();
	EmuRunning = true;
	GPU::InitRenderer(0); // Init software renderer
	GPU::RenderSettings videoSettings;
	videoSettings.Soft_Threaded = true;
	GPU::SetRenderSettings(0, videoSettings);

	SDL_Rect topRect;
	double topRot;
	SDL_Rect botRect;
	double botRot;
	float topMat[6];
	float botMat[6];
	Frontend::SetupScreenLayout(SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 0, 0, true);
	Frontend::GetScreenTransforms(topMat, botMat);
	calculateRects(topMat, &topRect, topRot);
	calculateRects(botMat, &botRect, botRot);

	// Start game
	if (Frontend::LoadROM(argv[1], Frontend::ROMSlot_NDS) != Frontend::Load_OK)
	{
		std::cerr << "Failed to load ROM: " << argv[1] << std::endl;
		return 1;
	}
	SDL_PauseAudioDevice(audioDev, 0);

	while (true) // Emulator main loop
	{
		SDL_PumpEvents();
		if (SDL_HasEvent(SDL_QUIT))
			break;
		u32 keyMask = 0xfff;
		const uint8_t* keyState = SDL_GetKeyboardState(nullptr);
		for (int i = 0; i < 12; i++)
			if (keyState[keys[i]])
				keyMask &= ~(1 << i);

		NDS::SetKeyMask(keyMask);

		Frontend::Mic_FeedSilence(); // TODO: Add hotkey for microphone

		uint32_t nlines = NDS::RunFrame();

		if (!EmuRunning)
			break;

		void* pixels;
		int pitch;
		int frontbuf = GPU::FrontBuffer;

		SDL_LockTexture(topTexture, nullptr, &pixels, &pitch);
		memcpy(pixels, GPU::Framebuffer[frontbuf][0], 256*192*4);
		SDL_UnlockTexture(topTexture);

		SDL_LockTexture(botTexture, nullptr, &pixels, &pitch);
		memcpy(pixels, GPU::Framebuffer[frontbuf][1], 256*192*4);
		SDL_UnlockTexture(botTexture);

		SDL_SetRenderDrawColor(mainRenderer, 0, 0, 0, 255);
		SDL_RenderClear(mainRenderer);
		SDL_RenderCopyEx(mainRenderer, topTexture, nullptr, &topRect, topRot, nullptr, SDL_FLIP_NONE);
		SDL_RenderCopyEx(mainRenderer, botTexture, nullptr, &botRect, botRot, nullptr, SDL_FLIP_NONE);
		SDL_RenderPresent(mainRenderer);

		SDL_LockMutex(acctx.audioSyncLock);
		while (SPU::GetOutputSize() > 1024)
		{
			int ret = SDL_CondWaitTimeout(acctx.audioSync, acctx.audioSyncLock, 500);
			if (ret == SDL_MUTEX_TIMEDOUT)
				break;
		}
		SDL_UnlockMutex(acctx.audioSyncLock);
	}

	std::cerr << "Shutting down...\n";

	GPU::DeInitRenderer();
	NDS::DeInit();

	Frontend::DeInit_ROM();
	SDL_CloseAudioDevice(audioDev);
	SDL_Quit();
	return 0;
}
