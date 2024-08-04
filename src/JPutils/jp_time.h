#pragma once

#include <filesystem>

using namespace std;

time_t to_time_t(filesystem::file_time_type fileTime);
