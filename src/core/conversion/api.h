/*
Abstract:
  Conversion API interface

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef CORE_CONVERSION_API_H_DEFINED
#define CORE_CONVERSION_API_H_DEFINED

//library includes
#include <core/module_holder.h>
#include <core/conversion.h>

namespace ZXTune
{
  namespace Module
  {
    Binary::Data::Ptr Convert(const Holder& holder, const Conversion::Parameter& spec, Parameters::Accessor::Ptr params);
  }
}

#endif //CORE_CONVERSION_API_H_DEFINED