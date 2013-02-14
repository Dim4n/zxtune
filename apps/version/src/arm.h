/*
Abstract:
  ARM architecture detection

Last changed:
  $Id: api.h 2089 2012-11-02 12:49:49Z vitamin.caig $

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef ARM_H_DEFINED
#define ARM_H_DEFINED

#if defined(__arm__)
//gcc-based
  //version
  #if defined(__ARM_ARCH_4__) || \
      defined(__ARM_ARCH_4T__)
    const std::string ARCH_VERSION("v4");
  #elif defined(__ARM_ARCH_5__) || \
      defined(__ARM_ARCH_5E__) || \
      defined(__ARM_ARCH_5T__) || \
      defined(__ARM_ARCH_5TE__) || \
      defined(__ARM_ARCH_5TEJ__)
    const std::string ARCH_VERSION("v5");
  #elif defined(__ARM_ARCH_6__) || \
        defined(__ARM_ARCH_6K__) || \
        defined(__ARM_ARCH_6T2__) || \
        defined(__ARM_ARCH_6Z__) || \
        defined(__ARM_ARCH_6M__)
    const std::string ARCH_VERSION("v6");
  #elif defined(__ARM_ARCH_7__) || \
        defined(__ARM_ARCH_7A__) || \
        defined(__ARM_ARCH_7R__) || \
        defined(__ARM_ARCH_7M__) || \
        defined(__ARM_ARCH_7EM__)
    const std::string ARCH_VERSION("v7");
  #elif defined(__ARM_ARCH_8__) || \
        defined(__ARM_ARCH_8A__)
    const std::string ARCH_VERSION("v8");
  #else
    const std::string ARCH_VERSION;
  #endif
  //fpu type
  #if !defined(__SOFTFP__)
    const std::string ARCH("armhf");
  #else
    const std::string ARCH("arm");
  #endif
#elif defined(_M_ARM)
//msvs-based
  #if _M_ARM == 4
    const std::string ARCH_VERSION("v4");
  #elif _M_ARM == 5
    const std::string ARCH_VERSION("v5");
  #else
    const std::string ARCH_VERSION;
  #endif
  const std::string ARCH("arm");
#else
#error "Not an arm architecture"
#endif

#endif //ARM_H_DEFINED
