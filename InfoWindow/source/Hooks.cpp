#include <YYToolkit/YYTK_Shared.hpp>

#include "Hooks.h"
#include "ModuleMain.h"

using namespace Aurie;
using namespace YYTK;

struct HookRequest
{
  const char *pre;
  const char *post;
  const char *HookId;
  PVOID HookFunction;
  PVOID *Trampoline;
};

std::vector<HookRequest> requested_hooks;

void CreateHooks()
{
  if (!requested_hooks.size())
  {
    return;
  }

  AurieStatus last_status = AURIE_SUCCESS;
  CScript *script = nullptr;
  int i = 0;
  while (true)
  {
    last_status = yytk->GetScriptData(i, script);
    if (last_status != AURIE_SUCCESS)
    {
      if (last_status != AURIE_OBJECT_NOT_FOUND)
      {
        DbgPrintEx(LOG_SEVERITY_WARNING, AurieStatusToString(last_status));
      }
      break;
    }
    std::string script_name = script->GetName();
    size_t hook_count = requested_hooks.size();
    for (size_t hook_i = 0; hook_i < hook_count; hook_i++)
    {
      HookRequest request = requested_hooks[hook_i];
      if (script_name.ends_with(request.post) && (request.pre == NULL || script_name.starts_with(request.pre)))
      {
        std::string combined_name = std::string(request.pre == NULL ? "" : request.pre) + request.post;
        last_status = MmCreateHook(
          g_ArSelfModule,
          request.HookId,
          script->m_Functions->m_ScriptFunction,
          request.HookFunction,
          request.Trampoline);
        if (!AurieSuccess(last_status))
        {
          DbgPrintEx(LOG_SEVERITY_WARNING, "Failed to create hook for %s", combined_name.c_str());
        }
        else
        {
          DbgPrintEx(LOG_SEVERITY_INFO, "Hook created for %s!", combined_name.c_str());
        }
        requested_hooks.erase(requested_hooks.begin() + hook_i);
        break;
      }
    }
    i++;
  }
  if (!requested_hooks.empty())
  {
    for (HookRequest request : requested_hooks)
    {
      DbgPrintEx(LOG_SEVERITY_WARNING, "Unable to find script for %s%s", request.pre == NULL ? "" : request.pre, request.post);
    }
  }
}

void RequestHook(const char *pre, const char *post, const char *HookId, PVOID HookFunction, PVOID *Trampoline)
{
  requested_hooks.push_back({pre, post, HookId, HookFunction, Trampoline});
}

void BuiltinHook(const char *HookId, const char *FnName, PVOID HookFunction, PVOID *Trampoline)
{
  PVOID function = nullptr;
  yytk->GetNamedRoutinePointer(FnName, &function);
  AurieStatus last_status = MmCreateHook(
    g_ArSelfModule,
    HookId,
    function,
    HookFunction,
    Trampoline);
  if (!AurieSuccess(last_status))
    DbgPrintEx(LOG_SEVERITY_WARNING, "Failed to create hook for %s", FnName);
  else
    DbgPrintEx(LOG_SEVERITY_INFO, "Hook created for %s!", FnName);
}
