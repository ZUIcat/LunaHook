#include"LightVN.h"
namespace{
bool _1() {
  //void __fastcall sub_1404B7960(void **Src) 
  //HQ-1C*0@4B7960:LightApp.exe
  const BYTE BYTES[] = {
      0x90,
      XX4,
      XX4,
      0x48,0x8b,0xce,
      0xe8,XX4,
      0x90,
      0x48,0x8b,XX2,
      0x48,0x83,0xfa,0x08,
      0x72,0x36,
      0x48,0x8D,0x14,0x55,0x02,0x00,0x00,0x00,
      0x48,0x8b,XX2,
      0x48,0x8b,0xc1,
      0x48,0x81,0xFA,0x00,0x10,0x00,0x00,
      0x72,0x19,
      0x48,0x83,0xC2,0x27,
      0x48,0x8b,XX2,
      0x48,0x2b,0xc1,
      0x48,0x83,0xC0,0xF8,
      0x48,0x83,0xF8,0x1F ,
      0x0f,0x87,XX4,
      0xe8,XX4


  };
  auto suc=false;
  auto addrs = Util::SearchMemory(BYTES, sizeof(BYTES), PAGE_EXECUTE, processStartAddress, processStopAddress);
  for (auto addr : addrs) {
    ConsoleOutput("LightVN %p", addr);
    const BYTE aligned[] = { 0xCC,0xCC,0xCC,0xCC };
    addr = reverseFindBytes(aligned, sizeof(aligned), addr - 0x100, addr);
    if (addr == 0)continue;
    addr += 4;
    ConsoleOutput("LightVN %p", addr);
    HookParam hp;
    hp.address = addr;
    hp.type = CODEC_UTF16 | USING_STRING|DATA_INDIRECT;
    hp.index = 0;
    hp.offset=get_reg(regs::rcx);
    hp.filter_fun = [](void* data, size_t* len, HookParam* hp) {
      std::wstring s((wchar_t*)data, *len / 2);
      if (s.substr(s.size() - 2, 2) == L"\\w")
        *len -= 4;
      return true;
    };
    suc|=NewHook(hp, "LightVN");
  }
  return suc;
} 
bool _2(){
  //https://vndb.org/r86006
  //ファーストキス（体験版）
  //https://vndb.org/r85992
  //フサの大正女中ぐらし

  BYTE sig[]={
    0x48,XX,0xFE,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x7F,
0x48,0x3B,0xC3,
0x76,XX,
0x48,XX,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x7F,
  };
		auto addr=MemDbg::findBytes(sig, sizeof(sig), processStartAddress, processStopAddress);
		if(addr==0)return 0;
		addr=MemDbg::findEnclosingAlignedFunction(addr);
		if(addr==0)return 0;
		HookParam hp;
		hp.address = addr;
		hp.type = CODEC_UTF16|USING_STRING;
		hp.offset =get_stack(6);
    hp.filter_fun = [](void* data, size_t* len, HookParam* hp)
                    {
                      if(all_ascii((wchar_t*)data,*len))return false;
                      //高架下に広がる[瀟洒]<しょうしゃ>な店内には、あたしたちのような学生の他に、
                        auto str=std::wstring(reinterpret_cast<LPWSTR>(data),*len/2);
                        auto filterpath={
                            L".rpy",L".rpa",L".py",L".pyc",L".txt",
                            L".png",L".jpg",L".bmp",
                            L".mp3",L".ogg",
                            L".webm",L".mp4",
                            L".otf",L".ttf",L"Data/"
                        };
                        for(auto _ :filterpath)
                            if(str.find(_)!=str.npos)
                                return false;
                        str = std::regex_replace(str, std::wregex(L"\\[(.*?)\\]<(.*?)>"), L"$1");
                        wcscpy((wchar_t*)data,str.c_str());
                        *len=str.size()*2;
                        return true;
                    };
		return NewHook(hp, "LightVN2");
}
}

bool LightVN::attach_function() {
  bool ok=_1();
  ok=_2()||ok;
  return ok;
}