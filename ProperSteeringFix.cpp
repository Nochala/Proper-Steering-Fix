
// Written by: 
// 
// ███╗   ██╗ ██████╗  ██████╗██╗  ██╗ █████╗ ██╗      █████╗ 
// ████╗  ██║██╔═══██╗██╔════╝██║  ██║██╔══██╗██║     ██╔══██╗
// ██╔██╗ ██║██║   ██║██║     ███████║███████║██║     ███████║
// ██║╚██╗██║██║   ██║██║     ██╔══██║██╔══██║██║     ██╔══██║
// ██║ ╚████║╚██████╔╝╚██████╗██║  ██║██║  ██║███████╗██║  ██║
// ╚═╝  ╚═══╝ ╚═════╝  ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝
//               https://github.com/Nochala
//                      N O C H A L A

#include <windows.h>
#include <psapi.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
#include <share.h>

#pragma comment(lib, "Psapi.lib")

#include "script.h"
#include "natives.h"

static int ReadIniInt(const char* file, const char* section, const char* key, int defVal)
{
    return GetPrivateProfileIntA(section, key, defVal, file);
}

static void Notify(const std::string& text)
{
    UI::_SET_NOTIFICATION_TEXT_ENTRY("STRING");
    UI::_ADD_TEXT_COMPONENT_STRING((char*)text.c_str());
    UI::_DRAW_NOTIFICATION(false, false);
}

static bool g_logEnabled = true;
static bool g_useRuntimeFallback = false;
static const char* g_logPath = "ProperSteeringFix.log";

static void ResetLogFile()
{
    if (!g_logEnabled) return;

    FILE* f = _fsopen(g_logPath, "w", _SH_DENYNO);
    if (f)
    {
        fclose(f);
    }
}

static void Logf(const char* fmt, ...)
{
    if (!g_logEnabled) return;

    FILE* f = _fsopen(g_logPath, "a", _SH_DENYNO);
    if (!f) return;

    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);

    fprintf(f, "\n");
    fflush(f);
    fclose(f);
}

static uint8_t* FindPatternSig(uint8_t* start, uint8_t* end, const char* signature)
{
    if (!start || !end || !signature) return nullptr;

    std::vector<int> pat;
    pat.reserve(64);

    const char* s = signature;
    while (*s)
    {
        while (*s == ' ') ++s;
        if (!*s) break;

        if (s[0] == '?')
        {
            pat.push_back(-1);
            if (s[1] == '?') s += 2; else s += 1;
            continue;
        }

        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            return -1;
            };

        char c1 = s[0];
        char c2 = s[1];
        if (!c2) break;

        int hi = hexVal(c1);
        int lo = hexVal(c2);
        if (hi < 0 || lo < 0) break;

        pat.push_back((hi << 4) | lo);
        s += 2;
    }

    if (pat.empty()) return nullptr;

    const size_t plen = pat.size();
    for (uint8_t* p = start; p + plen <= end; ++p)
    {
        bool ok = true;
        for (size_t i = 0; i < plen; ++i)
        {
            if (pat[i] != -1 && p[i] != (uint8_t)pat[i]) { ok = false; break; }
        }
        if (ok) return p;
    }
    return nullptr;
}

static bool PatchNops(void* addr, size_t count)
{
    DWORD oldProt = 0;
    if (!VirtualProtect(addr, count, PAGE_EXECUTE_READWRITE, &oldProt))
        return false;

    memset(addr, 0x90, count);
    FlushInstructionCache(GetCurrentProcess(), addr, count);

    DWORD tmp = 0;
    VirtualProtect(addr, count, oldProt, &tmp);
    return true;
}

static bool IsEnhancedVersion()
{
    // ScriptHookV: Enhanced builds report >= 1000.
    return (getGameVersion() >= 1000);
}

static const char* GetEditionTag(bool enhanced)
{
    return enhanced ? "Enhanced" : "Legacy";
}

static bool GetExeRange(uint8_t*& base, uint8_t*& end)
{
    base = nullptr;
    end = nullptr;

    HMODULE hExe = GetModuleHandleA(nullptr);
    if (!hExe)
    {
        Logf("[Error] GetModuleHandleA(nullptr) failed (GLE=%lu)", GetLastError());
        return false;
    }

    MODULEINFO mi{};
    if (!K32GetModuleInformation(GetCurrentProcess(), hExe, &mi, sizeof(mi)))
    {
        Logf("[Error] K32GetModuleInformation failed (GLE=%lu)", GetLastError());
        return false;
    }

    base = reinterpret_cast<uint8_t*>(mi.lpBaseOfDll);
    end = base + mi.SizeOfImage;
    return true;
}

