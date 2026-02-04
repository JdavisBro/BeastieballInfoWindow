#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "ModuleMain.h"

#include "Utils.h"

namespace Utils
{

RValue InstanceExists(const RValue &instance, const RValue &key)
{
    return yytk->CallBuiltin("variable_instance_exists", {instance, key});
}
RValue InstanceExists(const RValue &instance, const char *key)
{
    return yytk->CallBuiltin("variable_instance_exists", {instance, key});
}
RValue InstanceExists(const RValue &instance, const std::string_view key)
{
    return yytk->CallBuiltin("variable_instance_exists", {instance, RValue(key)});
}

RValue InstanceGet(const RValue &instance, const RValue &key)
{
    return yytk->CallBuiltin("variable_instance_get", {instance, key});
}
RValue InstanceGet(const RValue &instance, const char *key)
{
    return yytk->CallBuiltin("variable_instance_get", {instance, key});
}
RValue InstanceGet(const RValue &instance, const std::string_view key)
{
    return yytk->CallBuiltin("variable_instance_get", {instance, RValue(key)});
}

void InstanceSet(const RValue &instance, const RValue &key, const RValue &value)
{
    yytk->CallBuiltin("variable_instance_set", {instance, key, value});
}
void InstanceSet(const RValue &instance, const char *key, const RValue &value)
{
    yytk->CallBuiltin("variable_instance_set", {instance, key, value});
}
void InstanceSet(const RValue &instance, const std::string_view key, const RValue &value)
{
    yytk->CallBuiltin("variable_instance_set", {instance, RValue(key), value});
}

RValue GlobalExists(const RValue &key)
{
    return yytk->CallBuiltin("variable_global_exists", {key});
}
RValue GlobalExists(const char *key)
{
    return yytk->CallBuiltin("variable_global_exists", {key});
}
RValue GlobalExists(const std::string_view key)
{
    return yytk->CallBuiltin("variable_global_exists", {RValue(key)});
}

RValue GlobalGet(const RValue &key)
{
    return yytk->CallBuiltin("variable_global_get", {key});
}
RValue GlobalGet(const char *key)
{
    return yytk->CallBuiltin("variable_global_get", {key});
}
RValue GlobalGet(const std::string_view key)
{
    return yytk->CallBuiltin("variable_global_get", {RValue(key)});
}

void GlobalSet(const RValue &key, const RValue &value)
{
    yytk->CallBuiltin("variable_global_set", {key, value});
}
void GlobalSet(const char *key, const RValue &value)
{
    yytk->CallBuiltin("variable_global_set", {key, value});
}
void GlobalSet(const std::string_view key, const RValue &value)
{
    yytk->CallBuiltin("variable_global_set", {RValue(key), value});
}


RValue GetObjectInstance(const char *object_name)
{
    return yytk->CallBuiltin("instance_find", {yytk->CallBuiltin("asset_get_index", {object_name}), 0});
}

RValue GetObjectInstance(const char *object_name, int index)
{
    return yytk->CallBuiltin("instance_find", {yytk->CallBuiltin("asset_get_index", {object_name}), index});
}

bool ObjectInstanceExists(const char *object_name)
{
    return yytk->CallBuiltin("instance_exists", {yytk->CallBuiltin("asset_get_index", {object_name}), 0}).ToBoolean();
}

}
