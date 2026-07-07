#include <Windows.h>
#include <iostream>
#include <conio.h>
#include <stdio.h>
#include "utils.h"
#include <fstream>
#include <string>
#include <conio.h>
#include <stdio.h>
#include "Detours/src/detours.h"
#include "convar.h"
#include "VEHhook.h"

// IInputSystem and CInput interfaces (assuming definitions are in a header)
// Credits to Haze, schweiziske, FiBzY

// Function pointer types for hooking
typedef bool(__thiscall* GetRawMouseAccumulatorsFn)(void*, int&, int&);
typedef LRESULT(__thiscall* WindowProcFn)(void*, HWND, UINT, WPARAM, LPARAM);
typedef void(__thiscall* GetAccumulatedMouseDeltasAndResetAccumulatorsFn)(void*, float*, float*);
typedef void(__thiscall* ControllerMoveFn)(byte*, float, void*);
typedef void(__thiscall* In_SetSampleTimeFn)(void*, float);
typedef void(__thiscall* CreateMoveFn)(void*, int, float, bool);
typedef void(__thiscall* ExtraMouseSampleFn)(void*, float, bool);
typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);

// Global variables for interfaces
CInput* g_Input = nullptr;
IInputSystem* g_InputSystem = nullptr;

// Original function pointers
GetRawMouseAccumulatorsFn oGetRawMouseAccumulators;
WindowProcFn oWindowProc;
GetAccumulatedMouseDeltasAndResetAccumulatorsFn oGetAccumulatedMouseDeltasAndResetAccumulators;
ControllerMoveFn oControllerMove;
In_SetSampleTimeFn oIn_SetSampleTime;
CreateMoveFn oCreateMove;
ExtraMouseSampleFn oExtraMouseSample;

// Function pointers for tier0.dll functions
CCvar* g_pCVar;
ConVar* m_rawinput_cvar;

typedef double(__cdecl* Plat_FloatTimeFn)();
Plat_FloatTimeFn Plat_FloatTime;

// Global variables for mouse handling
float mouseMoveFrameTime;
double m_mouseSplitTime;
double m_mouseSampleTime;
float m_flMouseSampleTime;

int* m_mouseRawAccumX = nullptr;
int* m_mouseRawAccumY = nullptr;
bool* m_bRawInputSupported = nullptr;

uintptr_t g_exitPointAddress = 0;

// Utility function to display an error and exit
void Error(const char* text)
{
	MessageBoxA(0, (LPSTR)text, "ERROR", MB_ICONERROR | MB_OK);
	ExitProcess(0);
}

bool GetRawMouseAccumulators(int& accumX, int& accumY, double frame_split)
{

	//ConMsg("GetRawMouseAccumulators: %d | %d | %d\n", *(int*)m_mouseRawAccumX, *(int*)m_mouseRawAccumY, *(bool*)m_bRawInputSupported);

	MSG msg;
	if (frame_split != 0.0 && PeekMessageW(&msg, NULL, WM_INPUT, WM_INPUT, PM_REMOVE))
	{
		do
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		} while (PeekMessageW(&msg, NULL, WM_INPUT, WM_INPUT, PM_REMOVE));
	}

	double mouseSplitTime = m_mouseSplitTime;
	if (mouseSplitTime == 0.0)
	{
		mouseSplitTime = m_mouseSampleTime - 0.01;
		m_mouseSplitTime = mouseSplitTime;
	}

	double mouseSampleTime = m_mouseSampleTime;

	if (abs(mouseSplitTime - mouseSampleTime) >= 0.000001)
	{
		if (frame_split == 0.0 || frame_split >= mouseSampleTime)
		{
			accumX = *m_mouseRawAccumX;
			accumY = *m_mouseRawAccumY;
			*m_mouseRawAccumX = 0;
			*m_mouseRawAccumY = 0;

			m_mouseSplitTime = m_mouseSampleTime;

			return *m_bRawInputSupported;
		}
		else if (frame_split >= mouseSplitTime)
		{
			float splitSegment = (frame_split - mouseSplitTime) / (mouseSampleTime - mouseSplitTime);

			accumX = splitSegment * (*m_mouseRawAccumX);
			accumY = splitSegment * (*m_mouseRawAccumY);

			*m_mouseRawAccumX -= accumX;
			*m_mouseRawAccumY -= accumY;

			m_mouseSplitTime = frame_split;

			return *m_bRawInputSupported;
		}
	}

	accumX = accumY = 0;

	return *m_bRawInputSupported;
}

