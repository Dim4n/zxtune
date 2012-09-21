/*
Abstract:
  DMM modules playback support

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
#include "core/plugins/players/ay/ay_conversion.h"
//common includes
#include <byteorder.h>
#include <debug_log.h>
#include <error_tools.h>
#include <tools.h>
//library includes
#include <core/convert_parameters.h>
#include <core/core_parameters.h>
#include <core/error_codes.h>
#include <core/module_attrs.h>
#include <core/plugin_attrs.h>
//std includes
#include <utility>
//boost includes
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
//text includes
#include <formats/text/chiptune.h>

#define FILE_TAG 312C703E

namespace
{
  const Debug::Stream Dbg("Core::DMMSupp");
}

namespace DMM
{
  const std::size_t MAX_POSITIONS_COUNT = 0x32;
  const std::size_t MAX_PATTERN_SIZE = 64;
  const std::size_t PATTERNS_COUNT = 24;
  const std::size_t CHANNELS_COUNT = 3;
  const std::size_t SAMPLES_COUNT = 16;//15 really

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
        uint8_t NoteCommand;
        uint8_t SampleParam;
        uint8_t Effect;
      } PACK_POST;

      Channel Channels[CHANNELS_COUNT];
    } PACK_POST;

    Line Lines[1];//at least 1
  } PACK_POST;

  PACK_PRE struct SampleInfo
  {
    uint8_t Name[9];
    uint16_t Start;
    uint8_t Bank;
    uint16_t Limit;
    uint16_t Loop;
  } PACK_POST;

  PACK_PRE struct MixedLine
  {
    Pattern::Line::Channel Mixin;
    uint8_t Period;
  } PACK_POST;

  PACK_PRE struct Header
  {
    //+0
    boost::array<uint16_t, 6> EndOfBanks;
    //+0x0c
    uint8_t PatternSize;
    //+0x0d
    uint8_t Padding1;
    //+0x0e
    boost::array<uint8_t, 0x32> Positions;
    //+0x40
    uint8_t Tempo;
    //+0x41
    uint8_t Loop;
    //+0x42
    uint8_t Padding2;
    //+0x43
    uint8_t Length;
    //+0x44
    uint8_t HeaderSizeSectors;
    //+0x45
    MixedLine Mixings[5];
    //+0x59
    uint8_t Padding3;
    //+0x5a
    SampleInfo SampleDescriptions[SAMPLES_COUNT];
    //+0x15a
    uint8_t Padding4[4];
    //+0x15e
    //patterns starts here
  } PACK_POST;

#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

  BOOST_STATIC_ASSERT(sizeof(MixedLine) == 4);
  BOOST_STATIC_ASSERT(sizeof(SampleInfo) == 16);
  BOOST_STATIC_ASSERT(sizeof(Header) == 0x15e);
  BOOST_STATIC_ASSERT(sizeof(Pattern::Line) == 9);

  //supported tracking commands
  enum CmdType
  {
    //no parameters
    EMPTY_CMD,
    //2 param: direction, step
    FREQ_FLOAT,
    //3 params: isApply, step, period
    VIBRATO,
    //3 params: isApply, step, period
    ARPEGGIO,
    //3 param: direction, step, period
    TONE_SLIDE,
    //2 params: isApply, period
    DOUBLE_NOTE,
    //3 params: isApply, limit, period
    VOL_ATTACK,
    //3 params: isApply, limit, period
    VOL_DECAY,
    //1 param
    MIX_SAMPLE,
  };

  struct Sample
  {
    Sample()
      : Loop()
    {
    }

    Dump Data;
    std::size_t Loop;
  };

  void ConvertFrom4Bit(Dump& data)
  {
    Dump res(data.size() * 2);
    for (std::size_t idx = 0, lim = data.size(); idx != lim; ++idx)
    {
      res[idx * 2] = (data[idx] & 15) << 4;
      res[idx * 2 + 1] = data[idx] & 240;
    }
    data.swap(res);
  }

  enum
  {
    NOTE_BASE = 1,
    NO_DATA = 70,
    REST_NOTE = 61,
    SET_TEMPO = 62,
    SET_FREQ_FLOAT = 63,
    SET_VIBRATO = 64,
    SET_ARPEGGIO = 65,
    SET_SLIDE = 66,
    SET_DOUBLE = 67,
    SET_ATTACK = 68,
    SET_DECAY = 69,

    FX_FLOAT_UP = 1,
    FX_FLOAT_DN = 2,
    FX_VIBRATO = 3,
    FX_ARPEGGIO = 4,
    FX_STEP_UP = 5,
    FX_STEP_DN = 6,
    FX_DOUBLE = 7,
    FX_ATTACK = 8,
    FX_DECAY = 9,
    FX_MIX = 10,
    FX_DISABLE = 15
  };

  const std::size_t SAMPLES_ADDR = 0xc000;

  //stub for ornament
  struct VoidType {};

  typedef ZXTune::Module::TrackingSupport<CHANNELS_COUNT, CmdType, Sample, VoidType> Track;

  class ModuleData : public Track::ModuleData
  {
  public:
    typedef boost::shared_ptr<ModuleData> RWPtr;
    typedef boost::shared_ptr<const ModuleData> Ptr;

    static RWPtr Create()
    {
      return boost::make_shared<ModuleData>();
    }

    struct MixedChannel
    {
      Track::Line::Chan Mixin;
      uint_t Period;

      MixedChannel()
        : Period()
      {
      }
    };

    boost::array<MixedChannel, 64> Mixes;
  };

  void ParseChannel(const Pattern::Line::Channel& srcChan, Track::Line::Chan& dstChan)
  {
    const uint_t note = srcChan.NoteCommand;
    if (NO_DATA == note)
    {
      return;
    }
    if (note < SET_TEMPO)
    {
      if (note)
      {
        if (note != REST_NOTE)
        {
          dstChan.SetEnabled(true);
          dstChan.SetNote(note - NOTE_BASE);
        }
        else
        {
          dstChan.SetEnabled(false);
        }
      }
      const uint_t params = srcChan.SampleParam;
      if (const uint_t sample = params >> 4)
      {
        dstChan.SetSample(sample);
      }
      if (const uint_t volume = params & 15)
      {
        dstChan.SetVolume(volume);
      }
      switch (srcChan.Effect)
      {
      case 0:
        break;
      case FX_FLOAT_UP:
        dstChan.Commands.push_back(Track::Command(FREQ_FLOAT, 1));
        break;
      case FX_FLOAT_DN:
        dstChan.Commands.push_back(Track::Command(FREQ_FLOAT, -1));
        break;
      case FX_VIBRATO:
        dstChan.Commands.push_back(Track::Command(VIBRATO, true));
        break;
      case FX_ARPEGGIO:
        dstChan.Commands.push_back(Track::Command(ARPEGGIO, true));
        break;
      case FX_STEP_UP:
        dstChan.Commands.push_back(Track::Command(TONE_SLIDE, 1));
        break;
      case FX_STEP_DN:
        dstChan.Commands.push_back(Track::Command(TONE_SLIDE, -1));
        break;
      case FX_DOUBLE:
        dstChan.Commands.push_back(Track::Command(DOUBLE_NOTE, true));
        break;
      case FX_ATTACK:
        dstChan.Commands.push_back(Track::Command(VOL_ATTACK, true));
        break;
      case FX_DECAY:
        dstChan.Commands.push_back(Track::Command(VOL_DECAY, true));
        break;
      case FX_DISABLE:
        dstChan.Commands.push_back(Track::Command(EMPTY_CMD));
        break;
      default:
        {
          const uint_t mixNum = srcChan.Effect - FX_MIX;
          //according to player there can be up to 64 mixins (with enabled 4)
          dstChan.Commands.push_back(Track::Command(MIX_SAMPLE, mixNum % 64));
        }
        break; 
      }
    }
    else
    {
      switch (note)
      {
      case SET_TEMPO:
        break;
      case SET_FREQ_FLOAT:
        dstChan.Commands.push_back(Track::Command(FREQ_FLOAT, 0, srcChan.SampleParam));
        break;
      case SET_VIBRATO:
        dstChan.Commands.push_back(Track::Command(VIBRATO, false, srcChan.SampleParam, srcChan.Effect));
        break;
      case SET_ARPEGGIO:
        dstChan.Commands.push_back(Track::Command(ARPEGGIO, false, srcChan.SampleParam, srcChan.Effect));
        break;
      case SET_SLIDE:
        dstChan.Commands.push_back(Track::Command(TONE_SLIDE, 0, srcChan.SampleParam, srcChan.Effect));
        break;
      case SET_DOUBLE:
        dstChan.Commands.push_back(Track::Command(DOUBLE_NOTE, false, srcChan.SampleParam));
        break;
      case SET_ATTACK:
        dstChan.Commands.push_back(Track::Command(VOL_ATTACK, false, srcChan.SampleParam & 15, srcChan.Effect));
        break;
      case SET_DECAY:
        dstChan.Commands.push_back(Track::Command(VOL_DECAY, false, srcChan.SampleParam & 15, srcChan.Effect));
        break;
      }
    }
  }
}

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  // perform module 'playback' right after creating (debug purposes)
  #ifndef NDEBUG
  #define SELF_TEST
  #endif

  Renderer::Ptr CreateDMMRenderer(Parameters::Accessor::Ptr params, Information::Ptr info, DMM::ModuleData::Ptr data, Devices::DAC::Chip::Ptr device);

  class DMMHolder : public Holder
  {
    static void ParsePattern(uint_t size, const DMM::Pattern& src, DMM::Track::Pattern& res)
    {
      DMM::Track::Pattern result;
      for (uint_t lineNum = 0; lineNum != size; ++lineNum)
      {
        const DMM::Pattern::Line& srcLine = src.Lines[lineNum];
        DMM::Track::Line& dstLine = result.AddLine();
        for (uint_t chanNum = 0; chanNum != DMM::CHANNELS_COUNT; ++chanNum)
        {
          const DMM::Pattern::Line::Channel& srcChan = srcLine.Channels[chanNum];
          DMM::Track::Line::Chan& dstChan = dstLine.Channels[chanNum];
          DMM::ParseChannel(srcChan, dstChan);
          if (srcChan.NoteCommand == DMM::SET_TEMPO && srcChan.SampleParam)
          {
            dstLine.SetTempo(srcChan.SampleParam);
          }
        }
      }
      result.Swap(res);
    }

  public:
    DMMHolder(ModuleProperties::RWPtr properties, Binary::Container::Ptr rawData, std::size_t& usedSize)
      : Data(DMM::ModuleData::Create())
      , Properties(properties)
      , Info(CreateTrackInfo(Data, DMM::CHANNELS_COUNT))
    {
      //assume data is correct
      const IO::FastDump& data(*rawData);
      const DMM::Header* const header(safe_ptr_cast<const DMM::Header*>(data.Data()));

      //fill order
      const uint_t positionsCount = header->Length + 1;
      Data->Positions.resize(positionsCount);
      std::copy(header->Positions.begin(), header->Positions.begin() + positionsCount, Data->Positions.begin());

      //fill patterns
      const std::size_t patternsCount = 1 + *std::max_element(Data->Positions.begin(), Data->Positions.end());
      const uint_t patternSize = header->PatternSize;
      {
        Data->Patterns.resize(patternsCount);
        for (std::size_t patIdx = 0; patIdx < std::min(patternsCount, DMM::PATTERNS_COUNT); ++patIdx)
        {
          const DMM::Pattern* const pattern = safe_ptr_cast<const DMM::Pattern*>(header + 1) + patIdx * patternSize;
          ParsePattern(patternSize, *pattern, Data->Patterns[patIdx]);
        }
      }

      //big mixins amount support
      for (std::size_t mixIdx = 0; mixIdx != 64; ++mixIdx)
      {
        const DMM::MixedLine& src = header->Mixings[mixIdx];
        DMM::ModuleData::MixedChannel& dst = Data->Mixes[mixIdx];
        ParseChannel(src.Mixin, dst.Mixin);
        dst.Period = src.Period;
      }

      const bool is4bitSamples = true;//TODO: detect
      std::size_t lastData = 256 * header->HeaderSizeSectors;

      //bank => <offset, size>
      typedef std::map<std::size_t, Dump> Bank2Data;
      Bank2Data regions;
      for (std::size_t layIdx = 0; layIdx != header->EndOfBanks.size(); ++layIdx)
      {
        static const std::size_t BANKS[] = {0x50, 0x51, 0x53, 0x54, 0x56, 0x57};

        const uint_t bankNum = BANKS[layIdx];
        const std::size_t bankEnd = fromLE(header->EndOfBanks[layIdx]);
        if (bankEnd <= DMM::SAMPLES_ADDR)
        {
          Dbg("Skipping bank #%1$02x (end=#%2$04x)", bankNum, bankEnd);
          continue;
        }
        const std::size_t bankSize = bankEnd - DMM::SAMPLES_ADDR;
        const std::size_t alignedBankSize = align<std::size_t>(bankSize, 256);
        const std::size_t realSize = is4bitSamples
          ? 256 * (1 + alignedBankSize / 512)
          : alignedBankSize;
        const uint8_t* const bankStart = &data[lastData];
        regions[bankNum] = Dump(bankStart, bankStart + realSize);
        if (is4bitSamples)
        {
          DMM::ConvertFrom4Bit(regions[bankNum]);
        }
        Dbg("Added bank #%1$02x (end=#%2$04x, size=#%3$04x) offset=#%4$05x", bankNum, bankEnd, realSize, lastData);
        lastData += realSize;
      }

      Data->Samples.resize(DMM::SAMPLES_COUNT);
      for (uint_t samIdx = 1; samIdx != DMM::SAMPLES_COUNT; ++samIdx)
      {
        const DMM::SampleInfo& srcSample = header->SampleDescriptions[samIdx - 1];
        if (srcSample.Name[0] == '.')
        {
          Dbg("No sample %1%", samIdx);
          continue;
        }
        const std::size_t sampleStart = fromLE(srcSample.Start);
        const std::size_t sampleEnd = fromLE(srcSample.Limit);
        const std::size_t sampleLoop = fromLE(srcSample.Loop);
        Dbg("Processing sample %1% (bank #%2$02x #%3$04x..#%4$04x loop #%5$04x)", samIdx, uint_t(srcSample.Bank), sampleStart, sampleEnd, sampleLoop);
        if (sampleStart < DMM::SAMPLES_ADDR ||
            sampleStart > sampleEnd ||
            sampleStart > sampleLoop)
        {
          Dbg("Skipped due to invalid layout");
          continue;
        }
        if (!regions.count(srcSample.Bank))
        {
          Dbg("Skipped. No data");
          continue;
        }
        const Dump& bankData = regions[srcSample.Bank];
        const std::size_t offsetInBank = sampleStart - DMM::SAMPLES_ADDR;
        const std::size_t limitInBank = sampleEnd - DMM::SAMPLES_ADDR;
        const std::size_t sampleSize = limitInBank - offsetInBank;
        if (limitInBank > bankData.size())
        {
          Dbg("Skipped. Not enough data");
          continue;
        }
        const uint8_t* const sampleDataStart = &bankData[offsetInBank];
        const uint8_t* const sampleDataEnd = sampleDataStart + sampleSize;
        DMM::Sample& dstSample = Data->Samples[samIdx];
        dstSample.Data.assign(sampleDataStart, sampleDataEnd);
        if (dstSample.Data.size() >= 12)
        {
          dstSample.Data.resize(dstSample.Data.size() - 12);
        }
        dstSample.Loop = sampleLoop - sampleStart;
      }
      Data->LoopPosition = header->Loop;
      Data->InitialTempo = header->Tempo;

      usedSize = lastData;

      //meta properties
      {
        const ModuleRegion fixedRegion(sizeof(*header), sizeof(DMM::Pattern::Line) * patternsCount * patternSize);
        Properties->SetSource(usedSize, fixedRegion);
      }
      Properties->SetProgram(Text::DIGITALMUSICMAKER_DECODER_DESCRIPTION);
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

      const Devices::DAC::Receiver::Ptr receiver = DAC::CreateReceiver(target, DMM::CHANNELS_COUNT);
      const Devices::DAC::ChipParameters::Ptr chipParams = DAC::CreateChipParameters(params);
      const Devices::DAC::Chip::Ptr chip(Devices::DAC::CreateChip(DMM::CHANNELS_COUNT, totalSamples, DMM::BASE_FREQ, chipParams, receiver));
      for (uint_t idx = 0; idx != totalSamples; ++idx)
      {
        const DMM::Sample& smp(Data->Samples[idx]);
        if (const std::size_t size = smp.Data.size())
        {
          chip->SetSample(idx, smp.Data, smp.Loop);
        }
      }
      return CreateDMMRenderer(params, Info, Data, chip);
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
        return CreateUnsupportedConversionError(THIS_LINE, spec);
      }
      return Error();
    }
  private:
    const DMM::ModuleData::RWPtr Data;
    const ModuleProperties::RWPtr Properties;
    const Information::Ptr Info;
  };

  class DMMRenderer : public Renderer
  {
  public:
    DMMRenderer(Parameters::Accessor::Ptr params, Information::Ptr info, DMM::ModuleData::Ptr data, Devices::DAC::Chip::Ptr device)
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
      std::fill(Chans.begin(), Chans.end(), ChannelState());
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
    //TODO: is there standard function?
    template<class T>
    static void CopyValue(const boost::optional<T>& lh, boost::optional<T>& rh)
    {
      if (lh)
      {
        rh = *lh;
      }
    }

    class ChannelState
    {
    public:
      ChannelState()
        //values are from player' defaults
        : FreqSlideStep(1)
        , VibratoPeriod(4)
        , VibratoStep(3)
        , ArpeggioPeriod(1)
        , ArpeggioStep(18)
        , NoteSlidePeriod(2)
        , NoteSlideStep(12)
        , NoteDoublePeriod(3)
        , AttackPeriod(1)
        , AttackLimit(15)
        , DecayPeriod(1)
        , DecayLimit(1)
        , MixPeriod(3)
        , Counter(0)
        , Note(0)
        , NoteSlide(0)
        , FreqSlide(0)
        , Volume(15)
        , Sample(0)
        , Effect(&ChannelState::NoEffect)
      {
      }

      void OnFrame(Devices::DAC::DataChunk::ChannelData& dst)
      {
        (this->*Effect)(dst);
      }

      void OnNote(const DMM::Track::Line::Chan& src, const DMM::ModuleData& data, Devices::DAC::DataChunk::ChannelData& dst)
      {
        //if has new sample, start from it, else use previous sample
        const uint_t oldPos = src.SampleNum ? 0 : *dst.PosInSample;
        ParseNote(src, dst);
        if (src.Commands.empty())
        {
          return;
        }
        OldData = src;
        OldData.Commands.clear();
        for (DMM::Track::CommandsArray::const_iterator it = src.Commands.begin(), lim = src.Commands.end(); it != lim; ++it)
        {
          switch (it->Type)
          {
          case DMM::EMPTY_CMD:
            DisableEffect();
            break;
          case DMM::FREQ_FLOAT:
            if (it->Param1)
            {
              Effect = &ChannelState::FreqFloat;
              FreqSlideStep = it->Param1;
            }
            else
            {
              FreqSlideStep *= it->Param2;
            }
            break;
          case DMM::VIBRATO:
            if (it->Param1)
            {
              Effect = &ChannelState::Vibrato;
            }
            else
            {
              VibratoStep = it->Param2;
              VibratoPeriod = it->Param3;
            }
            break;
          case DMM::ARPEGGIO:
            if (it->Param1)
            {
              Effect = &ChannelState::Arpeggio;
            }
            else
            {
              ArpeggioStep = it->Param2;
              ArpeggioPeriod = it->Param3;
            }
            break;
          case DMM::TONE_SLIDE:
            if (it->Param1)
            {
              Effect = &ChannelState::NoteFloat;
              NoteSlideStep = it->Param1;
            }
            else
            {
              NoteSlideStep *= it->Param2;
              NoteSlidePeriod = it->Param3;
            }
            break;
          case DMM::DOUBLE_NOTE:
            if (it->Param1)
            {
              Effect = &ChannelState::DoubleNote;
            }
            else
            {
              NoteDoublePeriod = it->Param2;
            }
            break;
          case DMM::VOL_ATTACK:
            if (it->Param1)
            {
              Effect = &ChannelState::Attack;
            }
            else
            {
              AttackLimit = it->Param2;
              AttackPeriod = it->Param3;
            }
            break;
          case DMM::VOL_DECAY:
            if (it->Param1)
            {
              Effect = &ChannelState::Decay;
            }
            else
            {
              DecayLimit = it->Param2;
              DecayPeriod = it->Param3;
            }
            break;
          case DMM::MIX_SAMPLE:
            {
              DacState = dst;
              DacState.PosInSample = oldPos;
              const DMM::ModuleData::MixedChannel& mix = data.Mixes[it->Param1];
              ParseNote(mix.Mixin, dst);
              MixPeriod = mix.Period;
              Effect = &ChannelState::Mix;
            }
            break;
          }
        }
      }
    private:
      void ParseNote(const DMM::Track::Line::Chan& src, Devices::DAC::DataChunk::ChannelData& dst)
      {
        if (src.Note)
        {
          Counter = 0;
          VibratoStep = ArpeggioStep = 0;
          Note = *src.Note;
          NoteSlide = FreqSlide = 0;
        }
        if (src.Volume)
        {
          Volume = *src.Volume;
        }
        dst.Enabled = src.Enabled;
        if (dst.Enabled && !*dst.Enabled)
        {
          NoteSlide = FreqSlide = 0;
        }
        if (src.SampleNum)
        {
          Sample = *src.SampleNum;
          dst.PosInSample = 0;
        }
        else
        {
          dst.PosInSample = boost::optional<uint_t>();
        }

        dst.Note = Note;
        dst.NoteSlide = NoteSlide;
        //FreqSlide in 1/256 steps
        //step 44 is C-1@3.5Mhz AY
        //C-1 is 32.7 Hz
        dst.FreqSlideHz = FreqSlide * 327 / 440;
        dst.SampleNum = Sample;
        dst.LevelInPercents = 100 * Volume / 15;
      }

      void NoEffect(Devices::DAC::DataChunk::ChannelData& /*dst*/)
      {
      }

      void FreqFloat(Devices::DAC::DataChunk::ChannelData& /*dst*/)
      {
        SlideFreq(FreqSlideStep);
      }

      void Vibrato(Devices::DAC::DataChunk::ChannelData& /*dst*/)
      {
        if (Step(VibratoPeriod))
        {
          VibratoStep = -VibratoStep;
          SlideFreq(VibratoStep);
        }
      }

      void Arpeggio(Devices::DAC::DataChunk::ChannelData& /*dst*/)
      {
        if (Step(ArpeggioPeriod))
        {
          ArpeggioStep = -ArpeggioStep;
          NoteSlide += ArpeggioStep;
          FreqSlide = 0;
        }
      }

      void NoteFloat(Devices::DAC::DataChunk::ChannelData& /*dst*/)
      {
        if (Step(NoteSlidePeriod))
        {
          NoteSlide += NoteSlideStep;
          FreqSlide = 0;
        }
      }

      void DoubleNote(Devices::DAC::DataChunk::ChannelData& dst)
      {
        if (Step(NoteDoublePeriod))
        {
          ParseNote(OldData, dst);
          DisableEffect();
        }
      }

      void Attack(Devices::DAC::DataChunk::ChannelData& /*dst*/)
      {
        if (Step(AttackPeriod))
        {
          ++Volume;
          if (AttackLimit < Volume)
          {
            --Volume;
            DisableEffect();
          }
        }
      }

      void Decay(Devices::DAC::DataChunk::ChannelData& /*dst*/)
      {
        if (Step(DecayPeriod))
        {
          --Volume;
          if (DecayLimit > Volume)
          {
            ++Volume;
            DisableEffect();
          }
        }
      }

      void Mix(Devices::DAC::DataChunk::ChannelData& dst)
      {
        if (Step(MixPeriod))
        {
          ParseNote(OldData, dst);
          dst = DacState;
          //restore all
          const uint_t prevStep = GetStep() + FreqSlide;
          /*
            Player: 353 ticks/cycle, 9915 cycles/sec, 3305 cycles/chan
            C-1 = 44 * 3305 / 256 = 568Hz
          */
          const uint_t RENDERS_PER_SEC = 3305;
          const uint_t FPS = 50;//TODO
          const uint_t skipped = MixPeriod * prevStep * RENDERS_PER_SEC / (256 * FPS);
          dst.PosInSample = *dst.PosInSample + skipped;

          DisableEffect();
        }
      }

      bool Step(uint_t period)
      {
        if (++Counter == period)
        {
          Counter = 0;
          return true;
        }
        return false;
      }

      void SlideFreq(int_t step)
      {
        const int_t nextStep = GetStep() + FreqSlide + step;
        if (nextStep <= 0 || nextStep >= 0x0c00)
        {
          DisableEffect();
        }
        else
        {
          FreqSlide += step;
          NoteSlide = 0;
        }
      }

      uint_t GetStep() const
      {
        static const uint_t STEPS[] =
        {
          44, 47, 50, 53, 56, 59, 63, 66, 70, 74, 79, 83,
          88, 94, 99, 105, 111, 118, 125, 133, 140, 149, 158, 167,
          177, 187, 199, 210, 223, 236, 250, 265, 281, 297, 315, 334,
          354, 375, 397, 421, 446, 472, 500, 530, 561, 595, 630, 668,
          707, 749, 794, 841, 891, 944, 1001, 1060, 1123, 1189, 1216, 1335
        };
        return STEPS[Note + NoteSlide];
      }

      void DisableEffect()
      {
        Effect = &ChannelState::NoEffect;
      }
    private:
      int_t FreqSlideStep;

      uint_t VibratoPeriod;//VBT_x
      int_t VibratoStep;//VBF_x * VBA1/VBA2
      
      uint_t ArpeggioPeriod;//APT_x
      int_t ArpeggioStep;//APF_x * APA1/APA2

      uint_t NoteSlidePeriod;//SUT_x/SDT_x
      int_t NoteSlideStep;

      uint_t NoteDoublePeriod;//DUT_x

      uint_t AttackPeriod;//ATT_x
      uint_t AttackLimit;//ATL_x

      uint_t DecayPeriod;//DYT_x
      uint_t DecayLimit;//DYL_x

      uint_t MixPeriod;

      uint_t Counter;//COUN_x
      uint_t Note;  //NOTN_x
      uint_t NoteSlide;
      uint_t FreqSlide;
      uint_t Volume;//pVOL_x
      uint_t Sample;

      DMM::Track::Line::Chan OldData;
      Devices::DAC::DataChunk::ChannelData DacState;

      typedef void (ChannelState::*EffectFunc)(Devices::DAC::DataChunk::ChannelData&);
      EffectFunc Effect;
    };

    void RenderData(Devices::DAC::DataChunk& chunk)
    {
      std::vector<Devices::DAC::DataChunk::ChannelData> res;
      const TrackState::Ptr state = Iterator->GetStateObserver();
      const DMM::Track::Line* const line = Data->Patterns[state->Pattern()].GetLine(state->Line());
      for (uint_t chan = 0; chan != DMM::CHANNELS_COUNT; ++chan)
      {
        Devices::DAC::DataChunk::ChannelData dst;
        Device->GetChannelState(chan, dst);

        ChannelState& chanState = Chans[chan];
        chanState.OnFrame(dst);
        //begin note
        if (line && 0 == state->Quirk())
        {
          const DMM::Track::Line::Chan& src = line->Channels[chan];
          chanState.OnNote(src, *Data, dst);
        }
        //store if smth new
        dst.Channel = chan;
        res.push_back(dst);
      }
      chunk.Channels.swap(res);
    }
  private:
    const DMM::ModuleData::Ptr Data;
    const DAC::TrackParameters::Ptr Params;
    const Devices::DAC::Chip::Ptr Device;
    const StateIterator::Ptr Iterator;
    boost::array<ChannelState, DMM::CHANNELS_COUNT> Chans;
    uint64_t LastRenderTime;
  };

  Renderer::Ptr CreateDMMRenderer(Parameters::Accessor::Ptr params, Information::Ptr info, DMM::ModuleData::Ptr data, Devices::DAC::Chip::Ptr device)
  {
    return Renderer::Ptr(new DMMRenderer(params, info, data, device));
  }

  bool CheckDMM(const Binary::Container& data)
  {
    //check for header
    const std::size_t size(data.Size());
    if (sizeof(DMM::Header) > size)
    {
      return false;
    }
    const DMM::Header* const header(safe_ptr_cast<const DMM::Header*>(data.Start()));
    if (!(header->PatternSize == 64 || header->PatternSize == 48 || header->PatternSize == 32 || header->PatternSize == 24))
    {
      return false;
    }

    const bool is4bitSamples = true;//TODO: detect
    std::size_t lastData = 256 * header->HeaderSizeSectors;

    typedef std::map<std::size_t, std::pair<std::size_t, std::size_t> > Bank2OffsetAndSize;
    Bank2OffsetAndSize regions;
    for (std::size_t layIdx = 0; layIdx != header->EndOfBanks.size(); ++layIdx)
    {
      static const std::size_t BANKS[] = {0x50, 0x51, 0x53, 0x54, 0x56, 0x57};

      const std::size_t bankEnd = fromLE(header->EndOfBanks[layIdx]);
      if (bankEnd < DMM::SAMPLES_ADDR)
      {
        return false;
      }
      if (bankEnd == DMM::SAMPLES_ADDR)
      {
        continue;
      }
      const std::size_t bankSize = bankEnd - DMM::SAMPLES_ADDR;
      const std::size_t alignedBankSize = align<std::size_t>(bankSize, 256);
      const std::size_t realSize = is4bitSamples
        ? 256 * (1 + alignedBankSize / 512)
        : alignedBankSize;
      regions[BANKS[layIdx]] = std::make_pair(lastData, realSize);
      lastData += realSize;
    }
    if (lastData > size)
    {
      return false;
    }

    for (uint_t samIdx = 0; samIdx != DMM::SAMPLES_COUNT; ++samIdx)
    {
      const DMM::SampleInfo& srcSample = header->SampleDescriptions[samIdx];
      if (srcSample.Name[0] == '.')
      {
        continue;
      }
      const std::size_t sampleStart = fromLE(srcSample.Start);
      const std::size_t sampleEnd = fromLE(srcSample.Limit);
      const std::size_t sampleLoop = fromLE(srcSample.Loop);
      if (sampleStart < DMM::SAMPLES_ADDR ||
          sampleStart > sampleEnd ||
          sampleStart > sampleLoop)
      {
        return false;
      }
      if (!regions.count(srcSample.Bank))
      {
        return false;
      }
      const std::size_t offsetInBank = sampleStart - DMM::SAMPLES_ADDR;
      const std::size_t limitInBank = sampleEnd - DMM::SAMPLES_ADDR;
      const std::size_t sampleSize = limitInBank - offsetInBank;
      const std::size_t rawSampleSize = is4bitSamples ? sampleSize / 2 : sampleSize;
      if (rawSampleSize > regions[srcSample.Bank].second)
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
  const Char ID[] = {'D', 'M', 'M', 0};
  const Char* const INFO = Text::DIGITALMUSICMAKER_DECODER_DESCRIPTION;
  const uint_t CAPS = CAP_STOR_MODULE | CAP_DEV_3DAC | CAP_CONV_RAW;

  const std::string DMM_FORMAT(
    //bank ends
    "(?c0-ff){6}"
    //pat size: 64,48,32,24
    "%0xxxx000 ?"
    //positions
    "(00-17){50}"
    //tempo (3..30)
    "03-1e"
    //loop position
    "00-32 ?"
    //length
    "01-32"
    //base size
    "02-38"
  );

  class DMMModulesFactory : public ModulesFactory
  {
  public:
    DMMModulesFactory()
      : Format(Binary::Format::Create(DMM_FORMAT, sizeof(DMM::Header)))
    {
    }

    virtual bool Check(const Binary::Container& inputData) const
    {
      return Format->Match(inputData) && CheckDMM(inputData);
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
        const Holder::Ptr holder(new DMMHolder(properties, data, usedSize));
        return holder;
      }
      catch (const Error&/*e*/)
      {
        Dbg("Failed to create holder");
      }
      return Holder::Ptr();
    }
  private:
    const Binary::Format::Ptr Format;
  };
}

namespace ZXTune
{
  void RegisterDMMSupport(PluginsRegistrator& registrator)
  {
    const ModulesFactory::Ptr factory = boost::make_shared<DMMModulesFactory>();
    const PlayerPlugin::Ptr plugin = CreatePlayerPlugin(ID, INFO, CAPS, factory);
    registrator.RegisterPlugin(plugin);
  }
}
