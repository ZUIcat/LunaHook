#ifndef __LUNA_EMBED_ENGINE_H
#define __LUNA_EMBED_ENGINE_H
#include"types.h"
#include "texthook.h"
#include"dyncodec/dynsjiscodec.h"

extern EmbedSharedMem *embedsharedmem;
extern DynamicShiftJISCodec *dynamiccodec ;

namespace WinKey {
    inline bool isKeyPressed(int vk) { return ::GetKeyState(vk) & 0xf0; }
    inline bool isKeyToggled(int vk) { return ::GetKeyState(vk) & 0x0f; }

    inline bool isKeyReturnPressed() { return isKeyPressed(VK_RETURN); }
    inline bool isKeyControlPressed() { return isKeyPressed(VK_CONTROL); }
    inline bool isKeyShiftPressed() { return isKeyPressed(VK_SHIFT); }
    inline bool isKeyAltPressed() { return isKeyPressed(VK_MENU); }
 }
namespace Engine{
    enum TextRole { UnknownRole = 0, ScenarioRole,  NameRole, OtherRole,
                ChoiceRole = OtherRole, HistoryRole = OtherRole,
                RoleCount };
}
inline std::atomic<void(*)()> patch_fun = nullptr;
void ReplaceFunction(PVOID* oldf,PVOID newf);
bool check_embed_able(const ThreadParam& tp);
#endif