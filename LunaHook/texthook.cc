// texthook.cc
// 8/24/2013 jichi
// Branch: LUNA_HOOK_DLL/texthook.cpp, rev 128
// 8/24/2013 TODO: Clean up this file
#include"embed_util.h"
#include "texthook.h"
#include "main.h"
#include "ithsys/ithsys.h"
#include "MinHook.h"
#include"Lang/Lang.h"
extern WinMutex viewMutex;

// - Unnamed helpers -

namespace { // unnamed
#ifndef _WIN64
	BYTE common_hook[] = {
		0x9c, // pushfd
		0x60, // pushad
		0x9c, // pushfd ; Artikash 11/4/2018: not sure why pushfd happens twice. Anyway, after this a total of 0x28 bytes are pushed
		0x8d, 0x44, 0x24, 0x28, // lea eax,[esp+0x28]
		0x50, // push eax ; lpDatabase
		0xb9, 0,0,0,0, // mov ecx,@this
		0xbb, 0,0,0,0, // mov ebx,@TextHook::Send
		0xff, 0xd3, // call ebx
		0x9d, // popfd
		0x61, // popad
		0x9d, // popfd
		0x68, 0,0,0,0, // push @original
		0xc3  // ret ; basically absolute jmp to @original
	};
	int this_offset = 9, send_offset = 14, original_offset = 24;
#else
	BYTE common_hook[] = {
		0x9c, // push rflags
		0x50, // push rax
		0x53, // push rbx
		0x51, // push rcx
		0x52, // push rdx
		0x54, // push rsp
		0x55, // push rbp
		0x56, // push rsi
		0x57, // push rdi
		0x41, 0x50, // push r8
		0x41, 0x51, // push r9
		0x41, 0x52, // push r10
		0x41, 0x53, // push r11
		0x41, 0x54, // push r12
		0x41, 0x55, // push r13
		0x41, 0x56, // push r14
		0x41, 0x57, // push r15
		// https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention
		// https://stackoverflow.com/questions/43358429/save-value-of-xmm-registers
		0x48, 0x83, 0xec, 0x20, // sub rsp,0x20
		0xf3, 0x0f, 0x7f, 0x24, 0x24, // movdqu [rsp],xmm4
		0xf3, 0x0f, 0x7f, 0x6c, 0x24, 0x10, // movdqu [rsp+0x10],xmm5
		0x48, 0x8d, 0x94, 0x24, 0xa8, 0x00, 0x00, 0x00, // lea rdx,[rsp+0xa8]
		0x48, 0xb9, 0,0,0,0,0,0,0,0, // mov rcx,@this
		0x48, 0xb8, 0,0,0,0,0,0,0,0, // mov rax,@TextHook::Send
		0x48, 0x89, 0xe3, // mov rbx,rsp
		0x48, 0x83, 0xe4, 0xf0, // and rsp,0xfffffffffffffff0 ; align stack
		0xff, 0xd0, // call rax
		0x48, 0x89, 0xdc, // mov rsp,rbx
		0xf3, 0x0f, 0x6f, 0x6c, 0x24, 0x10, // movdqu xmm5,XMMWORD PTR[rsp + 0x10]
		0xf3, 0x0f, 0x6f, 0x24, 0x24, // movdqu xmm4,XMMWORD PTR[rsp]
		0x48, 0x83, 0xc4, 0x20, // add rsp,0x20
		0x41, 0x5f, // pop r15
		0x41, 0x5e, // pop r14
		0x41, 0x5d, // pop r13
		0x41, 0x5c, // pop r12
		0x41, 0x5b, // pop r11
		0x41, 0x5a, // pop r10
		0x41, 0x59, // pop r9
		0x41, 0x58, // pop r8
		0x5f, // pop rdi
		0x5e, // pop rsi
		0x5d, // pop rbp
		0x5c, // pop rsp
		0x5a, // pop rdx
		0x59, // pop rcx
		0x5b, // pop rbx
		0x58, // pop rax
		0x9d, // pop rflags
		0xff, 0x25, 0x00, 0x00, 0x00, 0x00, // jmp qword ptr [rip]
		0,0,0,0,0,0,0,0 // @original
	};
	int this_offset = 50, send_offset = 60, original_offset = 126;
#endif
	
	//thread_local BYTE buffer[PIPE_BUFFER_SIZE];
	//thread_local will crush on windowsxp
	enum { TEXT_BUFFER_SIZE = PIPE_BUFFER_SIZE - sizeof(TextOutput_T) };
} // unnamed namespace

// - TextHook methods -

bool TextHook::Insert(HookParam hp)
{
	local_buffer=new BYTE[PIPE_BUFFER_SIZE];
	{
		std::scoped_lock lock(viewMutex);
		this->hp = hp;
		address = hp.address;
	}
	if (hp.type & DIRECT_READ) return InsertReadCode();
	return InsertHookCode();
}

