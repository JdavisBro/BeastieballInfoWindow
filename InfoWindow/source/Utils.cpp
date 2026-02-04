#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "ModuleMain.h"

#include "Utils.h"
namespace Utils
{

inline RValue CallBuiltin(TRoutine &function, std::vector<RValue> args)
{
    RValue return_value;
    function(return_value, nullptr, nullptr, static_cast<int>(args.size()), args.data());
    return return_value;
}
TRoutine variable_instance_exists;
TRoutine variable_instance_get;
TRoutine variable_instance_set;

TRoutine variable_global_exists;
TRoutine variable_global_get;
TRoutine variable_global_set;

TRoutine asset_get_index;
TRoutine instance_exists;
TRoutine instance_find;

void Setup()
{
    yytk->GetNamedRoutinePointer("variable_instance_exists", reinterpret_cast<PVOID *>(&variable_instance_exists));
    yytk->GetNamedRoutinePointer("variable_instance_get", reinterpret_cast<PVOID *>(&variable_instance_get));
    yytk->GetNamedRoutinePointer("variable_instance_set", reinterpret_cast<PVOID *>(&variable_instance_set));

    yytk->GetNamedRoutinePointer("variable_global_exists", reinterpret_cast<PVOID *>(&variable_global_exists));
    yytk->GetNamedRoutinePointer("variable_global_get", reinterpret_cast<PVOID *>(&variable_global_get));
    yytk->GetNamedRoutinePointer("variable_global_set", reinterpret_cast<PVOID *>(&variable_global_set));

    yytk->GetNamedRoutinePointer("asset_get_index", reinterpret_cast<PVOID *>(&asset_get_index));
    yytk->GetNamedRoutinePointer("instance_exists", reinterpret_cast<PVOID *>(&instance_exists));
    yytk->GetNamedRoutinePointer("instance_find", reinterpret_cast<PVOID *>(&instance_find));
}

RValue InstanceExists(const RValue &instance, const RValue &key)
{
    return CallBuiltin(variable_instance_exists, {instance, key});
}
RValue InstanceExists(const RValue &instance, const char *key)
{
    return CallBuiltin(variable_instance_exists, {instance, key});
}
RValue InstanceExists(const RValue &instance, const std::string_view key)
{
    return CallBuiltin(variable_instance_exists, {instance, RValue(key)});
}

RValue InstanceGet(const RValue &instance, const RValue &key)
{
    return CallBuiltin(variable_instance_get, {instance, key});
}
RValue InstanceGet(const RValue &instance, const char *key)
{
    return CallBuiltin(variable_instance_get, {instance, key});
}
RValue InstanceGet(const RValue &instance, const std::string_view key)
{
    return CallBuiltin(variable_instance_get, {instance, RValue(key)});
}

void InstanceSet(const RValue &instance, const RValue &key, const RValue &value)
{
    CallBuiltin(variable_instance_set, {instance, key, value});
}
void InstanceSet(const RValue &instance, const char *key, const RValue &value)
{
    CallBuiltin(variable_instance_set, {instance, key, value});
}
void InstanceSet(const RValue &instance, const std::string_view key, const RValue &value)
{
    CallBuiltin(variable_instance_set, {instance, RValue(key), value});
}

RValue GlobalExists(const RValue &key)
{
    return CallBuiltin(variable_global_exists, {key});
}
RValue GlobalExists(const char *key)
{
    return CallBuiltin(variable_global_exists, {key});
}
RValue GlobalExists(const std::string_view key)
{
    return CallBuiltin(variable_global_exists, {RValue(key)});
}

RValue GlobalGet(const RValue &key)
{
    return CallBuiltin(variable_global_get, {key});
}
RValue GlobalGet(const char *key)
{
    return CallBuiltin(variable_global_get, {key});
}
RValue GlobalGet(const std::string_view key)
{
    return CallBuiltin(variable_global_get, {RValue(key)});
}

void GlobalSet(const RValue &key, const RValue &value)
{
    CallBuiltin(variable_global_set, {key, value});
}
void GlobalSet(const char *key, const RValue &value)
{
    CallBuiltin(variable_global_set, {key, value});
}
void GlobalSet(const std::string_view key, const RValue &value)
{
    CallBuiltin(variable_global_set, {RValue(key), value});
}


RValue GetObjectInstance(const char *object_name)
{
    return CallBuiltin(instance_find, {CallBuiltin(asset_get_index, {object_name}), 0});
}

RValue GetObjectInstance(const char *object_name, int index)
{
    return CallBuiltin(instance_find, {CallBuiltin(asset_get_index, {object_name}), index});
}

bool ObjectInstanceExists(const char *object_name)
{
    return CallBuiltin(instance_exists, {CallBuiltin(asset_get_index, {object_name}), 0}).ToBoolean();
}

}
