/*
Abstract:
  SQD modules playback support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "dac_base.h"
#include "core/plugins/registrator.h"
#include "core/plugins/utils.h"
#include "core/plugins/players/creation_result.h"
#include "core/plugins/players/module_properties.h"
#include "core/plugins/players/tracking.h"
//common includes
#include <byteorder.h>
#include <error_tools.h>
#include <logging.h>
#include <tools.h>
//library includes
#include <core/convert_parameters.h>
#include <core/core_parameters.h>
#include <core/error_codes.h>
#include <core/module_attrs.h>
#include <core/plugin_attrs.h>
//std includes
#include <set>
#include <utility>
//boost includes
#include <boost/bind.hpp>
//text includes
#include <core/text/core.h>
#include <core/text/plugins.h>
#include <core/text/warnings.h>

#define FILE_TAG 44AA4DF8

namespace SQD
{
  const std::size_t MAX_MODULE_SIZE = 0x4400 + 8 * 0x4000;
  const std::size_t MAX_POSITIONS_COUNT = 100;
  const std::size_t MAX_PATTERN_SIZE = 64;
  const std::size_t PATTERNS_COUNT = 32;
  const std::size_t CHANNELS_COUNT = 4;
  const std::size_t SAMPLES_COUNT = 16;

  //all samples has base freq at 4kHz (C-1)
  const uint_t BASE_FREQ = 4000;

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
  PACK_PRE struct Pattern
  {
    PACK_PRE struct Line
    {
      PACK_PRE struct Channel
      {
        uint8_t NoteCmd;
        uint8_t SampleEffect;

        bool IsEmpty() const
        {
          return 0 == NoteCmd;
        }

        bool IsRest() const
        {
          return 61 == NoteCmd;
        }

        const uint8_t* GetNewTempo() const
        {
          return 62 == NoteCmd
            ? &SampleEffect : 0;
        }

        bool IsEndOfPattern() const
        {
          return 63 == NoteCmd;
        }

        const uint8_t* GetVolumeSlidePeriod() const
        {
          return 64 == NoteCmd
            ? &SampleEffect : 0;
        }

        uint_t GetVolumeSlideDirection() const
        {
          return NoteCmd >> 6;
        }

        uint_t GetNote() const
        {
          return (NoteCmd & 63) - 1;
        }

        uint_t GetSample() const
        {
          return SampleEffect & 15;
        }

        uint_t GetSampleVolume() const
        {
          return SampleEffect >> 4;
        }
      } PACK_POST;

      Channel Channels[CHANNELS_COUNT];
    } PACK_POST;

    Line Lines[MAX_PATTERN_SIZE];
  } PACK_POST;

  PACK_PRE struct SampleInfo
  {
    uint16_t Start;
    uint16_t Loop;
    uint8_t IsLooped;
    uint8_t Bank;
    uint8_t Padding[2];
  } PACK_POST;

  PACK_PRE struct LayoutInfo
  {
    uint16_t Address;
    uint8_t Bank;
    uint8_t Sectors;
  } PACK_POST;

  PACK_PRE struct Header
  {
    //+0
    uint8_t SamplesData[0x80];
    //+0x80
    uint8_t Banks[0x40];
    //+0xc0
    boost::array<LayoutInfo, 8> Layouts;
    //+0xe0
    uint8_t Padding1[0x20];
    //+0x100
    boost::array<char, 0x20> Title;
    //+0x120
    SampleInfo Samples[SAMPLES_COUNT];
    //+0x1a0
    boost::array<uint8_t, MAX_POSITIONS_COUNT> Positions;
    //+0x204
    uint8_t PositionsLimit;
    //+0x205
    uint8_t Padding2[0xb];
    //+0x210
    uint8_t Tempo;
    //+0x211
    uint8_t Loop;
    //+0x212
    uint8_t Length;
    //+0x213
    uint8_t Padding3[0xed];
    //+0x300
    boost::array<uint8_t, 8> SampleNames[SAMPLES_COUNT];
    //+0x380
    uint8_t Padding4[0x80];
    //+0x400
    boost::array<Pattern, PATTERNS_COUNT> Patterns;
    //+0x4400
  } PACK_POST;

#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

  BOOST_STATIC_ASSERT(sizeof(Header) == 0x4400);

  //supported tracking commands
  enum CmdType
  {
    //no parameters
    EMPTY,
    //1 param
    VOLUME_SLIDE_PERIOD,
    //1 param
    VOLUME_SLIDE,
  };

  const std::size_t BIG_SAMPLE_ADDR = 0x8000;
  const std::size_t SAMPLES_ADDR = 0xc000;
  const std::size_t SAMPLES_LIMIT = 0x10000;

  struct Sample
  {
    Dump Data;
    std::size_t Loop;

    Sample()
      : Loop()
    {
    }
  };
}

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  //stub for ornament
  struct VoidType {};

  typedef TrackingSupport<SQD::CHANNELS_COUNT, SQD::CmdType, SQD::Sample, VoidType> SQDTrack;

  // perform module 'playback' right after creating (debug purposes)
  #ifndef NDEBUG
  #define SELF_TEST
  #endif

  Renderer::Ptr CreateSQDRenderer(Parameters::Accessor::Ptr params, Information::Ptr info, SQDTrack::ModuleData::Ptr data, Devices::DAC::Chip::Ptr device);

  class SQDHolder : public Holder
  {
    static void ParsePattern(const SQD::Pattern& src, SQDTrack::Pattern& res)
    {
      SQDTrack::Pattern result;
      bool end = false;
      for (uint_t lineNum = 0; !end && lineNum != SQD::MAX_PATTERN_SIZE; ++lineNum)
      {
        const SQD::Pattern::Line& srcLine = src.Lines[lineNum];
        SQDTrack::Line& dstLine = result.AddLine();
        for (uint_t chanNum = 0; chanNum != SQD::CHANNELS_COUNT; ++chanNum)
        {
          const SQD::Pattern::Line::Channel& srcChan = srcLine.Channels[chanNum];
          if (srcChan.IsEmpty())
          {
            continue;
          }
          else if (srcChan.IsEndOfPattern())
          {
            end = true;
            break;
          }
          else if (const uint8_t* newTempo = srcChan.GetNewTempo())
          {
            dstLine.SetTempo(*newTempo);
            continue;
          }

          SQDTrack::Line::Chan& dstChan = dstLine.Channels[chanNum];
          if (srcChan.IsRest())
          {
            dstChan.SetEnabled(false);
          }
          else
          {
            dstChan.SetEnabled(true);
            dstChan.SetNote(srcChan.GetNote());
            dstChan.SetSample(srcChan.GetSample());
            dstChan.SetVolume(srcChan.GetSampleVolume());
            if (const uint8_t* newPeriod = srcChan.GetVolumeSlidePeriod())
            {
              dstChan.Commands.push_back(SQDTrack::Command(SQD::VOLUME_SLIDE_PERIOD, *newPeriod));
            }
            if (const uint_t slideDirection = srcChan.GetVolumeSlideDirection())
            {
              dstChan.Commands.push_back(SQDTrack::Command(SQD::VOLUME_SLIDE, 1 == slideDirection ? -1 : 1));
            }
          }
        }
      }
      result.Swap(res);
    }

  public:
    SQDHolder(ModuleProperties::RWPtr properties, Binary::Container::Ptr rawData, std::size_t& usedSize)
      : Data(SQDTrack::ModuleData::Create())
      , Properties(properties)
      , Info(CreateTrackInfo(Data, SQD::CHANNELS_COUNT))
    {
      //assume data is correct
      const IO::FastDump& data(*rawData);
      const SQD::Header* const header(safe_ptr_cast<const SQD::Header*>(data.Data()));

      //fill order
      const uint_t positionsCount = header->Length;
      Data->Positions.resize(positionsCount);
      std::copy(header->Positions.begin(), header->Positions.begin() + positionsCount, Data->Positions.begin());

      //fill patterns
      const std::size_t patternsCount = 1 + *std::max_element(Data->Positions.begin(), Data->Positions.end());
      Data->Patterns.resize(patternsCount);
      for (std::size_t patIdx = 0; patIdx < std::min(patternsCount, SQD::PATTERNS_COUNT); ++patIdx)
      {
        ParsePattern(header->Patterns[patIdx], Data->Patterns[patIdx]);
      }

      std::size_t lastData = sizeof(*header);
      //bank => <offset, size>
      typedef std::map<std::size_t, std::pair<std::size_t, std::size_t> > Bank2OffsetAndSize;
      Bank2OffsetAndSize regions;
      for (std::size_t layIdx = 0; layIdx != header->Layouts.size(); ++layIdx)
      {
        const SQD::LayoutInfo& layout = header->Layouts[layIdx];
        const std::size_t addr = fromLE(layout.Address);
        const std::size_t size = 256 * layout.Sectors;
        if (addr >= SQD::BIG_SAMPLE_ADDR && addr + size <= SQD::SAMPLES_LIMIT)
        {
          regions[layout.Bank] = std::make_pair(lastData, size);
        }
        lastData += size;
      }

      //fill samples
      Data->Samples.resize(SQD::SAMPLES_COUNT);
      for (uint_t samIdx = 0; samIdx != SQD::SAMPLES_COUNT; ++samIdx)
      {
        const SQD::SampleInfo& srcSample = header->Samples[samIdx];
        const std::size_t addr = fromLE(srcSample.Start);
        if (addr < SQD::BIG_SAMPLE_ADDR)
        {
          continue;
        }
        const std::size_t sampleBase = addr < SQD::SAMPLES_ADDR
          ? SQD::BIG_SAMPLE_ADDR
          : SQD::SAMPLES_ADDR;
        const std::size_t loop = fromLE(srcSample.Loop);
        if (loop < addr)
        {
          continue;
        }
        const Bank2OffsetAndSize::const_iterator it = regions.find(srcSample.Bank);
        if (it == regions.end())
        {
          continue;
        }
        const std::size_t size = std::min(SQD::SAMPLES_LIMIT - addr, it->second.second);//TODO: get from samples layout
        const std::size_t sampleOffset = it->second.first + addr - sampleBase;
        const uint8_t* const sampleStart = &data[sampleOffset];
        const uint8_t* const sampleEnd = sampleStart + size;
        SQD::Sample& dstSample = Data->Samples[samIdx];
        dstSample.Data.assign(sampleStart, std::find(sampleStart, sampleEnd, 0));
        dstSample.Loop = srcSample.IsLooped ? loop - sampleBase : dstSample.Data.size();
      }
      Data->LoopPosition = header->Loop;
      Data->InitialTempo = header->Tempo;

      usedSize = lastData;

      //meta properties
      {
        const ModuleRegion fixedRegion(offsetof(SQD::Header, Patterns), sizeof(header->Patterns));
        Properties->SetSource(usedSize, fixedRegion);
      }
      const String title = *header->Title.begin() == '|' && *header->Title.rbegin() == '|'
        ? String(header->Title.begin() + 1, header->Title.end() - 1)
        : String(header->Title.begin(), header->Title.end());
      Properties->SetTitle(OptimizeString(title));
      Properties->SetProgram(Text::SQD_EDITOR);
    }

    virtual Plugin::Ptr GetPlugin() const
    {
      return Properties->GetPlugin();
    }

    virtual Information::Ptr GetModuleInformation() const
    {
      return Info;
    }

    virtual Parameters::Accessor::Ptr GetModuleProperties() const
    {
      return Properties;
    }

    virtual Renderer::Ptr CreateRenderer(Parameters::Accessor::Ptr params, Sound::MultichannelReceiver::Ptr target) const
    {
      const uint_t totalSamples = static_cast<uint_t>(Data->Samples.size());

      const Devices::DAC::Receiver::Ptr receiver = DAC::CreateReceiver(target);
      const Devices::DAC::ChipParameters::Ptr chipParams = DAC::CreateChipParameters(params);
      const Devices::DAC::Chip::Ptr chip(Devices::DAC::CreateChip(SQD::CHANNELS_COUNT, totalSamples, SQD::BASE_FREQ, chipParams, receiver));
      for (uint_t idx = 0; idx != totalSamples; ++idx)
      {
        const SQD::Sample& smp(Data->Samples[idx]);
        if (const std::size_t size = smp.Data.size())
        {
          chip->SetSample(idx, smp.Data, smp.Loop);
        }
      }
      return CreateSQDRenderer(params, Info, Data, chip);
    }

    virtual Error Convert(const Conversion::Parameter& spec, Parameters::Accessor::Ptr /*params*/, Dump& dst) const
    {
      using namespace Conversion;
      if (parameter_cast<RawConvertParam>(&spec))
      {
        Properties->GetData(dst);
      }
      else
      {
        return Error(THIS_LINE, ERROR_MODULE_CONVERT, Text::MODULE_ERROR_CONVERSION_UNSUPPORTED);
      }
      return Error();
    }
  private:
    const SQDTrack::ModuleData::RWPtr Data;
    const ModuleProperties::RWPtr Properties;
    const Information::Ptr Info;
  };

  class SQDRenderer : public Renderer
  {
    struct VolumeState
    {
      VolumeState()
        : Value(16)
        , SlideDirection(0)
        , SlideCounter(0)
        , SlidePeriod(0)
      {
      }

      int_t Value;
      int_t SlideDirection;
      uint_t SlideCounter;
      uint_t SlidePeriod;
    };
  public:
    SQDRenderer(Parameters::Accessor::Ptr params, Information::Ptr info, SQDTrack::ModuleData::Ptr data, Devices::DAC::Chip::Ptr device)
      : Data(data)
      , Params(DAC::TrackParameters::Create(params))
      , Device(device)
      , Iterator(CreateTrackStateIterator(info, Data))
      , LastRenderTime(0)
    {
#ifdef SELF_TEST
//perform self-test
      Devices::DAC::DataChunk chunk;
      while (Iterator->IsValid())
      {
        RenderData(chunk);
        Iterator->NextFrame(false);
      }
      Reset();
#endif
    }

    virtual TrackState::Ptr GetTrackState() const
    {
      return Iterator->GetStateObserver();
    }

    virtual Analyzer::Ptr GetAnalyzer() const
    {
      return DAC::CreateAnalyzer(Device);
    }

    virtual bool RenderFrame()
    {
      if (Iterator->IsValid())
      {
        LastRenderTime += Params->FrameDurationMicrosec();
        Devices::DAC::DataChunk chunk;
        RenderData(chunk);
        chunk.TimeInUs = LastRenderTime;
        Device->RenderData(chunk);
        Iterator->NextFrame(Params->Looped());
      }
      return Iterator->IsValid();
    }

    virtual void Reset()
    {
      Device->Reset();
      Iterator->Reset();
      LastRenderTime = 0;
    }

    virtual void SetPosition(uint_t frame)
    {
      const TrackState::Ptr state = Iterator->GetStateObserver();
      if (frame < state->Frame())
      {
        //reset to beginning in case of moving back
        Iterator->Reset();
      }
      //fast forward
      Devices::DAC::DataChunk chunk;
      while (state->Frame() < frame && Iterator->IsValid())
      {
        //do not update tick for proper rendering
        RenderData(chunk);
        Iterator->NextFrame(false);
      }
    }

  private:
    void RenderData(Devices::DAC::DataChunk& chunk)
    {
      std::vector<Devices::DAC::DataChunk::ChannelData> res;
      const TrackState::Ptr state = Iterator->GetStateObserver();
      const SQDTrack::Line* const line = Data->Patterns[state->Pattern()].GetLine(state->Line());
      for (uint_t chan = 0; chan != SQD::CHANNELS_COUNT; ++chan)
      {
        VolumeState& vol = Volumes[chan];
        Devices::DAC::DataChunk::ChannelData dst;
        dst.Channel = chan;
        if (vol.SlideDirection && !--vol.SlideCounter)
        {
          vol.Value += vol.SlideDirection;
          vol.SlideCounter = vol.SlidePeriod;
          if (-1 == vol.SlideDirection && -1 == vol.Value)
          {
            vol.Value = 0;
          }
          else if (vol.Value == 17)
          {
            vol.Value = 16;
          }
          dst.LevelInPercents = 100 * vol.Value / 16;
        }
        //begin note
        if (line && 0 == state->Quirk())
        {
          vol.SlideDirection = 0;
          vol.SlideCounter = 0;

          const SQDTrack::Line::Chan& src = line->Channels[chan];
          Devices::DAC::DataChunk::ChannelData dst;
          dst.Channel = chan;
          if (src.Enabled)
          {
            if (!(dst.Enabled = *src.Enabled))
            {
              dst.PosInSample = 0;
            }
          }
          if (src.Note)
          {
            dst.Note = *src.Note;
            dst.PosInSample = 0;
          }
          if (src.SampleNum)
          {
            dst.SampleNum = *src.SampleNum;
            dst.PosInSample = 0;
          }
          if (src.Volume)
          {
            vol.Value = *src.Volume;
            dst.LevelInPercents = 100 * vol.Value / 16;
          }
          for (SQDTrack::CommandsArray::const_iterator it = src.Commands.begin(), lim = src.Commands.end(); it != lim; ++it)
          {
            switch (it->Type)
            {
            case SQD::VOLUME_SLIDE_PERIOD:
              vol.SlideCounter = vol.SlidePeriod = it->Param1;
              break;
            case SQD::VOLUME_SLIDE:
              vol.SlideDirection = it->Param1;
              break;
            default:
              assert(!"Invalid command");
            }
          }
          //store if smth new
          if (dst.Enabled || dst.Note || dst.SampleNum || dst.PosInSample || dst.LevelInPercents)
          {
            res.push_back(dst);
          }
        }
      }
      chunk.Channels.swap(res);
    }
  private:
    const SQDTrack::ModuleData::Ptr Data;
    const DAC::TrackParameters::Ptr Params;
    const Devices::DAC::Chip::Ptr Device;
    const StateIterator::Ptr Iterator;
    boost::array<VolumeState, SQD::CHANNELS_COUNT> Volumes;
    uint64_t LastRenderTime;
  };

  Renderer::Ptr CreateSQDRenderer(Parameters::Accessor::Ptr params, Information::Ptr info, SQDTrack::ModuleData::Ptr data, Devices::DAC::Chip::Ptr device)
  {
    return Renderer::Ptr(new SQDRenderer(params, info, data, device));
  }

  bool CheckSQD(const Binary::Container& data)
  {
    //check for header
    const std::size_t size(data.Size());
    if (sizeof(SQD::Header) > size)
    {
      return false;
    }
    const SQD::Header* const header(safe_ptr_cast<const SQD::Header*>(data.Data()));
    //check layout
    std::size_t lastData = sizeof(*header);
    std::set<uint_t> banks, bigBanks;
    for (std::size_t layIdx = 0; layIdx != header->Layouts.size(); ++layIdx)
    {
      const SQD::LayoutInfo& layout = header->Layouts[layIdx];
      const std::size_t addr = fromLE(layout.Address);
      const std::size_t size = 256 * layout.Sectors;
      if (addr + size > SQD::SAMPLES_LIMIT)
      {
        continue;
      }
      if (addr >= SQD::SAMPLES_ADDR)
      {
        banks.insert(layout.Bank);
      }
      else if (addr >= SQD::BIG_SAMPLE_ADDR)
      {
        bigBanks.insert(layout.Bank);
      }
      lastData += size;
    }
    if (lastData > size)
    {
      return false;
    }
    if (bigBanks.size() > 1)
    {
      return false;
    }

    //check samples
    for (uint_t samIdx = 0; samIdx != SQD::SAMPLES_COUNT; ++samIdx)
    {
      const SQD::SampleInfo& srcSample = header->Samples[samIdx];
      const std::size_t addr = fromLE(srcSample.Start);
      if (addr < SQD::BIG_SAMPLE_ADDR)
      {
        continue;
      }
      const std::size_t loop = fromLE(srcSample.Loop);
      if (loop < addr)
      {
        return false;
      }
      const bool bigSample = addr < SQD::SAMPLES_ADDR;
      if (!(bigSample ? bigBanks : banks).count(srcSample.Bank))
      {
        return false;
      }
    }
    return true;
  }
}

