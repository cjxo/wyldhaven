/* date = July 1st 2024 10:22 pm */

#ifndef WYLD_COMPILER_STUFF_H
#define WYLD_COMPILER_STUFF_H

#if defined(OS_WINDOWS)
# pragma section(".rdata$", read)
# define read_only static __declspec(allocate(".rdata$"))
#endif

#if defined(COMPILER_CL)
# define thread_var __declspec(thread)
# define dll_export __declspec(dllexport)
# define dll_import __declspec(dllimport)
# define intrin_rdtsc() __rdtsc()
#endif

#endif //WYLD_COMPILER_STUFF_H
