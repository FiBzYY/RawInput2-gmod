#include <Windows.h>
#include <iostream>
#include <conio.h>
#include <stdio.h>
#include "utils.h"
#include "Detours/src/detours.h"
#include "convar.h"
#include "VEHhook.h"

// Credits go to Haze for the logic, schweiziske for the patterns, FiBzY for adding m_fiter 1

// IInputSystem and CInput interfaces (assuming definitions are in a header)

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

int nRawinput;
void GetAccumulatedMouseDeltasAndResetAccumulators(CInput* thisptr, float* mx, float* my, float frametime)
{
	float* m_flAccumulatedMouseXMovement = (float*)((uintptr_t)thisptr + 0x0);
	float* m_flAccumulatedMouseYMovement = (float*)((uintptr_t)thisptr + 0x0);

	nRawinput = 2;

	if (m_flMouseSampleTime > 0.0)
	{
		int rawMouseX = 0;
		int rawMouseY = 0;

		if (nRawinput != 0)
		{
			if (nRawinput == 2 && frametime > 0.0)
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

		// m_filter 1
		*(float*)m_flAccumulatedMouseXMovement = 0.0;
		*(float*)m_flAccumulatedMouseYMovement = 0.0;

		static float smoothX = 0.0f;
		static float smoothY = 0.0f;

		float rawX = (float)rawMouseX;
		float rawY = (float)rawMouseY;

		float alpha = 0.5f;

		smoothX += (rawX - smoothX) * alpha;
		smoothY += (rawY - smoothY) * alpha;

		*mx = smoothX;
		*my = smoothY;
	}
	else
	{
		*mx = 0.0f;
		*my = 0.0f;
	}
}

bool __fastcall Hooked_GetRawMouseAccumulators(CInput* thisptr, int& accumX, int& accumY)
{
	bool result = GetRawMouseAccumulators(accumX, accumY, 0.0);
	return result;
}

void __fastcall Hooked_GetAccumulatedMouseDeltasAndResetAccumulators(CInput* thisptr, float* mx, float* my)
{
	GetAccumulatedMouseDeltasAndResetAccumulators(thisptr, mx, my, mouseMoveFrameTime);

	mouseMoveFrameTime = 0.0;
}

void __fastcall Hooked_IN_SetSampleTime(void* thisptr, float frametime)
{
	m_flMouseSampleTime = frametime;
	if (!oIn_SetSampleTime) return;
	oIn_SetSampleTime(thisptr, frametime);
}

LRESULT __fastcall Hooked_WindowProc(void* thisptr, HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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

	if (!g_InputSystem) {
		Error("Failed to get IInputSystem interface");
		return 1;
	}

	auto vstdlib_factory = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(GetModuleHandleA("vstdlib.dll"), "CreateInterface"));

	// WiP
	g_pCVar = reinterpret_cast<CCvar*>(vstdlib_factory("VEngineCvar007", nullptr));
	m_rawinput_cvar = g_pCVar->FindVar("m_rawinput");

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
	UninstallHook();
	ExitThread(0);

	return 0;
}

// Injects the current PE into a target process
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

int main()
{
	SetConsoleTitleA("GMOD RawInput2");

	DWORD processID = 0;
	printf("Waiting for gmod.exe to start...");
	while (true)
	{
		processID = GetPIDByName("gmod.exe");
		if (processID) break;
		Sleep(1000);
	}
	printf(" gmod.exe found! (PID: %lu)\n", processID);

	printf("Waiting for client.dll to load...");
	while (true)
	{
		HMODULE pClient = (HMODULE)GetModuleHandleExtern(processID, "client.dll");
		if (pClient) break;
		Sleep(1000);
	}
	printf(" client.dll loaded!\n");

	system("cls");
	printf("INJECTED.\n");
	printf("Press ENTER to exit this injector window at any time.\n");

	HANDLE targetProcess = OpenProcess(MAXIMUM_ALLOWED, FALSE, processID);
	PEInjector(targetProcess, InjectionEntryPoint);

	while (_getch() != VK_RETURN) {}
	return 0;
}
