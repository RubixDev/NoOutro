#define PICO_BUILD
#include "unzipLIB.h"
#include <functional>
#include <string>

int unzip_file(
    std::string zip_path,
    std::string output_dir,
    std::function<bool(std::string path)> for_each_file = nullptr
);

int unzip_dsiware(std::string zip_path, std::string roms_path, std::string orig_filename);
