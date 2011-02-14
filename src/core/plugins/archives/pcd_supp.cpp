/*
Abstract:
  Powerfull Code Decreaser convertors support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
  (C) Based on XLook sources by HalfElf
*/

//local includes
#include "hrust1_bitstream.h"
#include "pack_utils.h"
#include <core/plugins/enumerator.h>
//common includes
#include <byteorder.h>
#include <detector.h>
#include <tools.h>
//library includes
#include <core/plugin_attrs.h>
#include <io/container.h>
//std includes
#include <algorithm>
#include <iterator>
//text includes
#include <core/text/plugins.h>

namespace PowerfullCodeDecreaser
{
  const std::size_t MAX_DECODED_SIZE = 0xc000;

  struct Version61
  {
    static const std::string DEPACKER_PATTERN;

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
    PACK_PRE struct FormatHeader
    {
      //+0
      uint8_t Padding1[0xe];
      //+0xe
      uint16_t LastOfSrcPacked;
      //+0x10
      uint8_t Padding2;
      //+0x11
      uint16_t LastOfDstPacked;
      //+0x13
      uint8_t Padding3;
      //+0x14
      uint16_t SizeOfPacked;
      //+0x16
      uint8_t Padding4[0xb3];
      //+0xc9
      uint8_t LastBytes[3];
      uint8_t Bitstream[2];
    } PACK_POST;
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

    BOOST_STATIC_ASSERT(sizeof(FormatHeader) == 0xc9 + 3 + 2);
  };

  struct Version62
  {
    static const std::string DEPACKER_PATTERN;

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
    PACK_PRE struct FormatHeader
    {
      //+0
      uint8_t Padding1[0x14];
      //+0x14
      uint16_t LastOfSrcPacked;
      //+0x16
      uint8_t Padding2;
      //+0x17
      uint16_t LastOfDstPacked;
      //+0x19
      uint8_t Padding3;
      //+0x1a
      uint16_t SizeOfPacked;
      //+0x1c
      uint8_t Padding4[0xa8];
      //+0xc4
      uint8_t LastBytes[5];
      uint8_t Bitstream[2];
    } PACK_POST;
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

    BOOST_STATIC_ASSERT(sizeof(FormatHeader) == 0xc4 + 5 + 2);
  };

  const std::string Version61::DEPACKER_PATTERN =
    "?"       // di/nop
    "21??"    // ld hl,xxxx 0xc017   depacker src
    "11??"    // ld de,xxxx 0xbf00   depacker target
    "01b500"  // ld bc,xxxx 0x00b5   depacker size
    "d5"      // push de
    "edb0"    // ldir
    "21??"    // ld hl,xxxx 0xd845   last of src packed (data = +0xe)
    "11??"    // ld de,xxxx 0xffff   last of dst packed (data = +0x11)
    "01??"    // ld bc,xxxx 0x177d   packed size (data = +0x14)
    "c9"      // ret
    "ed?"     // lddr/ldir                +0x17
    "21??"    // ld hl,xxxx 0xe883   rest bytes src (data = +0x1a)
    "11??"    // ld de,xxxx 0xbfb2   rest bytes dst (data = +0x1d)
    "01??"    // ld bc,xxxx 0x0003   rest bytes count
    "d5"      // push de
    "c5"      // push bc
    "edb0"    // ldir
    "ed73??"  // ld (xxxx),sp 0xbfa3
    "f9"      // ld sp,hl
    "11??"    // ld de,xxxx   0xc000 target of depack (data = +0x2c)
    "60"      // ld h,b
    "d9"      // exx
    "011001"  // ld bc,xxxx ; 0x110
    "3ed9"    // ld a,x ;0xd9
    "1002"    // djnz xxx, (2)
    "e1"      // pop hl
    "41"      // ld b,c
    "29"      // add hl,hl
    "3007"    // jr nc,xx
    "3b"      // dec sp
    "f1"      // pop af
    "d9"      // exx
    "12"      // ld (de),a
    "13"      // inc de
    "18f1"    // jr ...
  ;

