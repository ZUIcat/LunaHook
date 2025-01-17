#include"texthook.h"
enum class regs
{
	_flags,
#ifndef _WIN64
	eax,
	ecx,
	edx,
	ebx,
	esp,
	ebp,
	esi,
	edi,
	flags,
#else
	rax,
	rbx,
	rcx,
	rdx,
	rsp,
	rbp,
	rsi,
	rdi,
	r8,
	r9,
	r10,
	r11,
	r12,
	r13,
	r14,
	r15,
#endif
	invalid
};

inline int get_stack(int s){
	#ifdef _WIN64
	return s*8;
	#else
	return s*4;
	#endif
}
inline int get_reg(regs reg){
	#ifdef _WIN64
	return -8*(int)reg-8;
	#else
	return -4-(int)reg*4;
	#endif
}


inline uintptr_t regof(regs reg,hook_stack* stack){
	switch (reg)
	{
	#ifndef _WIN64
	case regs::eax:return stack->eax;
	case regs::ecx:return stack->ecx;
	case regs::edx:return stack->edx;
	case regs::ebx:return stack->ebx;
	case regs::esp:return stack->esp;
	case regs::ebp:return stack->ebp;
	case regs::esi:return stack->esi;
	case regs::edi:return stack->edi;
	#else
	case regs::rax:return stack->rax;
	case regs::rbx:return stack->rbx;
	case regs::rcx:return stack->rcx;
	case regs::rdx:return stack->rdx;
	case regs::rsp:return stack->rsp;
	case regs::rbp:return stack->rbp;
	case regs::rsi:return stack->rsi;
	case regs::rdi:return stack->rdi;
	case regs::r8:return stack->r8;
	case regs::r9:return stack->r9;
	case regs::r10:return stack->r10;
	case regs::r11:return stack->r11;
	case regs::r12:return stack->r12;
	case regs::r13:return stack->r13;
	case regs::r14:return stack->r14;
	case regs::r15:return stack->r15;
	#endif
	}
  return 0;
}