static bool FindAndPatchNops(
    uint8_t* base,
    uint8_t* end,
    const char* editionTag,
    const char* label,
    const char* signature,
    ptrdiff_t patchOffsetFromMatch,
    size_t nopCount)
{
    Logf("[Op] (%s) Finding %s...", editionTag, label);

    uint8_t* hit = FindPatternSig(base, end, signature);
    if (!hit)
    {
        Logf("[Error] (%s) %s not found", editionTag, label);
        return false;
    }

    uint8_t* patchAddr = hit + patchOffsetFromMatch;
    Logf(
        "[Op] (%s) Found %s at %p -> patch %p, patching %zu bytes (NOP)",
        editionTag,
        label,
        hit,
        patchAddr,
        nopCount);

    if (!PatchNops(patchAddr, nopCount))
    {
        Logf("[Error] (%s) %s patch failed (GLE=%lu)", editionTag, label, GetLastError());
        return false;
    }

    return true;
}

static bool ApplyLegacyPatches()
{
    uint8_t* base = nullptr;
    uint8_t* end = nullptr;
    if (!GetExeRange(base, end))
        return false;

    Logf("[Info] Legacy base=%p size=0x%lx", base, (unsigned long)(end - base));

    const char* LEG_SIG_1 = "44 89 BB ?? ?? ?? ?? 8B 0D";
    const char* LEG_SIG_2 = "89 82 ?? ?? ?? ?? 38 81";

    if (!FindAndPatchNops(base, end, "Legacy", "steering pattern #1", LEG_SIG_1, 0, 7))
        return false;

    if (!FindAndPatchNops(base, end, "Legacy", "steering pattern #2", LEG_SIG_2, 0, 6))
        return false;

    Logf("[OK] (Legacy) Steering patches applied.");
    return true;
}

static bool ApplyEnhancedPatches()
{
    uint8_t* base = nullptr;
    uint8_t* end = nullptr;
    if (!GetExeRange(base, end))
        return false;

    Logf("[Info] Enhanced base=%p size=0x%lx", base, (unsigned long)(end - base));

    const char* ENH_SIG_MOVING =
        "31 C0 80 B9 ? ? ? ? ? 0F 94 C0 48 8D 15 ? ? ? ? F3 0F 10 04 82";
    const char* ENH_SIG_STATIONARY =
        "83 F8 ? 0F 84 ? ? ? ? 8B 87 ? ? ? ? 83 E0 ? 0F 85";

    if (!FindAndPatchNops(base, end, "Enhanced", "moving auto-center locator", ENH_SIG_MOVING, -10, 10))
        return false;

    if (!FindAndPatchNops(base, end, "Enhanced", "stationary auto-center locator", ENH_SIG_STATIONARY, +24, 10))
        return false;

    Logf("[OK] (Enhanced) Steering patches applied.");
    return true;
}

static void RunSteeringFixOnce(Ped playerPed, Vehicle curVeh)
{
    (void)playerPed;

    if (!ENTITY::DOES_ENTITY_EXIST(curVeh)) return;

    Hash vModel = ENTITY::GET_ENTITY_MODEL(curVeh);
    if (VEHICLE::IS_THIS_MODEL_A_BIKE(vModel) || VEHICLE::IS_THIS_MODEL_A_BICYCLE(vModel) || VEHICLE::IS_THIS_MODEL_A_QUADBIKE(vModel))
        return;

    Vector3 pos = ENTITY::GET_ENTITY_COORDS(curVeh, true);
    float heading = ENTITY::GET_ENTITY_HEADING(curVeh);

    Hash bmxHash = GAMEPLAY::GET_HASH_KEY((char*)"bmx");
    if (!STREAMING::IS_MODEL_IN_CDIMAGE(bmxHash) || !STREAMING::IS_MODEL_A_VEHICLE(bmxHash))
        return;

    STREAMING::REQUEST_MODEL(bmxHash);
    int tries = 0;
    while (!STREAMING::HAS_MODEL_LOADED(bmxHash) && tries++ < 200) WAIT(0);

    if (!STREAMING::HAS_MODEL_LOADED(bmxHash))
        return;

    // Spawn exactly at the vehicle position to avoid an immediate constraint "pull".
    Vehicle anchor = VEHICLE::CREATE_VEHICLE(
        bmxHash,
        pos.x,
        pos.y,
        pos.z,
        heading,
        true,
        true
    );
    STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(bmxHash);

    if (!ENTITY::DOES_ENTITY_EXIST(anchor))
        return;

    // Make anchor effectively non-interactive.
    ENTITY::SET_ENTITY_ALPHA(anchor, 0, true);
    ENTITY::SET_ENTITY_COLLISION(anchor, false, false);
    ENTITY::FREEZE_ENTITY_POSITION(anchor, true);
    ENTITY::SET_ENTITY_INVINCIBLE(anchor, true);

    // Preserve current velocity so we don't "kick" the vehicle.
    Vector3 vvel = ENTITY::GET_ENTITY_VELOCITY(curVeh);

    ENTITY::ATTACH_ENTITY_TO_ENTITY_PHYSICALLY(
        curVeh,
        anchor,
        0,
        0,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        1.0f,
        true,
        false,
        false,
        1,
        true
    );

    // Restore velocity immediately after attach (helps reduce micro-jitters).
    ENTITY::SET_ENTITY_VELOCITY(curVeh, vvel.x, vvel.y, vvel.z);

    // Hold just long enough to cover the snap window at exit.
    WAIT(250);

    ENTITY::DETACH_ENTITY(curVeh, true, true);

    Entity e = anchor;
    ENTITY::DELETE_ENTITY(&e);
}

