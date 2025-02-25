#include"TinkerBell.h"
bool InsertTinkerBellHook()
{
  //DWORD s1,s2,i;
  //DWORD ch=0x8141;
  DWORD i;
  WORD count;
  count = 0;
  HookParam hp;
  hp.type = CODEC_ANSI_BE|NO_CONTEXT;
  for (i = processStartAddress; i< processStopAddress - 4; i++) {
    if (*(DWORD*)i == 0x8141) {
      BYTE t = *(BYTE*)(i - 1);
      if (t == 0x3d || t == 0x2d) {
        hp.offset=get_reg(regs::eax);
        hp.address = i - 1;
      } else if (*(BYTE*)(i-2) == 0x81) {
        t &= 0xf8;
        if (t == 0xf8 || t == 0xe8) {
          hp.offset = -8 - ((*(BYTE*)(i-1) & 7) << 2);
          hp.address = i - 2;
        }
      }
      if (hp.address) {
        char hook_name[0x20];
        ::strcpy(hook_name, "TinkerBell"); // size = 0xa
        hook_name[0xa] = '0' + count;
        hook_name[0xb] = 0;
        ConsoleOutput("INSERT TinkerBell");
        count+=NewHook(hp, hook_name);
        hp.address = 0;
      }
    }
  }
  if (count) return true;
  ConsoleOutput("TinkerBell: failed");
  return false;
}

//  s1=SearchPattern(processStartAddress,processStopAddress-processStartAddress-4,&ch,4);
//  if (s1)
//  {
//    for (i=s1;i>s1-0x400;i--)
//    {
//      if (*(WORD*)(processStartAddress+i)==0xec83)
//      {
//        hp.address=processStartAddress+i;
//        NewHook(hp, "C.System");
//        break;
//      }
//    }
//  }
//  s2=s1+SearchPattern(processStartAddress+s1+4,processStopAddress-s1-8,&ch,4);
//  if (s2)
//  {
//    for (i=s2;i>s2-0x400;i--)
//    {
//      if (*(WORD*)(processStartAddress+i)==0xec83)
//      {
//        hp.address=processStartAddress+i;
//        NewHook(hp, "TinkerBell");
//        break;
//      }
//    }
//  }
//  //if (count)
  //RegisterEngineType(ENGINE_TINKER);
namespace{ 
bool WendyBell_filter(void* data, size_t* len, HookParam* hp){ 
 
   auto wc=std::wstring(reinterpret_cast<LPWSTR>(data),*len/2);
 
  for(int i=0;i<wc.size()-1;i++){
    if(wc[i]==L'\xff' && wc[i+1]==L'\xff'){   
      wc=wc.substr(0,i)+wc.substr(i+3+wc[i+2]); 
      
    }
  } 
  for(int i=0;i<wc.size();i++){
    if(wc[i]<10){   
      wc=wc.substr(0,i)+wc.substr(i+1); 
      
    }
  } 
  *len=wc.size()*2; 
  wcscpy(reinterpret_cast<LPWSTR>(data),wc.c_str()); 
  return true;
}
}

namespace{
std::wstring last=L"";
bool tkbl_filter(void* data, size_t* len, HookParam* hp){ 
  StringFilter(reinterpret_cast<LPWSTR>(data), reinterpret_cast<size_t *>(len), L"\x26bc\x65\x25\xffff", 4);//移除心形
   
  WendyBell_filter(data,len,hp);
  auto str=std::wstring(reinterpret_cast<LPWSTR>(data),*len/2 -1);  //末尾存在一个换行符
  
  if(last==str)return false;
  last=str;  
   
  *len=str.size()*2;
  wcscpy(reinterpret_cast<LPWSTR>(data),str.c_str());
  return true;
}
bool tkbl(){
  // せをはやみ
  const BYTE bytes[] = {
    0x55,0x8b,0xec,
    0x83,0xec,0x0c,
    0x53,0x56,
    0x8b,0xf1,
    0x8b,0x5e,0x10,
    0x8b,0x4e,0x14,
    0x89,0x5d,0xf4,
    0x89,0x4d,0xfc,
    0x3b,0xd9
  };  
  ULONG addr = MemDbg::findBytes(bytes, sizeof(bytes), processStartAddress, processStopAddress);
  if (!addr)  return false; 

  HookParam hp;
  hp.type = USING_STRING|CODEC_UTF16|NO_CONTEXT;  
  hp.address = addr; 
  hp.filter_fun=tkbl_filter;
  hp.text_fun = [](auto, HookParam *hp, uintptr_t* data, uintptr_t* split, size_t* count)
	{
    auto str=reinterpret_cast<wchar_t*>(*data);
		*count = wcslen(str)*2 ;
		*split = (wcschr(str ,0x3010) != nullptr)&&(wcschr(str, 0x3011) != nullptr);
	};
  hp.offset=get_reg(regs::ebx);
  return NewHook(hp, "tkbl");
}
} 

