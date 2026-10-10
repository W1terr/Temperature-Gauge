#pragma once

// windows.h ends up in here through the libraries; keep its macros from breaking std::min/max and RE:: names
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <numbers>
#include <optional>
#include <span>
#include <unordered_map>
#include <string>
#include <vector>

namespace logs = SKSE::log;
using json = nlohmann::json;
using namespace std::literals;

#undef GetObject
#undef PlaySound
