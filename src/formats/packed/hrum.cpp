/*
Abstract:
  Hrum support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
  (C) Based on XLook sources by HalfElf
*/

//local includes
#include "hrust1_bitstream.h"
#include "pack_utils.h"
//common includes
#include <byteorder.h>
#include <detector.h>
#include <tools.h>
//library includes
#include <formats/packed.h>
//std includes
#include <numeric>
//text includes
#include <core/text/plugins.h>

namespace Hrum
{
  const std::size_t MAX_DECODED_SIZE = 0xc000;
  //checkers
  const std::string DEPACKER_PATTERN =
    "?"       // di/nop
    "ed73??"  // ld (xxxx),sp
    "21??"    // ld hl,xxxx   start+0x1f
    "11??"    // ld de,xxxx   tmp buffer
    "017700"  // ld bc,0x0077 size of depacker
    "d5"      // push de
    "edb0"    // ldir
    "11??"    // ld de,xxxx   dst of depack (data = +0x12)
    "d9"      // exx
    "21??"    // ld hl,xxxx   last byte of src packed (data = +0x16)
    "11??"    // ld de,xxxx   last byte of dst packed (data = +0x19)
    "01??"    // ld bc,xxxx   size of packed          (data = +0x1c)
    "c9"      // ret
    "ed?"     // lddr/ldir
    "16?"     // ld d,xx
    "31??"    // ld sp,xxxx   ;start of moved packed (data = +0x24)
    "c1"      // pop bc
  ;

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
  PACK_PRE struct RawHeader
  {
    //+0
    uint8_t Padding1[0x16];
    //+0x16
    uint16_t PackedSource;
    //+0x18
    uint8_t Padding2;
    //+0x19
    uint16_t PackedTarget;
    //+0x1b
    uint8_t Padding3;
    //+0x1c
    uint16_t SizeOfPacked;
    //+0x1e
    uint8_t Padding4[2];
    //+0x20
    uint8_t PackedDataCopyDirection;
    //+0x21
    uint8_t Padding5[3];
    //+0x24
    uint16_t FirstOfPacked;
    //+0x26
    uint8_t Padding6[0x6b];
    //+0x91
    uint8_t LastBytes[5];
    //+0x96 taken from stack to initialize variables, always 0x1010
    //packed data starts from here
    uint8_t Padding7[2];
    //+0x98
    uint8_t BitStream[2];
    //+0x9a
    uint8_t ByteStream[1];
    //+0x9b
  } PACK_POST;
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

  BOOST_STATIC_ASSERT(sizeof(RawHeader) == 0x9b);

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
      if (Size < sizeof(RawHeader))
      {
        return false;
      }
      const RawHeader& header = GetHeader();
      const DataMovementChecker checker(fromLE(header.PackedSource), fromLE(header.PackedTarget), fromLE(header.SizeOfPacked), header.PackedDataCopyDirection);
      if (!checker.IsValid())
      {
        return false;
      }
      if (checker.FirstOfMovedData() != fromLE(header.FirstOfPacked))
      {
        return false;
      }
      return GetUsedSize() <= Size;
    }

    bool FullCheck() const
    {
      if (!FastCheck())
      {
        return false;
      }
      return DetectFormat(Data, Size, DEPACKER_PATTERN);
    }

    uint_t GetUsedSize() const
    {
      const RawHeader& header = GetHeader();
      return sizeof(header) - (sizeof(header.Padding7) + sizeof(header.BitStream) + sizeof(header.ByteStream))
        + fromLE(header.SizeOfPacked);
    }

    const RawHeader& GetHeader() const
    {
      assert(Size >= sizeof(RawHeader));
      return *safe_ptr_cast<const RawHeader*>(Data);
    }
  private:
    const uint8_t* const Data;
    const std::size_t Size;
  };

  class Bitstream : public Hrust1Bitstream
  {
  public:
    Bitstream(const uint8_t* data, std::size_t size)
      : Hrust1Bitstream(data, size)
    {
    }

    uint_t GetDist()
    {
      if (GetBit())
      {
        const uint_t hiBits = 0xf0 + GetBits(4);
        return 256 * hiBits + GetByte();
      }
      else
      {
        return 0xff00 + GetByte();
      }
    }
  };

  class DataDecoder
  {
  public:
    explicit DataDecoder(const Container& container)
      : IsValid(container.FastCheck())
      , Header(container.GetHeader())
      , Stream(Header.BitStream, fromLE(Header.SizeOfPacked) - sizeof(Header.Padding7))
    {
    }

    Dump* GetDecodedData()
    {
      if (IsValid && !Stream.Eof())
      {
        IsValid = DecodeData();
      }
      return IsValid ? &Decoded : 0;
    }
  private:
    bool DecodeData()
    {
      // The main concern is to decode data as much as possible, skipping defenitely invalid structure
      Decoded.reserve(2 * fromLE(Header.SizeOfPacked));
      //put first byte
      Decoded.push_back(Stream.GetByte());
      //assume that first byte always exists due to header format
      while (!Stream.Eof() && Decoded.size() < MAX_DECODED_SIZE)
      {
        if (Stream.GetBit())
        {
          Decoded.push_back(Stream.GetByte());
          continue;
        }
        uint_t len = 1 + Stream.GetLen();
        uint_t offset = 0;
        if (4 == len)
        {
          len = Stream.GetByte();
          if (!len)
          {
            //eof
            break;
          }
          offset = Stream.GetDist();
        }
        else
        {
          if (len > 4)
          {
            --len;
          }
          offset = DecodeOffsetByLen(len);
        }
        if (!CopyFromBack(-static_cast<int16_t>(offset), Decoded, len))
        {
          return false;
        }
      }
      //put remaining bytes
      std::copy(Header.LastBytes, ArrayEnd(Header.LastBytes), std::back_inserter(Decoded));
      return true;
    }
  private:
    uint_t DecodeOffsetByLen(uint_t len)
    {
      if (1 == len)
      {
        return 0xfff8 + Stream.GetBits(3);
      }
      else if (2 == len)
      {
        return 0xff00 + Stream.GetByte();
      }
      else
      {
        return Stream.GetDist();
      }
    }
  private:
    bool IsValid;
    const RawHeader& Header;
    Bitstream Stream;
    Dump Decoded;
  };
}

namespace Formats
{
  namespace Packed
  {
    class HrumDecoder : public Decoder
    {
    public:
      virtual bool Check(const void* data, std::size_t availSize) const
      {
        const Hrum::Container container(data, availSize);
        return container.FullCheck();
      }

      virtual std::auto_ptr<Dump> Decode(const void* data, std::size_t availSize, std::size_t& usedSize) const
      {
        const Hrum::Container container(data, availSize);
        assert(container.FullCheck());
        Hrum::DataDecoder decoder(container);
        if (Dump* decoded = decoder.GetDecodedData())
        {
          usedSize = container.GetUsedSize();
          std::auto_ptr<Dump> res(new Dump());
          res->swap(*decoded);
          return res;
        }
        return std::auto_ptr<Dump>();
      }
    };

    Decoder::Ptr CreateHrumDecoder()
    {
      return Decoder::Ptr(new HrumDecoder());
    }
  }
}