bool InsertWendyBellHook() {
  const BYTE bytes[] = {
    
    0x83,0xbe,XX4,0x00,
    0x8b,XX2,
    0x0f,0x85,XX4,
    0x83,0xbe,XX4,0x00,
    0x0f,0x85,XX4,
    0x83,0xbe,XX4,0x00,
    0x0f,0x84,XX4
/*.always:0048E4CA 83 BE F8 04 00 00 00          cmp     dword ptr[esi + 4F8h], 0
.always : 0048E4D1 8B 5D 84                      mov     ebx,[ebp + Src]
.always : 0048E4D4 0F 85 86 F8 FF FF             jnz     loc_48DD60
.always : 0048E4D4
.always : 0048E4DA 83 BE F4 04 00 00 00          cmp     dword ptr[esi + 4F4h], 0
.always : 0048E4E1 0F 85 79 F8 FF FF             jnz     loc_48DD60
.always : 0048E4E1
.always : 0048E4E7 83 BE 00 05 00 00 00          cmp     dword ptr[esi + 500h], 0
.always : 0048E4EE 0F 84 6C F8 FF FF             jz      loc_48DD60*/

  };
  const BYTE bytes2[] = {
    //夢幻のさくら ~緋艶姫淫辱孕蝕譚~
    //妖花の園
    0x8b,0x86,XX4,
    0x6a,0x00,
    0x8b,0x80,XX4,
    0x50,
    0x8b,0x08,
    0xff,0x91,XX4,
    0x8b,0x45,XX,
    0x83,0xF8,0x08
    //
//.always:0048E51D 8B 86 58 0A 00 00             mov     eax,[esi + 0A58h]
//.always : 0048E523 6A 00                         push    0
//.always : 0048E525 8B 80 B8 01 00 00             mov     eax,[eax + 1B8h]
//.always : 0048E52B 50                            push    eax
//.always : 0048E52C 8B 08                         mov     ecx,[eax]
//.always:0048E52E FF 91 C4 00 00 00             call    dword ptr[ecx + 0C4h]
//.always : 0048E52E
//.always : 0048E534 8B 45 DC                      mov     eax,[ebp + var_24]
//.always : 0048E537 83 F8 08                      cmp     eax, 8
  };
  
  auto addrs = Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, processStartAddress, processStopAddress);
  auto addrs2 = Util::SearchMemory(bytes2, sizeof(bytes2), PAGE_EXECUTE, processStartAddress, processStopAddress);
  addrs.insert(addrs.end(), addrs2.begin(), addrs2.end());
  auto succ=false;
  for (auto addr : addrs) { 
    HookParam hp;
    hp.address = addr;
    hp.offset=get_reg(regs::ebx);
    hp.filter_fun=WendyBell_filter;
    hp.type = USING_STRING | CODEC_UTF16 | NO_CONTEXT;
    ConsoleOutput("%p",addr);
    succ|=NewHook(hp, "WendyBell");
  }
    
  
  return succ;
}

namespace{
bool _2() {
  
  const BYTE bytes[] = {
   //夢幻のさくら2
     0x55,0x8b,0xec,
     0x53,
     0x8b,0x5d,0x08,
     0x56,0x8b,0xf1,
     0x57,
     0x8b,0x4e,0x10,
     0x8b,0xc1,
     0xf7,0xd0,
     0x3b,0xc3
  };
  auto addrs = Util::SearchMemory(bytes, sizeof(bytes), PAGE_EXECUTE, processStartAddress, processStopAddress);
  auto succ=false;
  for (auto addr : addrs) { 
    HookParam hp;
    hp.address = addr;
    hp.offset=get_stack(2); 
    hp.type = CODEC_UTF16;
    succ|=NewHook(hp, "TinkerBell");
  } 
  return succ;
}
}
bool TinkerBell::attach_function() { 
    return InsertTinkerBellHook()||tkbl()||InsertWendyBellHook()||_2();
} 
bool TinkerBellold::attach_function(){
  HookParam hp;
  hp.address =(DWORD) ExtTextOutA;
  
  hp.offset =get_stack(6);
  hp.type = USING_STRING|USING_SPLIT;
  hp.split=get_stack(5);
  return NewHook(hp, "TinkerBell");
}
