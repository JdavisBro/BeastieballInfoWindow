#include <fstream>
#include "json.hpp"

#include "Storage.h"

using nlohmann::json;

namespace Storage
{

extern json data = {};

std::filesystem::path path = "mod_data/InfoWindow.json";

void LoadFile()
{
    if (!std::filesystem::is_regular_file(path))
        return;
    std::fstream file;
    file.open(path);
    data = json::parse(file, nullptr, false);
}

extern bool needs_save = false;
extern bool save_disbled = true;

void SaveFile()
{
    if (!needs_save || save_disbled)
        return;
    std::fstream file;
    file.open(path, std::fstream::out);
    std::string out_str = data.dump(2).c_str();
    file.write(out_str.c_str(), out_str.size());
    needs_save = false;
}

json defaults = {};
bool defaults_complete = false;

void ResetToDefault()
{
    defaults_complete = true;
    data = defaults;
    needs_save = true;
    SaveFile();
}

extern bool reading = true;
extern const char *category = "general";

void WriteDefault(const char *key, json value)
{
    if (defaults_complete) return;
    if (!defaults[category].is_object())
        defaults[category] = {};
    if (defaults[category][key] != value) {
        defaults[category][key] = value;
    }
}

void Write(const char *key, json value)
{
    if (!data[category].is_object())
        data[category] = {};
    if (data[category][key] != value) {
        data[category][key] = value;
        needs_save = true;
    }
}

json Read(const char *key)
{
    return data[category][key];
}

json Store(const char *key, json value)
{
    if (!reading) {
        Write(key, value);
        return value;
    }
    WriteDefault(key, value);
    return Read(key);
}

void Store(const char *key, bool *value)
{
    if (!reading) {
        Write(key, *value);
        return;
    }
    WriteDefault(key, *value);
    json value_json = Read(key);
    if (!value_json.is_boolean())
        return;
    *value = (bool)value_json;
}

void Store(const char *key, double *value)
{
    if (!reading) {
        Write(key, *value);
        return;
    }
    WriteDefault(key, *value);
    json value_json = Read(key);
    if (!value_json.is_number())
        return;
    *value = (double)value_json;
}

void Store(const char *key, std::string *value)
{
    if (!reading) {
        Write(key, *value);
        return;
    }
    WriteDefault(key, *value);
    json value_json = Read(key);
    if (!value_json.is_string())
        return;
    *value = (std::string)value_json;
}

}