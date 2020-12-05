#include "Config.h"

namespace Config
{

int ConsoleType;
int DirectBoot;
int SavestateRelocSRAM;

ConfigEntry PlatformConfigFile[] =
{
	{"ConsoleType", 0, &ConsoleType, 0, nullptr, 0},
	{"DirectBoot", 0, &DirectBoot, 1, nullptr, 0},
	{"SavStaRelocSRAM", 0, &SavestateRelocSRAM, 0, nullptr, 0},
	{"", -1, nullptr, 0, nullptr, 0}
};

}
