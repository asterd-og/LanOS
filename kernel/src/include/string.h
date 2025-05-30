#pragma once

#include <stdint.h>
#include <stddef.h>

#define toupper(x) (x > 'A' ? x - 32 : x)

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

int strlen(const char *s1);
int strcmp(const char *s1, const char *s2);
char *strtok(char *str, const char *delim);