  const std::string Version62::DEPACKER_PATTERN =
    "?"       // di/nop
    "21??"    // ld hl,xxxx 0x6026   depacker src
    "11??"    // ld de,xxxx 0x5b00   depacker target
    "01a300"  // ld bc,xxxx 0x00a3   depacker size
    "edb0"    // ldir
    "011001"  // ld bc,xxxx ; 0x110
    "d9"      // exx
    "22??"    // ld (xxxx),hl 0x5b97
    "21??"    // ld hl,xxxx 0xb244   last of src packed (data = +0x14)
    "11??"    // ld de,xxxx 0xfaa4   last of dst packed (data = +0x17)
    "01??"    // ld bc,xxxx 0x517c   packed size (data = +0x1a)
    "ed73??"  // ld (xxxx),sp 0x0000
    "31??"    // ld sp,xxxx   0xa929
    "c3??"    // jp xxxx 0x5b00
    "ed?"     // lddr/lddr           +0x26
    "11??"    // ld de,xxxx 0x6000   target of depack (data = +0x29)
    "60"      // ld h,b
    "d9"      // exx
    "1002"    // djnz xxx, (2)
    "e1"      // pop hl
    "41"      // ld b,c
    "29"      // add hl,hl
    "3007"    // jr nc,xx
    "3b"      // dec sp
    "f1"      // pop af
    "d9"      // exx
    "12"      // ld (de),a
    "13"      // inc de
    "18f1"    // jr ...
  ;

  template<class Version>
  class Container
  {
  public:
    Container(const void* data, std::size_t size)
      : Data(static_cast<const uint8_t*>(data))
      , Size(size)
    {
    }

    bool FastCheck() const
    {
      if (Size < sizeof(typename Version::FormatHeader))
      {
        return false;
      }
      const uint_t usedSize = GetUsedSize();
      return usedSize <= Size;
    }

    bool FullCheck() const
    {
      if (!FastCheck())
      {
        return false;
      }
      return DetectFormat(Data, Size, Version::DEPACKER_PATTERN);
    }

    uint_t GetUsedSize() const
    {
      const typename Version::FormatHeader& header = GetHeader();
      return sizeof(header) + fromLE(header.SizeOfPacked) - sizeof(header.LastBytes) - sizeof(header.Bitstream);
    }

    const typename Version::FormatHeader& GetHeader() const
    {
      assert(Size >= sizeof(typename Version::FormatHeader));
      return *safe_ptr_cast<const typename Version::FormatHeader*>(Data);
    }
  private:
    const uint8_t* const Data;
    const std::size_t Size;
  };

  class Bitstream : public ByteStream
  {
  public:
    Bitstream(const uint8_t* data, std::size_t size)
      : ByteStream(data, size)
      , Bits(), Mask(0)
    {
    }

    uint_t GetBit()
    {
      if (!(Mask >>= 1))
      {
        Bits = GetLEWord();
        Mask = 0x8000;
      }
      return (Bits & Mask) != 0 ? 1 : 0;
    }

    uint_t GetBits(unsigned count)
    {
      uint_t result = 0;
      while (count--)
      {
        result = 2 * result | GetBit();
      }
      return result;
    }
  private:
    uint_t Bits;
    uint_t Mask;
  };

  class BitstreamDecoder
  {
  public:
    template<class Header>
    explicit BitstreamDecoder(const Header& header)
      : Stream(header.Bitstream, fromLE(header.SizeOfPacked) - sizeof(header.LastBytes))
    {
    }

    bool DecodeMainData()
    {
      while (GetSingleBytes() &&
             Decoded.size() < MAX_DECODED_SIZE)
      {
        const uint_t index = GetIndex();
        uint_t offset = 0;
        uint_t len = index + 1;
        if (0 == index)
        {
          offset = Stream.GetBits(4) + 1;
        }
        else if (1 == index)
        {
          offset = 1 + Stream.GetByte();
          if (0x100 == offset)
          {
            return true;
          }
        }
        else
        {
          len = GetLongLen(index);
          offset = GetLongOffset();
        }
        if (!CopyFromBack(offset, Decoded, len))
        {
          return false;
        }
      }
      return true;
    }

    bool GetSingleBytes()
    {
      while (Stream.GetBit())
      {
        Decoded.push_back(Stream.GetByte());
      }
      return !Stream.Eof();
    }

    uint_t GetIndex()
    {
      uint_t idx = 0;
      for (uint_t bits = 3; bits == 3 && idx != 0xf;)
      {
        bits = Stream.GetBits(2);
        idx += bits;
      }
      return idx;
    }

