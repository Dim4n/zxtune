/*
Abstract:
  Different utils

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef __CORE_PLUGINS_UTILS_H_DEFINED__
#define __CORE_PLUGINS_UTILS_H_DEFINED__

//local includes
#include "core/src/callback.h"
//common includes
#include <logging.h>
#include <tools.h>
//std includes
#include <algorithm>
#include <cctype>
//boost includes
#include <boost/algorithm/string/trim.hpp>

inline String OptimizeString(const String& str, Char replace = '\?')
{
  // applicable only printable chars in range 0x21..0x7f
  String res(boost::algorithm::trim_copy_if(str, !boost::is_from_range('\x21', '\x7f')));
  std::replace_if(res.begin(), res.end(), std::ptr_fun<int, int>(&std::iscntrl), replace);
  return res;
}

inline Log::ProgressCallback::Ptr CreateProgressCallback(const ZXTune::Module::DetectCallback& callback, uint_t limit)
{
  if (Log::ProgressCallback* cb = callback.GetProgress())
  {
    return Log::CreatePercentProgressCallback(limit, *cb);
  }
  return Log::ProgressCallback::Ptr();
}

class IndexPathComponent
{
public:
  explicit IndexPathComponent(const String& prefix)
    : Prefix(prefix)
  {
  }

  bool GetIndex(const String& str, std::size_t& index) const
  {
    Parameters::IntType res = 0;
    if (0 == str.find(Prefix) && 
        Parameters::ConvertFromString(str.substr(Prefix.size()), res))
    {
      index = static_cast<std::size_t>(res);
      return true;
    }
    return false;
  }

  String Build(std::size_t idx) const
  {
    return Prefix + Parameters::ConvertToString(idx);
  }
private:
  const String Prefix;
};

#endif //__CORE_PLUGINS_UTILS_H_DEFINED__
