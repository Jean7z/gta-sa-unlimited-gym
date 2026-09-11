#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>

#include <aml-psdk/game_sa/plugin.h>

#include <pthread.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>

MYMODCFG(net.psdk.samod.unlimitedgym, SA Android Unlimited Gym Training, 1.2, Jean7z)

/*
 * How the daily gym grind limit works (SA Android 2.10):
 *
 * The streamed gym scripts + the main-script gym-entry scripts all test
 *   IF gym_day > gym_final_day OR gym_month > gym_final_month
 * before they allow any training, and during a session they accumulate
 *   gym_day_fitness
 * and once that exceeds 200.0 they record the current date into
 * gym_final_day/gym_final_month, print "limit reached" (GYM1_1B) and force
 * the player off the machine. Those four vars are saved globals, so the block
 * also survives a restart ("come back tomorrow" wall).
 *
 * The mobile engine keeps every script variable in one global buffer
 * (CTheScripts::ScriptSpace). IMPORTANT: ScriptSpace is an ARRAY, not a
 * pointer-to-pointer -- the SYMBOL ADDRESS is the buffer base (verified:
 * dereferencing it reads the first bytes of the streamed script program, e.g.
 * 0x730000C03C010002 at resume). The var refs compile to ScriptSpace + byte
 * offset:
 *   +0x6924  gym_day          (real day-of-month)
 *   +0x6928  gym_month
 *   +0x692C  gym_final_day    <- keep -1
 *   +0x6930  gym_final_month  <- keep -1
 *   +0x6934  gym_day_fitness  <- keep 0.0
 *
 * Pinning gym_final_* to -1 makes the entry gate always pass; pinning
 * gym_day_fitness to 0.0 stops the 200-rep cap (no forced step-off, no
 * GYM1_1B, and it never records a "today" into the final vars).
 *
 * Implementation note: hooks on script/clocks crashed on resume in this
 * build, so we deliberately use a plain background thread that pokes the
 * ScriptSpace cells every 50 ms. No game code is modified at all.
 */

#define OFFSET_DAY          0x6924u
#define OFFSET_FINAL_DAY   0x692Cu
#define OFFSET_FINAL_MONTH 0x6930u
#define OFFSET_FITNESS     0x6934u

static ConfigEntry* cfgMaster;
static uintptr_t    g_scriptSpace  = 0;   // base address of the var buffer (symbol address)
static uintptr_t    g_validSSBase = 0;
static uintptr_t    g_validSSEnd  = 0;

// Validate that [addr, addr+len) lies inside one WRITABLE mapping, by
// parsing /proc/self/maps. Caches the result for a given base so we do not
// re-parse the maps file every 50 ms.
static bool IsMappedWritable(uintptr_t addr, size_t len)
{
    if(addr >= g_validSSBase && addr + len <= g_validSSEnd) return true;

    bool ok = false;
    FILE* f = fopen("/proc/self/maps", "r");
    if(!f) return false;
    char line[512];
    while(fgets(line, sizeof(line), f))
    {
        uintptr_t start = 0, end = 0;
        char prot[8];
        if(sscanf(line, "%lx-%lx %7s", (unsigned long*)&start, (unsigned long*)&end, prot) != 3) continue;
        if(addr >= start && addr + len <= end && strchr(prot, 'w'))
        {
            g_validSSBase = start;
            g_validSSEnd  = end;
            ok = true;
            break;
        }
    }
    fclose(f);
    return ok;
}

// Only poke when the block really looks like the gym var cluster: final_day
// must be -1 or a plausible day-of-month, or the day slot holds 1..31.
static bool LooksLikeGymVars(uintptr_t ss)
{
    int finalDay = *(int*)(ss + OFFSET_FINAL_DAY);
    int day      = *(int*)(ss + OFFSET_DAY);
    if(finalDay == -1 || (finalDay >= 1 && finalDay <= 31)) return true;
    if(day >= 1 && day <= 31) return true;
    return false;
}

static void RefreshGymPins(void)
{
    if(!g_scriptSpace) return;
    uintptr_t ss = g_scriptSpace;
    if(!IsMappedWritable(ss, OFFSET_FITNESS + 4)) return;
    if(!LooksLikeGymVars(ss)) return;

    int*   finalDay   = (int*)(ss + OFFSET_FINAL_DAY);
    if(*finalDay != -1)      *finalDay   = -1;
    int*   finalMonth = (int*)(ss + OFFSET_FINAL_MONTH);
    if(*finalMonth != -1)    *finalMonth = -1;
    float* fitness    = (float*)(ss + OFFSET_FITNESS);
    if(*fitness != 0.0f)     *fitness    = 0.0f;
}

static bool g_bLoggedPins = false;

static void* PinThread(void*)
{
    logger->Info("Pin thread started.");
    for(;;)
    {
        usleep(50000);
        RefreshGymPins();
        if(!g_scriptSpace) continue;
        uintptr_t ss = g_scriptSpace;
        bool mapped   = IsMappedWritable(ss, OFFSET_FITNESS + 4);
        bool plausible = mapped && LooksLikeGymVars(ss);
        if(!g_bLoggedPins && plausible)
        {
            logger->Info("ScriptSpace live @ 0x%lX, pins armed (day=%d finalDay=%d fitness=%f).",
                         (unsigned long)ss,
                         *(int*)(ss + OFFSET_DAY),
                         *(int*)(ss + OFFSET_FINAL_DAY),
                         *(float*)(ss + OFFSET_FITNESS));
            g_bLoggedPins = true;
        }
    }
    return nullptr;
}

ON_MOD_LOAD()
{
    logger->SetTag("UnlimitedGym");

    cfgMaster = cfg->Bind("MasterEnabled", true, "Unlimited Gym");
    if(!cfgMaster->GetBool())
    {
        logger->Info("Unlimited Gym disabled via config.");
        return;
    }

    uintptr_t pGame = aml->GetLib("libGTASA.so");
    if(!pGame)
    {
        logger->Error("libGTASA.so not found!");
        return;
    }

    uintptr_t pSymScriptSpace = GetMainLibrarySymbol<uintptr_t>("_ZN11CTheScripts11ScriptSpaceE");
    if(!pSymScriptSpace)
    {
        logger->Error("CTheScripts::ScriptSpace symbol not found!");
        return;
    }
    g_scriptSpace = pSymScriptSpace;
    logger->Info("CTheScripts::ScriptSpace base @ 0x%lX",
                 (unsigned long)pSymScriptSpace);

    pthread_t tid;
    if(pthread_create(&tid, nullptr, PinThread, nullptr) != 0)
        logger->Error("Pin thread creation failed!");
    else
        pthread_detach(tid);

    logger->Info("Unlimited Gym loaded (thread mode).");
}