#pragma once
#include <windows.h>
#include <iostream>
#include <vector>
#include "utils.h"
static uintptr_t g_startAddress = 0;
static uintptr_t g_endAddress = 0;
static PVOID g_pExceptionHandler = nullptr;
static uintptr_t g_hookAddress = 0;

typedef void(__thiscall* HookedFn)(CInput*, float*, float*);
static HookedFn g_pfnHookedLogic;

LONG CALLBACK VectoredExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
    
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP)
    {
        
        if (pExceptionInfo->ContextRecord->Rip == g_startAddress)
        {
            
            CInput* thisptr = (CInput*)pExceptionInfo->ContextRecord->Rbx;
            float mx, my;
            g_pfnHookedLogic(thisptr, &mx, &my);
            
            _mm_store_ss((float*)&pExceptionInfo->ContextRecord->Xmm6, _mm_load_ss(&mx));
            _mm_store_ss((float*)&pExceptionInfo->ContextRecord->Xmm7, _mm_load_ss(&my));

            pExceptionInfo->ContextRecord->Rip = g_endAddress;

            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

bool SetHardwareBreakpoint(HANDLE hThread, uintptr_t address)
{
    CONTEXT ctx = { 0 };
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    if (!GetThreadContext(hThread, &ctx)) return false;

    ctx.Dr0 = address;
    ctx.Dr7 = (1 << 0); // 在Dr0上启用执行断点

    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!SetThreadContext(hThread, &ctx)) return false;
    
    return true;
}

void RemoveHardwareBreakpoint(HANDLE hThread)
{
    CONTEXT ctx = { 0 };
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    if (GetThreadContext(hThread, &ctx))
    {
        ctx.Dr0 = 0;
        ctx.Dr7 &= ~(1 << 0);
        SetThreadContext(hThread, &ctx);
    }
}

void InstallHook(uintptr_t startAddress, uintptr_t endAddress, HookedFn pfnHook) {
    g_startAddress = startAddress;
    g_endAddress = endAddress;
    g_pfnHookedLogic = pfnHook;

    g_pExceptionHandler = AddVectoredExceptionHandler(1, VectoredExceptionHandler);
    if (!g_pExceptionHandler) return;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        if (Thread32First(hSnapshot, &te)) {
            do {
                if (te.th32OwnerProcessID == GetCurrentProcessId()) {
                    HANDLE hThread = OpenThread(THREAD_SET_CONTEXT | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
                    if (hThread) {
                        SetHardwareBreakpoint(hThread, g_startAddress);
                        CloseHandle(hThread);
                    }
                }
            } while (Thread32Next(hSnapshot, &te));
        }
        CloseHandle(hSnapshot);
    }
}

void UninstallHook() {
    if (g_pExceptionHandler) {
        RemoveVectoredExceptionHandler(g_pExceptionHandler);
        g_pExceptionHandler = nullptr;
    }

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        if (Thread32First(hSnapshot, &te)) {
            do {
                if (te.th32OwnerProcessID == GetCurrentProcessId()) {
                    HANDLE hThread = OpenThread(THREAD_SET_CONTEXT | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
                    if (hThread) {
                        RemoveHardwareBreakpoint(hThread);
                        CloseHandle(hThread);
                    }
                }
            } while (Thread32Next(hSnapshot, &te));
        }
        CloseHandle(hSnapshot);
    }
}
