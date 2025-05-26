#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <filesystem>

HRESULT open_file_dialog(std::filesystem::path& out_path);