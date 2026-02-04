#pragma once
#include <YYToolkit/YYTK_Shared.hpp>
using namespace YYTK;

namespace Utils
{

void Setup();

bool InstanceExists(const RValue &instance, const RValue &key);
bool InstanceExists(const RValue &instance, const char *key);
bool InstanceExists(const RValue &instance, const std::string_view key);

RValue InstanceGet(const RValue &instance, const RValue &key);
RValue InstanceGet(const RValue &instance, const char *key);
RValue InstanceGet(const RValue &instance, const std::string_view key);

void InstanceSet(const RValue &instance, const RValue &key, const RValue &value);
void InstanceSet(const RValue &instance, const char *key, const RValue &value);
void InstanceSet(const RValue &instance, const std::string_view key, const RValue &value);

bool GlobalExists(const RValue &key);
bool GlobalExists(const char *key);
bool GlobalExists(const std::string_view key);

RValue GlobalGet(const RValue &key);
RValue GlobalGet(const char *key);
RValue GlobalGet(const std::string_view key);

void GlobalSet(const RValue &key, const RValue &value);
void GlobalSet(const char *key, const RValue &value);
void GlobalSet(const std::string_view key, const RValue &value);

RValue GetObjectInstance(const char *object_name);
RValue GetObjectInstance(const char *object_name, int index);

bool ObjectInstanceExists(const char *object_name);

}