void ScriptMain()
{
    const char* iniPath = ".\\ProperSteeringFix.ini";

    // INI: logging must be decided before first write.
    g_logEnabled = (ReadIniInt(iniPath, "Settings", "EnableLog", 1) != 0);
    if (g_logEnabled)
    {
        ResetLogFile();
        Logf("ProperSteeringFix starting...");
    }

    int showNotif = ReadIniInt(iniPath, "Settings", "ShowNotification", 1);
    const char* notifText = "~b~~h~[ Proper Steering Fix ]~h~~w~  ~w~~h~Loaded~w~";
    const char* fallbackNotifText = "~y~~h~[ Proper Steering Fix ]~h~~w~  ~w~~h~Fallback Loaded~w~";
    const int gv = getGameVersion();
    const bool enhanced = IsEnhancedVersion();
    const char* editionTag = GetEditionTag(enhanced);

    Logf("[Info] Script startup");
    Logf("[Info] Detected edition: %s", editionTag);
    Logf("[Info] getGameVersion()=%d", gv);
    Logf("[Info] Logging enabled: %s", g_logEnabled ? "yes" : "no");
    Logf("[Info] Attempting %s binary steering patch path...", editionTag);

    bool patchInstalled = false;
    if (enhanced)
    {
        patchInstalled = ApplyEnhancedPatches();
    }
    else
    {
        patchInstalled = ApplyLegacyPatches();
    }

    if (!patchInstalled)
    {
        g_useRuntimeFallback = true;
        Logf("[Warn] (%s) Binary patch scan failed.", editionTag);
        Logf("[Warn] (%s) Runtime fallback applied: YES", editionTag);
        Logf("[Info] (%s) Final mode: runtime fallback workaround", editionTag);
        Logf("[Info] (%s) Runtime fallback loop will monitor exit input and recent steering input.", editionTag);
    }
    else
    {
        g_useRuntimeFallback = false;
        Logf("[OK] (%s) Binary patch installed successfully.", editionTag);
        Logf("[Info] (%s) Runtime fallback applied: NO", editionTag);
        Logf("[Info] (%s) Final mode: binary patch", editionTag);
    }

    if (showNotif == 1)
    {
        if (patchInstalled)
        {
            Notify(notifText);
        }
        else
        {
            Notify(fallbackNotifText);
        }
    }

    ULONGLONG nextAllowed = 0;
    ULONGLONG lastSteerInputMs = 0;
    const float kSteerDeadzone = 0.05f;
    const ULONGLONG kRecentSteerWindowMs = 300ULL;

    bool exitWasDown = false;
    while (true)
    {
        WAIT(0);

        if (!g_useRuntimeFallback)
            continue;

        const ULONGLONG now = GetTickCount64();
        if (now < nextAllowed)
            continue;

        Ped ped = PLAYER::PLAYER_PED_ID();
        if (!ENTITY::DOES_ENTITY_EXIST(ped))
            continue;

        if (!PED::IS_PED_IN_ANY_VEHICLE(ped, true))
            continue;

        float steer = CONTROLS::GET_CONTROL_NORMAL(2, 59);
        if (steer > kSteerDeadzone || steer < -kSteerDeadzone)
            lastSteerInputMs = now;

        const bool exitDown = (CONTROLS::IS_CONTROL_PRESSED(2, 75) != 0);
        const bool exitJustPressed = (exitDown && !exitWasDown);
        exitWasDown = exitDown;
        if (!exitJustPressed)
            continue;

        if ((now - lastSteerInputMs) > kRecentSteerWindowMs)
            continue;

        Vehicle v = PED::GET_VEHICLE_PED_IS_IN(ped, false);
        if (!ENTITY::DOES_ENTITY_EXIST(v))
            continue;

        RunSteeringFixOnce(ped, v);
        nextAllowed = now + 350ULL;
    }
}
