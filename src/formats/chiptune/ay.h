/*
Abstract:
  AY modules format description

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef FORMATS_CHIPTUNE_AY_H_DEFINED
#define FORMATS_CHIPTUNE_AY_H_DEFINED

//common includes
#include <types.h>
//library includes
#include <formats/chiptune.h>

namespace Formats
{
  namespace Chiptune
  {
    namespace AY
    {
      class Builder
      {
      public:
        virtual ~Builder() {}

        virtual void SetTitle(const String& title) = 0;
        virtual void SetAuthor(const String& author) = 0;
        virtual void SetComment(const String& comment) = 0;
        virtual void SetDuration(uint_t duration) = 0;
        virtual void SetRegisters(uint16_t reg, uint16_t sp) = 0;
        virtual void SetRoutines(uint16_t init, uint16_t play) = 0;
        virtual void AddBlock(uint16_t addr, const void* data, std::size_t size) = 0;
      };

      uint_t GetModulesCount(const Binary::Container& data);
      Formats::Chiptune::Container::Ptr Parse(const Binary::Container& data, std::size_t idx, Builder& target);
      Builder& GetStubBuilder();

      class BlobBuilder : public Builder
      {
      public:
        typedef boost::shared_ptr<BlobBuilder> Ptr;

        virtual Binary::Container::Ptr Result() const = 0;
      };

      BlobBuilder::Ptr CreateMemoryDumpBuilder();
      BlobBuilder::Ptr CreateFileBuilder();
    }
  }
}

#endif //FORMATS_CHIPTUNE_AY_H_DEFINED
