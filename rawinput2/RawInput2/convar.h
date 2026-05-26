#pragma once
#include <cstdint>

#define FORCEINLINE __forceinline
typedef long long			intp;


class ConVar {

public:
	int GetInt(void){
		ConVar* m_pParent = (ConVar*)((uintptr_t)this + 0x38);

		int iVal = *(int*)((uintptr_t)m_pParent + 0x58);
		
		return iVal ^ (intp)m_pParent;
	}
};



class CCvar
{
public:
	/*0*/	virtual void Connect(void* (*)(char const*, int*)) = 0;
	/*1*/	virtual void Disconnect(void) = 0;
	/*2*/	virtual void* QueryInterface(char const*) = 0;
	/*3*/	virtual void* Init(void) = 0;
	/*4*/	virtual void Shutdown(void) = 0;

	/*5*/	virtual void* Nothing1(void) = 0;
	/*6*/	virtual void* Nothing2(void) = 0;
	/*7*/	virtual void* Nothing3(void) = 0;
	/*8*/	virtual void* Nothing4(void) = 0;

	/*10*/	virtual void* AllocateDLLIdentifier(void) = 0;
	/*11*/	virtual void RegisterConCommand(void*) = 0;
	/*12*/	virtual void UnregisterConCommand(void*) = 0;
	/*13*/	virtual void UnregisterConCommands(int) = 0;
	/*14*/	virtual const char* GetCommandLineValue(char const*) = 0;
	/*15*/	virtual void* FindCommandBase(char const*) = 0;
	/*16*/	virtual const void* FindCommandBase(char const*)const = 0;
	/*17*/	virtual ConVar* FindVar(char const* var_name) = 0;
	/*18*/	virtual ConVar* FindVar(char const* var_name)const = 0;
	/*19*/	virtual void* FindCommand(char const*) = 0;
	/*20*/	virtual void* FindCommand(char const*)const = 0;

	/*21*/	virtual void InstallGlobalChangeCallback(void (*)(ConVar*, char const*, float)) = 0;
	/*22*/	virtual void RemoveGlobalChangeCallback(void (*)(ConVar*, char const*, float)) = 0;
	/*23*/	virtual void CallGlobalChangeCallbacks(ConVar*, char const*, float) = 0;
	/*24*/	virtual void InstallConsoleDisplayFunc(void*) = 0;
	/*25*/	virtual void RemoveConsoleDisplayFunc(void*) = 0;
	/*26*/	virtual void ConsoleColorPrintf(void*, char const*, ...)const = 0;
	/*27*/	virtual void ConsolePrintf(char const*, ...)const = 0;
	/*28*/	virtual void ConsoleDPrintf(char const*, ...)const = 0;
	/*29*/	virtual void RevertFlaggedConVars(int) = 0;
	/*30*/	virtual void InstallCVarQuery(void*) = 0;
	/*31*/	virtual bool IsMaterialThreadSetAllowed(void)const = 0;
	/*32*/	virtual void QueueMaterialThreadSetValue(ConVar*, char const*) = 0;
	/*33*/	virtual void QueueMaterialThreadSetValue(ConVar*, int) = 0;
	/*34*/	virtual void QueueMaterialThreadSetValue(ConVar*, float) = 0;
	/*35*/	virtual bool HasQueuedMaterialThreadConVarSets(void)const = 0;
	/*36*/	virtual void ProcessQueuedMaterialThreadConVarSets(void) = 0;
	/*37*/	virtual void FactoryInternalIterator(void) = 0;
};


