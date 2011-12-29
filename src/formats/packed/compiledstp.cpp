/*
Abstract:
  Sound Tracker Pro compiled modules support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//library includes
#include "container.h"
#include <formats/chiptune/soundtrackerpro.h>
//common includes
#include <byteorder.h>
#include <logging.h>
#include <tools.h>
//library includes
#include <binary/typed_container.h>
//std includes
#include <cstring>
//boost includes
#include <boost/array.hpp>
//text includes
#include <formats/text/chiptune.h>
#include <formats/text/packed.h>

namespace
{
  const std::string THIS_MODULE("Formats::Packed::CompiledSTP");
}

namespace CompiledSTP
{
  const std::size_t MAX_PLAYER_SIZE = 2000;

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
  struct Version1
  {
    static const String DESCRIPTION;
    static const std::string FORMAT;

    PACK_PRE struct Player
    {
      uint8_t Padding1;
      uint16_t DataAddr;
      uint8_t Padding2;
      uint16_t InitAddr;
      uint8_t Padding3;
      uint16_t PlayAddr;
      uint8_t Padding4[8];
      //+17
      uint8_t Information[53];
      uint8_t Padding5[8];
      //+78
      uint8_t Initialization;

      std::size_t GetSize() const
      {
        const uint_t initAddr = fromLE(InitAddr);
        const uint_t compileAddr = initAddr - offsetof(Player, Initialization);
        return fromLE(DataAddr) - compileAddr;
      }

      Dump GetInfo() const
      {
        return Dump(&Information[0], ArrayEnd(Information));
      }
    } PACK_POST;
  };

  struct Version2
  {
    static const String DESCRIPTION;
    static const std::string FORMAT;

    PACK_PRE struct Player
    {
      uint8_t Padding1;
      uint16_t InitAddr;
      uint8_t Padding2;
      uint16_t PlayAddr;
      uint8_t Padding3[2];
      //+8
      uint8_t Information[56];
      uint8_t Padding4[8];
      //+0x48
      uint8_t Initialization[2];
      uint16_t DataAddr;

      std::size_t GetSize() const
      {
        const uint_t initAddr = fromLE(InitAddr);
        const uint_t compileAddr = initAddr - offsetof(Player, Initialization);
        return fromLE(DataAddr) - compileAddr;
      }

      Dump GetInfo() const
      {
        Dump result(53);
        const uint8_t* const src = Information;
        uint8_t* const dst = &result[0];
        std::memcpy(dst, src, 24);
        std::memcpy(dst + 24, src + 26, 4);
        std::memcpy(dst + 27, src + 31, 25);
        return result;
      }
    } PACK_POST;
  };
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

  BOOST_STATIC_ASSERT(offsetof(Version1::Player, Information) == 17);
  BOOST_STATIC_ASSERT(offsetof(Version1::Player, Initialization) == 78);
  BOOST_STATIC_ASSERT(offsetof(Version2::Player, Information) == 8);
  BOOST_STATIC_ASSERT(offsetof(Version2::Player, Initialization) == 72);

  const String Version1::DESCRIPTION = String(Text::SOUNDTRACKERPRO_DECODER_DESCRIPTION) + Text::PLAYER_SUFFIX;
  const String Version2::DESCRIPTION = String(Text::SOUNDTRACKERPRO2_DECODER_DESCRIPTION) + Text::PLAYER_SUFFIX;

  const std::string Version1::FORMAT =
    "21??"     //ld hl,ModuleAddr
    "c3??"     //jp xxxx
    "c3??"     //jp xxxx
    "ed4b??"   //ld bc,(xxxx)
    "c3??"     //jp xxxx
    "?"        //nop?
    "'K'S'A' 'S'O'F'T'W'A'R'E' 'C'O'M'P'I'L'A'T'I'O'N' 'O'F' "
    "+25+"
    "+8+"
    "f3"       //di
    "22??"     //ld (xxxx),hl
    "3e?"      //ld a,xx
    "32??"     //ld (xxxx),a
    "32??"     //ld (xxxx),a
    "32??"     //ld (xxxx),a
    "7e"       //ld a,(hl)
    "23"       //inc hl
    "32??"     //ld (xxxx),a
  ;

  const std::string Version2::FORMAT =
    "c3??"     //jp InitAddr
    "c3??"     //jp PlayAddr
    "??"       //nop,nop
    "'K'S'A' 'S'O'F'T'W'A'R'E' 'C'O'M'P'I'L'A'T'I'O'N' ' ' 'O'F' ' ' "
    "+24+"
    "+8+"
    //+0x48
    "f3"       //di
    "21??"     //ld hl,ModuleAddr
    "22??"     //ld (xxxx),hl
    "3e?"      //ld a,xx
    "32??"     //ld (xxxx),a
    "32??"     //ld (xxxx),a
    "32??"     //ld (xxxx),a
    "7e"       //ld a,(hl)
    "23"       //inc hl
    "32??"     //ld (xxxx),a
  ;
}//CompiledSTP

namespace Formats
{
  namespace Packed
  {
    template<class Version>
    class CompiledSTPDecoder : public Decoder
    {
    public:
      CompiledSTPDecoder()
        : Player(Binary::Format::Create(Version::FORMAT))
        , Decoder(Formats::Chiptune::SoundTrackerPro::CreateCompiledModulesDecoder())
      {
      }

      virtual String GetDescription() const
      {
        return Version::DESCRIPTION;
      }

      virtual Binary::Format::Ptr GetFormat() const
      {
        return Player;
      }

      virtual Container::Ptr Decode(const Binary::Container& rawData) const
      {
        const void* const data = rawData.Data();
        const std::size_t availSize = rawData.Size();
        if (!Player->Match(data, availSize) || availSize < sizeof(typename Version::Player))
        {
          return Container::Ptr();
        }
        const typename Version::Player& rawPlayer = *safe_ptr_cast<const typename Version::Player*>(data);
        const std::size_t playerSize = rawPlayer.GetSize();
        if (playerSize >= std::min(availSize, CompiledSTP::MAX_PLAYER_SIZE))
        {
          Log::Debug(THIS_MODULE, "Invalid player");
          return Container::Ptr();
        }
        Log::Debug(THIS_MODULE, "Detected player in first %1% bytes", playerSize);
        const Binary::Container::Ptr modData = rawData.GetSubcontainer(playerSize, availSize - playerSize);
        const Dump metainfo = rawPlayer.GetInfo();
        if (const Binary::Container::Ptr fixedModule = Decoder->InsertMetainformation(*modData, metainfo))
        {
          if (Decoder->Decode(*fixedModule))
          {
            const std::size_t originalSize = fixedModule->Size() - metainfo.size();
            return CreatePackedContainer(fixedModule, playerSize + originalSize);
          }
          Log::Debug(THIS_MODULE, "Failed to parse fixed module");
        }
        Log::Debug(THIS_MODULE, "Failed to find module after player");
        return Container::Ptr();
      }
    private:
      const Binary::Format::Ptr Player;
      const Formats::Chiptune::SoundTrackerPro::Decoder::Ptr Decoder;
    };

    Decoder::Ptr CreateCompiledSTP1Decoder()
    {
      return boost::make_shared<CompiledSTPDecoder<CompiledSTP::Version1> >();
    }

    Decoder::Ptr CreateCompiledSTP2Decoder()
    {
      return boost::make_shared<CompiledSTPDecoder<CompiledSTP::Version2> >();
    }
  }//namespace Packed
}//namespace Formats