void GetAccumulatedMouseDeltasAndResetAccumulators(CInput* thisptr, float* mx, float* my, float frametime)
{

	//Assert(mx);
	//Assert(my);

	float* m_flAccumulatedMouseXMovement = (float*)((uintptr_t)thisptr + 0x0);
	float* m_flAccumulatedMouseYMovement = (float*)((uintptr_t)thisptr + 0x0);

	int rawinputoffset = 0x0;
	bool* m_bRawInputSupported = (bool*)((uintptr_t)g_InputSystem + rawinputoffset);

	//ConMsg("GetAccumulatedMouseDeltasAndResetAccumulators: %.3f | %.3f | %d\n", *(float*)m_flAccumulatedMouseXMovement, *(float*)m_flAccumulatedMouseYMovement, m_rawinput_cvar->GetInt());

	if (m_flMouseSampleTime > 0.0)
	{
		int rawMouseX = 0;
		int rawMouseY = 0;

		if (m_rawinput_cvar->GetInt() != 0)
		{
			if (m_rawinput_cvar->GetInt() == 2 && frametime > 0.0)
			{
				m_flMouseSampleTime -= MIN(m_flMouseSampleTime, frametime);

				GetRawMouseAccumulators(rawMouseX, rawMouseY, Plat_FloatTime() - m_flMouseSampleTime);
			}
			else
			{
				GetRawMouseAccumulators(rawMouseX, rawMouseY, 0.0);

				m_flMouseSampleTime = 0.0;
			}
		}
		else
		{
			rawMouseX = *(float*)m_flAccumulatedMouseXMovement;
			rawMouseY = *(float*)m_flAccumulatedMouseYMovement;
		}

		*(float*)m_flAccumulatedMouseXMovement = 0.0;
		*(float*)m_flAccumulatedMouseYMovement = 0.0;

		static float previousX = 0.0f;
		static float previousY = 0.0f;

		float outX = (rawMouseX + previousX) * 0.5f;
		float outY = (rawMouseY + previousY) * 0.5f;

		previousX = rawMouseX;
		previousY = rawMouseY;

		*mx = outX;
		*my = outY;
	}
	else
	{
		*mx = 0.0f;
		*my = 0.0f;
	}
}

bool __fastcall Hooked_GetRawMouseAccumulators(CInput* thisptr, int& accumX, int& accumY)
{
	return GetRawMouseAccumulators(accumX, accumY, 0.0);

	//GetRawMouseAccumulators(accumX, accumY, 0.0);
	//return oGetRawMouseAccumulators(thisptr, accumX, accumY);
}

void __fastcall Hooked_GetAccumulatedMouseDeltasAndResetAccumulators(CInput* thisptr, float* mx, float* my)
{
	//ConMsg("%f", *mx);
	GetAccumulatedMouseDeltasAndResetAccumulators(thisptr, mx, my, mouseMoveFrameTime);
	mouseMoveFrameTime = 0.0;

	//ConMsg("test: %.5f\n", mouseMoveFrameTime);
	//oGetAccumulatedMouseDeltasAndResetAccumulators(thisptr, mx, my);
}

void __fastcall Hooked_IN_SetSampleTime(void* thisptr, float frametime)
{
	m_flMouseSampleTime = frametime;
	if (!oIn_SetSampleTime) return;
	oIn_SetSampleTime(thisptr, frametime);
}

LRESULT __fastcall Hooked_WindowProc(void* thisptr, HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//ConMsg("WindowProc: %.3f\n", m_mouseSampleTime);

	if (uMsg == WM_INPUT)
	{
		if (Plat_FloatTime)
		{
			m_mouseSampleTime = Plat_FloatTime();
		}
	}
	return oWindowProc(thisptr, hwnd, uMsg, wParam, lParam);
}

void __fastcall Hooked_CreateMove(CInput* thisptr, int sequence_number, float input_sample_frametime, bool active)
{
	mouseMoveFrameTime = input_sample_frametime;
	oCreateMove(thisptr, sequence_number, input_sample_frametime, active);
}

void __fastcall Hooked_ExtraMouseSample(CInput* thisptr, float frametime, bool active)
{
	mouseMoveFrameTime = frametime;
	oExtraMouseSample(thisptr, frametime, active);
}

BOOL IsProcessRunning(DWORD processID)
{
	HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, processID);
	if (process == NULL) return FALSE;
	DWORD ret = WaitForSingleObject(process, 0);
	CloseHandle(process);
	return ret == WAIT_TIMEOUT;
}

