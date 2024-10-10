#include "Filenames.hpp"

#include <chrono>

namespace utils {

std::string make_formatted_time_for_filename() {
    auto tzconv = std::chrono::current_zone();
    auto const time = tzconv->to_local(std::chrono::system_clock::now());
    return std::format("{0:%Y%m%d_%H%M%S}", time);
}

}  // namespace utils
