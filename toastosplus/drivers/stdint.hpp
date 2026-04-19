/*
 * toastOS++ Standard Integer Types
 * Converted to C++ from toastOS
 */

#ifndef STDINT_HPP
#define STDINT_HPP

#ifdef __cplusplus
extern "C" {
#endif

/* Exact-width integer types */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

/* Minimum-width integer types */
typedef signed char int_least8_t;
typedef unsigned char uint_least8_t;
typedef short int_least16_t;
typedef unsigned short uint_least16_t;
typedef int int_least32_t;
typedef unsigned int uint_least32_t;
typedef long long int_least64_t;
typedef unsigned long long uint_least64_t;

/* Fastest minimum-width integer types */
typedef signed char int_fast8_t;
typedef unsigned char uint_fast8_t;
typedef int int_fast16_t;
typedef unsigned int uint_fast16_t;
typedef int int_fast32_t;
typedef unsigned int uint_fast32_t;
typedef long long int_fast64_t;
typedef unsigned long long uint_fast64_t;

/* Integer types capable of holding object pointers */
typedef int intptr_t;
typedef unsigned int uintptr_t;

/* Greatest-width integer types */
typedef long long intmax_t;
typedef unsigned long long uintmax_t;

/* Size type */
typedef unsigned int size_t;

/* NULL pointer */
#ifndef NULL
#ifdef __cplusplus
#define NULL nullptr
#else
#define NULL ((void*)0)
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* STDINT_HPP */
