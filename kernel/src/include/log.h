#pragma once

#include <stdio.h>

#define LOG_INFO(...) printf("\x1b[46;30mINFO:\x1b[40;37m " __VA_ARGS__)
#define LOG_OK(...) printf("\x1b[42;30mOK:\x1b[40;37m " __VA_ARGS__)
#define LOG_WARNING(...) printf("\x1b[103;30mWARNING:\x1b[40;37m " __VA_ARGS__)
#define LOG_ERROR(...) printf("\x1b[41;30mERROR:\x1b[40;37m " __VA_ARGS__)