void TextHook::Send(uintptr_t lpDataBase)
{
	auto buffer =(TextOutput_T*) local_buffer;
	auto pbData = buffer->data;
	_InterlockedIncrement((long*)&useCount);
	__try
	{
		auto stack=(hook_stack*)(lpDataBase-sizeof(hook_stack)+sizeof(uintptr_t));
		
		#ifndef _WIN64
		if (auto current_trigger_fun = trigger_fun.exchange(nullptr))
			if (!current_trigger_fun(location, stack->ebp, stack->esp)) trigger_fun = current_trigger_fun;
		#endif
		
		size_t lpCount = 0;
		uintptr_t lpSplit = 0,
			lpRetn = stack->retaddr,
			plpdatain=(lpDataBase + hp.offset),
			lpDataIn=*(uintptr_t*)plpdatain;
		
		buffer->type=hp.type;
		bool isstring=false;
		if((hp.type&EMBED_ABLE)&&!(hp.type&EMBED_BEFORE_SIMPLE) )
		{
			isstring=true;
			lpRetn=0;
			lpSplit=Engine::ScenarioRole;
			if(hp.hook_before(stack,pbData,&lpCount,&lpSplit)==false)__leave;
			if (hp.filter_fun && !hp.filter_fun(pbData, &lpCount, &hp) || lpCount <= 0) __leave;
			
		}
		else
		{
			// jichi 10/24/2014: generic hook function
			if (hp.hook_fun && !hp.hook_fun(stack, &hp)) hp.hook_fun = nullptr;

			if (hp.type & HOOK_EMPTY) __leave; // jichi 10/24/2014: dummy hook only for dynamic hook

			if (hp.text_fun) {
				isstring=true;
				hp.text_fun(stack, &hp, &lpDataIn, &lpSplit, &lpCount);
			}
			else {
				if (hp.type & FIXING_SPLIT) lpSplit = FIXED_SPLIT_VALUE; // fuse all threads, and prevent floating
				else if (hp.type & USING_SPLIT) {
					lpSplit = *(uintptr_t *)(lpDataBase + hp.split);
					if (hp.type & SPLIT_INDIRECT) lpSplit = *(uintptr_t *)(lpSplit + hp.split_index);
				}
				if (hp.type & DATA_INDIRECT) {
					plpdatain=(lpDataIn + hp.index);
					lpDataIn = *(uintptr_t *)plpdatain;
				}
				lpDataIn += hp.padding;
				lpCount = GetLength(stack, lpDataIn);
			}
			
			//hook_fun&&text_fun change hookparam.type
			buffer->type=hp.type;
			
			if (lpCount <= 0) __leave;
			if (lpCount > TEXT_BUFFER_SIZE) lpCount = TEXT_BUFFER_SIZE;
			if ((!(hp.type&USING_CHAR))&&(isstring||(hp.type&USING_STRING))) 
			{
				if(lpDataIn == 0)__leave;
				::memcpy(pbData, (void*)lpDataIn, lpCount);
			}
			else{
				if(hp.type &CODEC_UTF32)
				{
					*(uint32_t*)pbData=lpDataIn&0xffffffff;
				}
				else
				{//CHAR_LITTEL_ENDIAN,CODEC_ANSI_BE,CODEC_UTF16
					lpDataIn &= 0xffff;
					if ((hp.type & CODEC_ANSI_BE) && (lpDataIn >> 8)) lpDataIn = _byteswap_ushort(lpDataIn & 0xffff);
					if (lpCount == 1) lpDataIn &= 0xff;
					*(WORD*)pbData = lpDataIn & 0xffff;
				}
			} 

			if (hp.filter_fun && !hp.filter_fun(pbData, &lpCount, &hp) || lpCount <= 0) __leave;

			if (hp.type & (NO_CONTEXT | FIXING_SPLIT)) lpRetn = 0;
		}
		
		ThreadParam tp{ GetCurrentProcessId(), address, lpRetn, lpSplit };
		if((hp.type&EMBED_ABLE)&&(check_embed_able(tp)))
		{
			auto lpCountsave=lpCount;
			if(waitfornotify(buffer,pbData,&lpCount,tp))
			{
				if(hp.type&EMBED_AFTER_NEW)
				{
					auto _ = new char[max(lpCountsave,lpCount)+10];
					memcpy(_,pbData,lpCount);
					for(int i=lpCount;i<max(lpCountsave,lpCount)+10;i++)
						_[i]=0;
					*(uintptr_t*)plpdatain=(uintptr_t)_;
				}
				else if(hp.type&EMBED_AFTER_OVERWRITE)
				{
					memcpy((void*)lpDataIn,pbData,lpCount);
					for(int i=lpCount;i<lpCountsave;i++)
						((BYTE*)(lpDataIn))[i]=0;
				}
				else
					hp.hook_after(stack,pbData,lpCount);
			}
		}
		else
		{
			TextOutput(tp, buffer, lpCount);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		if (!err && !(hp.type & KNOWN_UNSTABLE))
		{
			err = true;
			ConsoleOutput(SEND_ERROR, hp.name);
		}
	}

	_InterlockedDecrement((long*) & useCount);
}

bool TextHook::InsertHookCode()
{
	// jichi 9/17/2013: might raise 0xC0000005 AccessViolationException on win7
	// Artikash 10/30/2018: No, I think that's impossible now that I moved to minhook
	if (hp.type & MODULE_OFFSET)  // Map hook offset to real address
		if (hp.type & FUNCTION_OFFSET)
			if (FARPROC function = GetProcAddress(GetModuleHandleW(hp.module), hp.function)) address += (uint64_t)function;
			else return ConsoleOutput(FUNC_MISSING), false;
		else if (HMODULE moduleBase = GetModuleHandleW(hp.module)) address += (uint64_t)moduleBase;
		else return ConsoleOutput(MODULE_MISSING), false;

	VirtualProtect(location, 10, PAGE_EXECUTE_READWRITE, DUMMY);
	void* original;
	MH_STATUS error;
	while ((error = MH_CreateHook(location, trampoline, &original)) != MH_OK)
		if (error == MH_ERROR_ALREADY_CREATED) RemoveHook(address);
		else return ConsoleOutput(MH_StatusToString(error)), false;

	*(TextHook**)(common_hook + this_offset) = this;
	*(void(TextHook::**)(uintptr_t))(common_hook + send_offset) = &TextHook::Send;
	*(void**)(common_hook + original_offset) = original;
	memcpy(trampoline, common_hook, sizeof(common_hook));
	return MH_EnableHook(location) == MH_OK;
}

void TextHook::Read()
{
	size_t dataLen = 1;
	//BYTE(*buffer)[PIPE_BUFFER_SIZE] = &::buffer, *pbData = *buffer + sizeof(ThreadParam);
	 
	auto buffer =(TextOutput_T*) local_buffer;
	auto pbData = buffer->data;
	buffer->type=hp.type;
	__try
	{
		while (WaitForSingleObject(readerEvent, 500) == WAIT_TIMEOUT) if (location&&(memcmp(pbData, location, dataLen) != 0)) if (int currentLen = HookStrlen((BYTE*)location))
		{
			dataLen = min(currentLen, TEXT_BUFFER_SIZE);
			memcpy(pbData, location, dataLen);
			if (hp.filter_fun && !hp.filter_fun(pbData, &dataLen, &hp) || dataLen <= 0) continue;
			TextOutput({ GetCurrentProcessId(), address, 0, 0 }, buffer, dataLen);
			memcpy(pbData, location, dataLen);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ConsoleOutput(READ_ERROR, hp.name);
		Clear();
	}
}

bool TextHook::InsertReadCode()
{
	readerThread = CreateThread(nullptr, 0, [](void* This) { ((TextHook*)This)->Read(); return 0UL; }, this, 0, nullptr);
	readerEvent = CreateEventW(nullptr, FALSE, FALSE, NULL);
	return true;
}

void TextHook::RemoveHookCode()
{
	MH_DisableHook(location);
	while (useCount != 0);
	MH_RemoveHook(location);
}

void TextHook::RemoveReadCode()
{
	SetEvent(readerEvent);
	if (GetThreadId(readerThread) != GetCurrentThreadId()) WaitForSingleObject(readerThread, 1000);
	CloseHandle(readerEvent);
	CloseHandle(readerThread);
}

void TextHook::Clear()
{
	if (address == 0) return;
	if (hp.type & DIRECT_READ) RemoveReadCode();
	else RemoveHookCode();
	NotifyHookRemove(address, hp.name);
	std::scoped_lock lock(viewMutex);
	memset(&hp, 0, sizeof(HookParam));
	address = 0;
	if(local_buffer)delete []local_buffer;
}

int TextHook::GetLength(hook_stack* stack, uintptr_t in)
{
	int len;
	if(hp.type&USING_STRING)
	{
		if(hp.length_offset)
		{
			len = *((uintptr_t*)stack->base + hp.length_offset);
			if (len >= 0) 
			{
				if (hp.type & CODEC_UTF16)
					len <<= 1;
				else if(hp.type & CODEC_UTF32)
					len <<= 2;
				
			}
			else if (len != -1)
			{

			}
			else
			{//len==-1
				len = HookStrlen((BYTE*)in);
			}
		}
		else
		{
			len = HookStrlen((BYTE*)in);
		}
	}
	else
	{
		if (hp.type & CODEC_UTF16)
			len = 2;
		else if(hp.type&CODEC_UTF32)
			len = 4;
		else 
		{	//CODEC_ANSI_BE,CHAR_LITTLE_ENDIAN
			if (hp.type & CODEC_ANSI_BE)
				in >>= 8;
			len = !!IsDBCSLeadByteEx(hp.codepage, in & 0xff) + 1;
		} 
	}
	return max(0, len);
}

int TextHook::HookStrlen(BYTE* data)
{
	if(data==0)return 0;

	if(hp.type&CODEC_UTF16)
		return wcslen((wchar_t*)data)*2;
	else if(hp.type&CODEC_UTF32)
		return u32strlen((uint32_t*)data)*4;
	else
		return strlen((char*)data);

}

// EOF
