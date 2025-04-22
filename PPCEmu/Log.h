#include <iostream>

#define LOG_INFO(system, msg, ...)    printf("[INFO]    " msg "\n", ##__VA_ARGS__)
#define LOG_DEBUG(system, msg, ...)    printf("[DEBUG]    " msg "\n", ##__VA_ARGS__)
#define LOG_ERROR(system, msg, ...)    printf("[ERROR]    " msg "\n", ##__VA_ARGS__)
#define LOG_WARNING(system, msg, ...) printf("[WARNING] " msg "\n", ##__VA_ARGS__)
#define LOG_CRITICAL(system, msg, ...) printf("[CRITICAL] " msg "\n", ##__VA_ARGS__)
#define SYSTEM_PAUSE() std::cin.get()