DWORD InjectionEntryPoint(DWORD processID)
{
	if (!LoadLibraryA("VCRUNTIME140.dll")) {
		Error("Failed to load VCRUNTIME140.dll");
		return 1;
	}

	HMODULE tier0_handle = GetModuleHandleA("tier0.dll");
	if (!tier0_handle) {
		Error("Failed to get handle for tier0.dll");
		return 1;
	}

	ConMsg = (ConMsgFn)(uintptr_t)GetProcAddress((HMODULE)tier0_handle, "?ConMsg@@YAXPEBDZZ");
	if (!ConMsg) Error("Failed to find ConMsg in tier0.dll");

	// Get factory from inputsystem.dll
	auto inputsystem_factory = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(GetModuleHandleA("inputsystem.dll"), "CreateInterface"));
	if (!inputsystem_factory) {
		Error("Failed to find CreateInterface in inputsystem.dll");
		return 1;
	}
	g_InputSystem = reinterpret_cast<IInputSystem*>(inputsystem_factory("InputSystemVersion001", nullptr));
	if (g_InputSystem) {
		m_mouseRawAccumX = (int*)((uintptr_t)g_InputSystem + 0x5fa0);
		m_mouseRawAccumY = (int*)((uintptr_t)g_InputSystem + 0x5fa4);
		m_bRawInputSupported = (bool*)((uintptr_t)g_InputSystem + 0x5fac);
	}

	auto vstdlib_factory = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(GetModuleHandleA("vstdlib.dll"), "CreateInterface"));

	g_pCVar = reinterpret_cast<CCvar*>(vstdlib_factory("VEngineCvar007", nullptr));
	while (!m_rawinput_cvar)
	{
		m_rawinput_cvar = g_pCVar->FindVar("m_rawinput");
		Sleep(100);
	}

	oGetRawMouseAccumulators = (GetRawMouseAccumulatorsFn)(FindPattern("inputsystem.dll", ""));
	if (!oGetRawMouseAccumulators) Error("Failed to find pattern for GetRawMouseAccumulators");

	oWindowProc = (WindowProcFn)(FindPattern("inputsystem.dll", ""));
	if (!oWindowProc) Error("Failed to find pattern for WindowProc");

	oGetAccumulatedMouseDeltasAndResetAccumulators = (GetAccumulatedMouseDeltasAndResetAccumulatorsFn)(FindPattern("client.dll", ""));
	if (!oGetAccumulatedMouseDeltasAndResetAccumulators) Error("Failed to find pattern for GetAccumulatedMouseDeltasAndResetAccumulators");

	oControllerMove = (ControllerMoveFn)(FindPattern("client.dll", ""));
	if (!oControllerMove) Error("Failed to find pattern for ControllerMove");

	oIn_SetSampleTime = (In_SetSampleTimeFn)(FindPattern("client.dll", ""));
	if (!oIn_SetSampleTime) Error("Failed to find pattern for In_SetSampleTime");

	oCreateMove = (CreateMoveFn)(FindPattern("client.dll", ""));
	if (!oCreateMove) Error("Failed to find pattern for oCreateMove");

	oExtraMouseSample = (ExtraMouseSampleFn)(FindPattern("client.dll", ""));
	if (!oExtraMouseSample) Error("Failed to find pattern for oExtraMouseSample");

	Plat_FloatTime = (Plat_FloatTimeFn)(uintptr_t)GetProcAddress((HMODULE)tier0_handle, "Plat_FloatTime");
	if (!Plat_FloatTime) Error("Failed to find Plat_FloatTime in tier0.dll");

	// DO TO: Hook GetMouseDelta

	//ConMsg("Plat_FloatTime: %.5f\n", Plat_FloatTime());

	uintptr_t createMoveAddress = FindPattern("client.dll", "");
	size_t start_point_offset = 0x0;
    size_t end_point_offset = 0x0; 
	uintptr_t startAddress = createMoveAddress + start_point_offset;
	uintptr_t endAddress = createMoveAddress + end_point_offset;
	
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)oGetRawMouseAccumulators, Hooked_GetRawMouseAccumulators);
	DetourAttach(&(PVOID&)oWindowProc, Hooked_WindowProc);
	DetourAttach(&(PVOID&)oIn_SetSampleTime, Hooked_IN_SetSampleTime);

	DetourAttach(&(PVOID&)oCreateMove, Hooked_CreateMove);
	DetourAttach(&(PVOID&)oExtraMouseSample, Hooked_ExtraMouseSample);
	if (DetourTransactionCommit() != NO_ERROR) {
		Error("Failed to commit Detour transaction.");
		return 1;
	}
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	//patch
	InstallHook(startAddress,endAddress, Hooked_GetAccumulatedMouseDeltasAndResetAccumulators);

	while (IsProcessRunning(processID))
	{
		Sleep(1000);
	}

	DetourDetach(&(PVOID&)oGetRawMouseAccumulators, Hooked_GetRawMouseAccumulators);
	DetourDetach(&(PVOID&)oWindowProc, Hooked_WindowProc);
	DetourDetach(&(PVOID&)oIn_SetSampleTime, Hooked_IN_SetSampleTime);
	DetourDetach(&(PVOID&)oCreateMove, Hooked_CreateMove);
	DetourDetach(&(PVOID&)oExtraMouseSample, Hooked_ExtraMouseSample);
	DetourTransactionCommit();

	UninstallHook();

	ExitThread(0);

	return 0;
}

