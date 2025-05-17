#pragma once

#include <stdio.h>

#define LOG_INFO(...) printf("[ \x1b[36mINFO\x1b[0m ] " __VA_ARGS__)
#define LOG_OK(...) printf("[ \x1b[32mOK\x1b[0m ] " __VA_ARGS__)
#define LOG_WARNING(...) printf("[ \x1b[93mWARNING\x1b[0m ] " __VA_ARGS__)
#define LOG_ERROR(...) printf("[ \x1b[31mERROR\x1b[0m ] " __VA_ARGS__)
