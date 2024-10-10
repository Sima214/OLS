#ifndef SATR_VK_SEVFATE_INTERACTIVE_LOGGER_HPP
#define SATR_VK_SEVFATE_INTERACTIVE_LOGGER_HPP
/**
 * @file
 * @brief Proxy header, so it can be easily ported/integrated to other code-bases.
 */

#include <OFS_FileLogging.h>
#include <cstdlib>

#define SEVFATE_INTERACTIVE_LOGGER_DEBUG(msg) LOG_DEBUG(msg)
#define SEVFATE_INTERACTIVE_LOGGER_INFO(msg) LOG_INFO(msg)
#define SEVFATE_INTERACTIVE_LOGGER_WARN(msg) LOG_WARN(msg)
#define SEVFATE_INTERACTIVE_LOGGER_ERROR(msg) LOG_ERROR(msg)
#define SEVFATE_INTERACTIVE_LOGGER_FATAL(msg) \
    { \
        LOG_ERROR(msg); \
        std::abort(); \
    }

#endif /*SATR_VK_SEVFATE_INTERACTIVE_LOGGER_HPP*/
