#pragma once
#include "json.hpp"

using nlohmann::json;

namespace Storage
{

extern json data;

void LoadFile();

extern bool needs_save;
extern bool save_disbled;

void SaveFile();

void ResetToDefault();

extern bool reading;
extern const char *category;

void Store(const char *key, json *value);
void Store(const char *key, bool *value);
void Store(const char *key, double *value);
void Store(const char *key, std::string *value);

}