    uint_t GetLongLen(uint_t idx)
    {
      uint_t len = idx;
      if (3 == idx)
      {
        if (uint_t len = Stream.GetByte())
        {
          return len + 0xf;
        }
        else
        {
          return Stream.GetLEWord();
        }
      }
      else if (2 == idx)
      {
        return 3;
      }
      return len;
    }

    uint_t GetLongOffset()
    {
      const uint_t loOffset = Stream.GetByte();
      uint_t hiOffset = 0;
      if (Stream.GetBit())
      {
        do
        {
          hiOffset = 4 * hiOffset + Stream.GetBits(2);
        }
        while (Stream.GetBit());
        ++hiOffset;
      }
      return 256 * hiOffset + loOffset;
    }
  protected:
    Bitstream Stream;
    Dump Decoded;
  };

  template<class Version>
  class Decoder : private BitstreamDecoder
  {
  public:
    explicit Decoder(const Container<Version>& container)
      : BitstreamDecoder(container.GetHeader())
      , IsValid(container.FastCheck())
      , Header(container.GetHeader())
    {
    }

    const Dump* GetDecodedData()
    {
      if (IsValid && !Stream.Eof() && Decoded.empty())
      {
        IsValid = DecodeData();
      }
      return IsValid ? &Decoded : 0;
    }
  private:
    bool DecodeData()
    {
      Decoded.reserve(2 * fromLE(Header.SizeOfPacked));

      if (!DecodeMainData())
      {
        return false;
      }
      std::copy(Header.LastBytes, ArrayEnd(Header.LastBytes), std::back_inserter(Decoded));
      return true;
    }
  private:
    bool IsValid;
    const typename Version::FormatHeader& Header;
  };
}

namespace
{
  using namespace ZXTune;

  const Char PCD_PLUGIN_ID[] = {'P', 'C', 'D', '\0'};
  const String PCD_PLUGIN_VERSION(FromStdString("$Rev$"));

  class PCDPlugin : public ArchivePlugin
  {
  public:
    virtual String Id() const
    {
      return PCD_PLUGIN_ID;
    }

    virtual String Description() const
    {
      return Text::PCD_PLUGIN_INFO;
    }

    virtual String Version() const
    {
      return PCD_PLUGIN_VERSION;
    }

    virtual uint_t Capabilities() const
    {
      return CAP_STOR_CONTAINER;
    }

    virtual bool Check(const IO::DataContainer& inputData) const
    {
      const PowerfullCodeDecreaser::Container<PowerfullCodeDecreaser::Version61> container61(inputData.Data(), inputData.Size());
      if (container61.FullCheck())
      {
        return true;
      }
      const PowerfullCodeDecreaser::Container<PowerfullCodeDecreaser::Version62> container62(inputData.Data(), inputData.Size());
      return container62.FullCheck();
    }

    virtual IO::DataContainer::Ptr ExtractSubdata(const Parameters::Accessor& /*parameters*/,
      const IO::DataContainer& data, ModuleRegion& region) const
    {
      {
        const PowerfullCodeDecreaser::Container<PowerfullCodeDecreaser::Version61> container61(data.Data(), data.Size());
        if (container61.FastCheck())
        {
          PowerfullCodeDecreaser::Decoder<PowerfullCodeDecreaser::Version61> decoder61(container61);
          if (const Dump* res = decoder61.GetDecodedData())
          {
            region.Offset = 0;
            region.Size = container61.GetUsedSize();
            return IO::CreateDataContainer(*res);
          }
        }
      }
      {
        const PowerfullCodeDecreaser::Container<PowerfullCodeDecreaser::Version62> container62(data.Data(), data.Size());
        PowerfullCodeDecreaser::Decoder<PowerfullCodeDecreaser::Version62> decoder62(container62);
        if (const Dump* res = decoder62.GetDecodedData())
        {
          region.Offset = 0;
          region.Size = container62.GetUsedSize();
          return IO::CreateDataContainer(*res);
        }
      }
      return IO::DataContainer::Ptr();
    }
  };
}

namespace ZXTune
{
  void RegisterPCDConvertor(PluginsEnumerator& enumerator)
  {
    const ArchivePlugin::Ptr plugin(new PCDPlugin());
    enumerator.RegisterPlugin(plugin);
  }
}