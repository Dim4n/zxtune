/*
Abstract:
  Module detect helper implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "detect_helper.h"
//common includes
#include <detector.h>

namespace
{
  using namespace ZXTune;

  class FastDataContainer : public IO::DataContainer
  {
  public:
    FastDataContainer(const IO::DataContainer& delegate, std::size_t offset)
      : Delegate(delegate)
      , Offset(offset)
    {
    }

    virtual std::size_t Size() const
    {
      return Delegate.Size() - Offset;
    }

    virtual const void* Data() const
    {
      return static_cast<const uint8_t*>(Delegate.Data()) + Offset;
    }

    virtual Ptr GetSubcontainer(std::size_t offset, std::size_t size) const
    {
      return Delegate.GetSubcontainer(offset + Offset, size);
    }
  private:
    const IO::DataContainer& Delegate;
    const std::size_t Offset;
  };
}

namespace ZXTune
{
  bool CheckDataFormat(const DataDetector& detector, const IO::DataContainer& inputData)
  {
    const uint8_t* const data = static_cast<const uint8_t*>(inputData.Data());
    const std::size_t limit = inputData.Size();

    //detect without
    if (detector.CheckData(data, limit))
    {
      return true;
    }
    for (DataPrefixIterator chain = detector.GetPrefixes(); chain; ++chain)
    {
      if (limit > chain->PrefixSize &&
          DetectFormat(data, limit, chain->PrefixDetectString) &&
          detector.CheckData(data + chain->PrefixSize, limit - chain->PrefixSize))
      {
        return true;
      }
    }
    return false;
  }

  Module::Holder::Ptr CreateModuleFromData(const ModuleDetector& detector,
    Parameters::Accessor::Ptr parameters, const MetaContainer& container, ModuleRegion& region)
  {
    const std::size_t limit(container.Data->Size());
    const uint8_t* const data(static_cast<const uint8_t*>(container.Data->Data()));

    ModuleRegion tmpRegion;
    //try to detect without player
    if (detector.CheckData(data, limit))
    {
      if (Module::Holder::Ptr holder = detector.TryToCreateModule(parameters, container, tmpRegion))
      {
        region = tmpRegion;
        return holder;
      }
    }
    for (DataPrefixIterator chain = detector.GetPrefixes(); chain; ++chain)
    {
      tmpRegion.Offset = chain->PrefixSize;
      if (limit < chain->PrefixSize ||
          !DetectFormat(data, limit, chain->PrefixDetectString) ||
          !detector.CheckData(data + chain->PrefixSize, limit - region.Offset))
      {
        continue;
      }
      if (Module::Holder::Ptr holder = detector.TryToCreateModule(parameters, container, tmpRegion))
      {
        region = tmpRegion;
        return holder;
      }
    }
    return Module::Holder::Ptr();
  }

  IO::DataContainer::Ptr ExtractSubdataFromData(const ArchiveDetector& detector,
    const Parameters::Accessor& parameters, const IO::DataContainer& data, ModuleRegion& region)
  {
    const std::size_t limit(data.Size());
    const uint8_t* const rawData(static_cast<const uint8_t*>(data.Data()));

    //try to detect without prefix
    if (detector.CheckData(rawData, limit))
    {
      std::size_t packedSize = 0;
      if (IO::DataContainer::Ptr subdata = detector.TryToExtractSubdata(parameters, data, packedSize))
      {
        region.Offset = 0;
        region.Size = packedSize;
        return subdata;
      }
    }
    //check all the prefixes
    for (DataPrefixIterator chain = detector.GetPrefixes(); chain; ++chain)
    {
      const std::size_t dataOffset = chain->PrefixSize;
      if (limit < dataOffset ||
          !DetectFormat(rawData, dataOffset, chain->PrefixDetectString) ||
          !detector.CheckData(rawData + dataOffset, limit - dataOffset))
      {
        continue;
      }
      const FastDataContainer subcontainer(data, dataOffset);
      std::size_t packedSize = 0;
      if (IO::DataContainer::Ptr subdata = detector.TryToExtractSubdata(parameters, subcontainer, packedSize))
      {
        region.Offset = dataOffset;
        region.Size = packedSize;
        return subdata;
      }
    }
    return IO::DataContainer::Ptr();
  }
}