//Credits: https://www.ired.team/offensive-security/code-injection-process-injection/pe-injection-executing-pes-inside-remote-processes
void PEInjector(HANDLE targetProcess, DWORD Func(DWORD))
{
	// Get current image's base address
	PVOID imageBase = GetModuleHandle(NULL);
	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)imageBase;
	PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)((DWORD_PTR)imageBase + dosHeader->e_lfanew);

	// Allocate a new memory block and copy the current PE image to this new memory block
	PVOID localImage = VirtualAlloc(NULL, ntHeader->OptionalHeader.SizeOfImage, MEM_COMMIT, PAGE_READWRITE);
	memcpy(localImage, imageBase, ntHeader->OptionalHeader.SizeOfImage);

	// Allote a new memory block in the target process. This is where we will be injecting this PE
	PVOID targetImage = VirtualAllocEx(targetProcess, NULL, ntHeader->OptionalHeader.SizeOfImage, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	// Calculate delta between addresses of where the image will be located in the target process and where it's located currently
	DWORD_PTR deltaImageBase = (DWORD_PTR)targetImage - (DWORD_PTR)imageBase;

	// Relocate localImage, to ensure that it will have correct addresses once its in the target process
	PIMAGE_BASE_RELOCATION relocationTable = (PIMAGE_BASE_RELOCATION)((DWORD_PTR)localImage + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
	DWORD relocationEntriesCount = 0;
	PDWORD_PTR patchedAddress;
	PBASE_RELOCATION_ENTRY relocationRVA = NULL;

	while (relocationTable->SizeOfBlock > 0)
	{
		relocationEntriesCount = (relocationTable->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(USHORT);
		relocationRVA = (PBASE_RELOCATION_ENTRY)(relocationTable + 1);

		for (DWORD i = 0; i < relocationEntriesCount; i++)
		{
			if (relocationRVA[i].Offset)
			{
				patchedAddress = (PDWORD_PTR)((DWORD_PTR)localImage + relocationTable->VirtualAddress + relocationRVA[i].Offset);
				*patchedAddress += deltaImageBase;
			}
		}
		relocationTable = (PIMAGE_BASE_RELOCATION)((DWORD_PTR)relocationTable + relocationTable->SizeOfBlock);
	}

	// Write the relocated localImage into the target process
	WriteProcessMemory(targetProcess, targetImage, localImage, ntHeader->OptionalHeader.SizeOfImage, NULL);

	// Start the injected PE inside the target process
	CreateRemoteThread(targetProcess, NULL, 0, (LPTHREAD_START_ROUTINE)((DWORD_PTR)Func + deltaImageBase), (LPVOID)GetCurrentProcessId(), 0, NULL);
}

// https://stackoverflow.com/a/14678800
std::string ReplaceString(std::string subject, const std::string& search,
	const std::string& replace) {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
	return subject;
}

std::string GetSteamPath()
{
	HKEY key;
#ifdef _WIN64
	RegOpenKeyA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", &key);
#else
	RegOpenKeyA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam", &key);
#endif
	char buf[256];
	DWORD size = sizeof(buf) / sizeof(buf[0]);
	RegQueryValueExA(key, "InstallPath", 0, NULL, (BYTE*)buf, &size);
	return std::string(buf);
}

// Assumes the libraryfolders.vdf is "well formed"
std::string GetGMODPath(std::string const& steampath)
{
	std::ifstream libraryfolders(steampath + "\\steamapps\\libraryfolders.vdf");
	std::string line, gmod_path, library_path;
	while (std::getline(libraryfolders, line))
	{
#define PPPPP "\t\t\"path\"\t\t\""
		if (line.rfind(PPPPP, 0) == 0)
		{
			library_path = line.substr(sizeof(PPPPP) - 1, line.size() - sizeof(PPPPP));
			library_path = ReplaceString(library_path, "\\\\", "\\");
		}
		if (line.rfind("\t\t\t\"4000\"", 0) == 0)
		{
			gmod_path = library_path;
			break;
		}
	}
	if (gmod_path != "")
		gmod_path += "\\steamapps\\common\\GarrysMod\\";
	return gmod_path;
}

std::string GetSteamID3()
{
	HKEY key;
	RegOpenKeyA(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam\\ActiveProcess", &key);
	DWORD steamid3, size = sizeof(steamid3);
	RegQueryValueExA(key, "ActiveUser", 0, NULL, (BYTE*)&steamid3, &size);
	return std::to_string(steamid3);
}

// Assumes "X:\Program Files (x86)\Steam\userdata\STEAMIDHERE\config\localconfig.vdf" is "well formed"
std::string GetGMODLaunchOptions(std::string const& steampath, std::string const& steamid3)
{
	std::ifstream localconfig(steampath + "\\userdata\\" + steamid3 + "\\config\\localconfig.vdf");
	std::string line;
	bool in_gmod = false;
	while (std::getline(localconfig, line))
	{
		if (line.rfind("\t\t\t\t\t\"4000\"", 0) == 0)
			in_gmod = true;
		if (line.rfind("\t\t\t\t\t}", 0) == 0)
			in_gmod = false;
#define LLLLL "\t\t\t\t\t\t\"LaunchOptions\"\t\t\""
		if (in_gmod && line.rfind(LLLLL, 0) == 0)
		{
			line = line.substr(sizeof(LLLLL) - 1, line.size() - sizeof(LLLLL));
			line = ReplaceString(line, "\\\\", "\\");
			return line;
		}
#if 1
		// You're not going to believe it but this section is required to not crash when spawning in.
		for (int i = 0; i < 5; i++)
			(void)GetCurrentProcessId();
#endif
	}
	return "";
}

//Сredits: https://github.com/alkatrazbhop/BunnyhopAPE
int main()
{
	SetConsoleTitle("RawInput2 for gmod");

	//printf("%d\n", &(((struct request_t*)0)->total));

	auto steamid3 = GetSteamID3();
	printf("steamid3  = %s\n", steamid3.c_str());
	auto steam_path = GetSteamPath();
	printf("steampath = %s\n", steam_path.c_str());
	auto launch_options = GetGMODLaunchOptions(steam_path, steamid3);
	launch_options = "-steam -game garrysmod   " + launch_options;
	printf("launchopt = %s\n", launch_options.c_str());
	auto gmod_path = GetGMODPath(steam_path);
	printf("gmod path = %s\n\n", gmod_path.c_str());
#ifdef _WIN64
	auto gmod_exe = gmod_path + "bin\\win64\\gmod.exe";
#else
	auto gmod_exe = gmod_path + "hl2.exe";
	auto bleh = gmod_path + "bin\\gmod.exe";
	if (GetFileAttributesA(bleh.c_str()) != INVALID_FILE_ATTRIBUTES)
		gmod_exe = bleh;
#endif
	launch_options = "\"" + gmod_exe + "\" " + launch_options;

	PROCESS_INFORMATION pi = {};
	STARTUPINFOA si = {};

	if (!CreateProcessA(gmod_exe.c_str(), (char*)launch_options.c_str(), NULL, NULL, FALSE, 0, NULL, gmod_path.c_str(), &si, &pi))
	{
		auto err = GetLastError();
		char* buf;
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);

		printf("CreateProcessA failed (0x%x): %s\n", err, buf);

		while (1)
		{
			if (_kbhit() && _getch() == VK_RETURN)
				return 0;
			Sleep(500);
		}

		return 1;
	}

	while (1)
	{
		auto pClient = GetModuleHandleExtern(pi.dwProcessId, "client.dll");
		if (pClient) break;
		Sleep(1000);
		DWORD exitcode;
		if (GetExitCodeProcess(pi.hProcess, &exitcode) && exitcode != STILL_ACTIVE)
			return 0;
	}

	//system("cls");
	printf("Set \"m_rawinput 2\" in game for it to take effect\n");

	PEInjector(pi.hProcess, InjectionEntryPoint);

	WaitForSingleObject(pi.hProcess, INFINITE);
	return 0;
}
