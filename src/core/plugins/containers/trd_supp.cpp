/*
Abstract:
  TRD containers support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "trdos_process.h"
#include "core/src/callback.h"
#include "core/plugins/registrator.h"
#include "core/plugins/utils.h"
#include "core/src/core.h"
//common includes
#include <byteorder.h>
#include <error_tools.h>
#include <logging.h>
#include <range_checker.h>
#include <tools.h>
//library includes
#include <core/error_codes.h>
#include <core/module_detect.h>
#include <core/plugin_attrs.h>
#include <core/plugins_parameters.h>
#include <io/container.h>
//boost includes
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
//text includes
#include <core/text/core.h>
#include <core/text/plugins.h>

#define FILE_TAG A1239034

namespace
{
  using namespace ZXTune;

  const Char TRD_PLUGIN_ID[] = {'T', 'R', 'D', 0};
  const String TRD_PLUGIN_VERSION(FromStdString("$Rev$"));

  //hints
  const std::size_t TRD_MODULE_SIZE = 655360;
  const uint_t BYTES_PER_SECTOR = 256;
  const uint_t SECTORS_IN_TRACK = 16;
  const uint_t MAX_FILES_COUNT = 128;
  const uint_t SERVICE_SECTOR_NUM = 8;

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif

  enum
  {
    NOENTRY = 0,
    DELETED = 1
  };
  PACK_PRE struct CatEntry
  {
    char Name[8];
    char Type[3];
    uint16_t Length;
    uint8_t SizeInSectors;
    uint8_t Sector;
    uint8_t Track;

    uint_t Offset() const
    {
      return BYTES_PER_SECTOR * (SECTORS_IN_TRACK * Track + Sector);
    }

    uint_t Size() const
    {
      //use rounded file size for better compatibility
      return BYTES_PER_SECTOR * SizeInSectors;
    }
  } PACK_POST;

  enum
  {
    TRDOS_ID = 0x10,

    DS_DD = 0x16,
    DS_SD = 0x17,
    SS_DD = 0x18,
    SS_SD = 0x19
  };

  PACK_PRE struct ServiceSector
  {
    uint8_t Zero;
    uint8_t Reserved1[224];
    uint8_t FreeSpaceSect;
    uint8_t FreeSpaceTrack;
    uint8_t Type;
    uint8_t Files;
    uint16_t FreeSectors;
    uint8_t ID;//0x10
    uint8_t Reserved2[12];
    uint8_t DeletedFiles;
    uint8_t Title[8];
    uint8_t Reserved3[3];
  } PACK_POST;

#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

  BOOST_STATIC_ASSERT(sizeof(CatEntry) == 16);
  BOOST_STATIC_ASSERT(sizeof(ServiceSector) == 256);

  const std::string TRD_SERVICE_SECTOR_PATTERN = 
    "16"            //type DS_DD
    "?"             //files
    "?%00000xxx"    //free sectors
    "10"            //ID
  ;

  const DataFormat::Ptr ServiceSectorFormat = DataFormat::Create(TRD_SERVICE_SECTOR_PATTERN);

  bool CheckTRDFile(const IO::FastDump& data)
  {
    //it's meaningless to support trunkated files
    if (data.Size() < TRD_MODULE_SIZE)
    {
      return false;
    }
    const ServiceSector* const sector = safe_ptr_cast<const ServiceSector*>(&data[SERVICE_SECTOR_NUM * BYTES_PER_SECTOR]);
    if (sector->ID != TRDOS_ID || sector->Type != DS_DD || 0 != sector->Zero)
    {
      return false;
    }
    const CatEntry* catEntry = safe_ptr_cast<const CatEntry*>(data.Data());
    const RangeChecker::Ptr checker = RangeChecker::Create(TRD_MODULE_SIZE);
    checker->AddRange(0, SECTORS_IN_TRACK * BYTES_PER_SECTOR);
    uint_t idx = 0;
    for (; idx != MAX_FILES_COUNT && NOENTRY != catEntry->Name[0]; ++idx, ++catEntry)
    {
      if (!catEntry->SizeInSectors)
      {
        continue;
      }
      const uint_t offset = catEntry->Offset();
      const uint_t size = catEntry->Size();
      if (!checker->AddRange(offset, size))
      {
        return false;
      }
    }
    return idx > 0;
  }

  TRDos::Catalogue::Ptr ParseTRDFile(IO::DataContainer::Ptr data)
  {
    assert(CheckTRDFile(IO::FastDump(*data)));

    const TRDos::CatalogueBuilder::Ptr builder = TRDos::CatalogueBuilder::CreateFlat(data);

    const ServiceSector* const sector = safe_ptr_cast<const ServiceSector*>(data->Data()) + SERVICE_SECTOR_NUM;
    uint_t deleted = 0;
    const CatEntry* catEntry = safe_ptr_cast<const CatEntry*>(data->Data());
    for (uint_t idx = 0; idx != MAX_FILES_COUNT && NOENTRY != catEntry->Name[0]; ++idx, ++catEntry)
    {
      //TODO: parametrize this
      const bool isDeleted = DELETED == catEntry->Name[0];
      if (isDeleted)
      {
        ++deleted;
      }
      if (catEntry->SizeInSectors)
      {
        String entryName = TRDos::GetEntryName(catEntry->Name, catEntry->Type);
        if (isDeleted)
        {
          entryName.insert(0, 1, '~');
        }
        const TRDos::File::Ptr newOne = TRDos::File::CreateReference(entryName, catEntry->Offset(), catEntry->Size());
        builder->AddFile(newOne);
      }
    }
    if (deleted != sector->DeletedFiles)
    {
      Log::Debug("Core::TRDSupp", "Deleted files count is differs from calculated");
    }
    builder->SetUsedSize(TRD_MODULE_SIZE);
    return builder->GetResult();
  }

  class TRDDetectionResult : public DetectionResult
  {
  public:
    TRDDetectionResult(std::size_t parsedSize, IO::DataContainer::Ptr rawData)
      : ParsedSize(parsedSize)
      , RawData(rawData)
    {
    }

    virtual std::size_t GetMatchedDataSize() const
    {
      return ParsedSize;
    }

    virtual std::size_t GetLookaheadOffset() const
    {
      const std::size_t size = RawData->Size();
      if (size < TRD_MODULE_SIZE)
      {
        return size;
      }
      const std::size_t skipBytes = SERVICE_SECTOR_NUM * BYTES_PER_SECTOR + offsetof(ServiceSector, Type);
      const uint8_t* const begin = static_cast<const uint8_t*>(RawData->Data()) + skipBytes;
      return ServiceSectorFormat->Search(begin, size - skipBytes);
    }
  private:
    const std::size_t ParsedSize;
    const IO::DataContainer::Ptr RawData;
  };


  class TRDPlugin : public ArchivePlugin
                  , public boost::enable_shared_from_this<TRDPlugin>
  {
  public:
    virtual String Id() const
    {
      return TRD_PLUGIN_ID;
    }

    virtual String Description() const
    {
      return Text::TRD_PLUGIN_INFO;
    }

    virtual String Version() const
    {
      return TRD_PLUGIN_VERSION;
    }

    virtual uint_t Capabilities() const
    {
      return CAP_STOR_MULTITRACK | CAP_STOR_PLAIN;
    }

    virtual DetectionResult::Ptr Detect(DataLocation::Ptr input, const Module::DetectCallback& callback) const
    {
      const IO::DataContainer::Ptr rawData = input->GetData();
      const IO::FastDump dump(*rawData);
      std::size_t parsedSize = 0;
      if (CheckTRDFile(dump))
      {
        const TRDos::Catalogue::Ptr files = ParseTRDFile(rawData);
        if (files->GetFilesCount())
        {
          ProcessEntries(input, callback, shared_from_this(), *files);
          parsedSize = files->GetUsedSize();
        }
      }
      return boost::make_shared<TRDDetectionResult>(parsedSize, rawData);
    }

    virtual DataLocation::Ptr Open(const Parameters::Accessor& /*commonParams*/, DataLocation::Ptr location, const DataPath& inPath) const
    {
      const String& pathComp = inPath.GetFirstComponent();
      if (pathComp.empty())
      {
        return DataLocation::Ptr();
      }
      const IO::DataContainer::Ptr inData = location->GetData();
      const IO::FastDump dump(*inData); 
      if (!CheckTRDFile(dump))
      {
        return DataLocation::Ptr();
      }
      const TRDos::Catalogue::Ptr files = ParseTRDFile(inData);
      if (const TRDos::File::Ptr fileToOpen = files->FindFile(pathComp))
      {
        const Plugin::Ptr subPlugin = shared_from_this();
        const IO::DataContainer::Ptr subData = fileToOpen->GetData();
        return CreateNestedLocation(location, subData, subPlugin, pathComp); 
      }
      return DataLocation::Ptr();
    }
  };
}

namespace ZXTune
{
  void RegisterTRDContainer(PluginsRegistrator& registrator)
  {
    const ArchivePlugin::Ptr plugin(new TRDPlugin());
    registrator.RegisterPlugin(plugin);
  }
}
