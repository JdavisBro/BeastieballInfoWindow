#pragma once

#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

void CreateHooks();

void RequestHook(const char *pre, const char *post, const char *HookId, PVOID HookFunction, PVOID *Trampoline);