namespace
{
  using namespace ZXTune;

  //plugin attributes
  const Char ID[] = {'S', 'Q', 'D', 0};
  const Char* const INFO = Text::SQD_PLUGIN_INFO;
  const uint_t CAPS = CAP_STOR_MODULE | CAP_DEV_4DAC | CAP_CONV_RAW;


  const std::string SQD_FORMAT(
    "+192+"
    //layouts
    "(0080-c0 58-5f 01-80){8}"
    "+32+"
    //title
    "20-7f{32}"
    "+128+"
    //positions
    "00-1f{100}"
    "ff"
    "+11+"
    //tempo???
    "02-10"
    //loop
    "00-63"
    //length
    "01-64"
  );

  class SQDModulesFactory : public ModulesFactory
  {
  public:
    SQDModulesFactory()
      : Format(Binary::Format::Create(SQD_FORMAT))
    {
    }

    virtual bool Check(const Binary::Container& inputData) const
    {
      return Format->Match(inputData.Data(), inputData.Size()) && CheckSQD(inputData);
    }

    virtual Binary::Format::Ptr GetFormat() const
    {
      return Format;
    }

    virtual Holder::Ptr CreateModule(ModuleProperties::RWPtr properties, Binary::Container::Ptr data, std::size_t& usedSize) const
    {
      try
      {
        assert(Check(*data));
        const Holder::Ptr holder(new SQDHolder(properties, data, usedSize));
        return holder;
      }
      catch (const Error&/*e*/)
      {
        Log::Debug("Core::SQDSupp", "Failed to create holder");
      }
      return Holder::Ptr();
    }
  private:
    const Binary::Format::Ptr Format;
  };
}

namespace ZXTune
{
  void RegisterSQDSupport(PluginsRegistrator& registrator)
  {
    const ModulesFactory::Ptr factory = boost::make_shared<SQDModulesFactory>();
    const PlayerPlugin::Ptr plugin = CreatePlayerPlugin(ID, INFO, CAPS, factory);
    registrator.RegisterPlugin(plugin);
  }
}