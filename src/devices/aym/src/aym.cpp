/*
Abstract:
  AY/YM chips interface implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  Based on sources of UnrealSpeccy by SMT and Xpeccy sources by SamStyle
*/

//local includes
#include "device.h"
//common includes
#include <tools.h>
//library includes
#include <math/fixedpoint.h>
#include <time/oscillator.h>
//std includes
#include <cassert>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
//boost includes
#include <boost/scoped_ptr.hpp>

namespace
{
  using namespace Devices::AYM;

  enum
  {
    // set of registers which required input data masking (4 or 5 lsb)
    REGS_4BIT_SET = (1 << DataChunk::REG_TONEA_H) | (1 << DataChunk::REG_TONEB_H) |
                    (1 << DataChunk::REG_TONEC_H) | (1 << DataChunk::REG_ENV) | (1 << DataChunk::REG_BEEPER),
    REGS_5BIT_SET = (1 << DataChunk::REG_TONEN) | (1 << DataChunk::REG_VOLA) |
                    (1 << DataChunk::REG_VOLB) | (1 << DataChunk::REG_VOLC),
  };

  BOOST_STATIC_ASSERT(DataChunk::REG_LAST < 8 * sizeof(uint_t));

  // chip-specific volume tables- ym supports 32 volume steps, ay - only 16
  const VolTable AYVolumeTab =
  { {
    0x0000, 0x0000, 0x0340, 0x0340, 0x04C0, 0x04C0, 0x06F2, 0x06F2,
    0x0A44, 0x0A44, 0x0F13, 0x0F13, 0x1510, 0x1510, 0x227E, 0x227E,
    0x289F, 0x289F, 0x414E, 0x414E, 0x5B21, 0x5B21, 0x7258, 0x7258,
    0x905E, 0x905E, 0xB550, 0xB550, 0xD7A0, 0xD7A0, 0xFFFF, 0xFFFF
  } };
  const VolTable YMVolumeTab =
  { {
    0x0000, 0x0000, 0x00EF, 0x01D0, 0x0290, 0x032A, 0x03EE, 0x04D2,
    0x0611, 0x0782, 0x0912, 0x0A36, 0x0C31, 0x0EB6, 0x1130, 0x13A0,
    0x1751, 0x1BF5, 0x20E2, 0x2594, 0x2CA1, 0x357F, 0x3E45, 0x475E,
    0x5502, 0x6620, 0x7730, 0x8844, 0xA1D2, 0xC102, 0xE0A2, 0xFFFF
  } };

  typedef boost::array<uint_t, Devices::AYM::CHANNELS> LayoutData;

  const LayoutData LAYOUTS[] =
  {
    { {0, 1, 2} }, //ABC
    { {0, 2, 1} }, //ACB
    { {1, 0, 2} }, //BAC
    { {1, 2, 0} }, //BCA
    { {2, 1, 0} }, //CBA
    { {2, 0, 1} }, //CAB
  };

  const uint_t AYM_CLOCK_DIVISOR = 8;

  class Renderer
  {
  public:
    virtual ~Renderer() {}

    virtual void GetLevels(MultiSample& result) const = 0;
  };

  class AYMRenderer : public Renderer
  {
  public:
    AYMRenderer()
      : Mixer(State.Data[DataChunk::REG_MIXER])
      , VolA(State.Data[DataChunk::REG_VOLA]), VolB(State.Data[DataChunk::REG_VOLB]), VolC(State.Data[DataChunk::REG_VOLC])
    {
      Mixer = 0xff;
    }

    void SetVolumeTable(const VolTable& table)
    {
      Device.SetVolumeTable(table);
    }

    void SetDutyCycle(uint_t value, uint_t mask)
    {
      Device.SetDutyCycle(value, mask);
    }

    void Reset()
    {
      State = DataChunk();
      Mixer = 0xff;
      Device.Reset();
    }

    void SetNewData(const DataChunk& data)
    {
      for (uint_t idx = 0, mask = 1; idx != data.Data.size(); ++idx, mask <<= 1)
      {
        if (0 == (data.Mask & mask))
        {
          //no new data
          continue;
        }
        //copy registers
        uint8_t reg = data.Data[idx];
        //limit values
        if (mask & REGS_4BIT_SET)
        {
          reg &= 0x0f;
        }
        else if (mask & REGS_5BIT_SET)
        {
          reg &= 0x1f;
        }
        State.Data[idx] = reg;
      }
      if (data.Mask & (1 << DataChunk::REG_MIXER))
      {
        Device.SetMixer(Mixer);
      }
      if (data.Mask & ((1 << DataChunk::REG_TONEA_L) | (1 << DataChunk::REG_TONEA_H) |
                       (1 << DataChunk::REG_TONEB_L) | (1 << DataChunk::REG_TONEB_H) |
                       (1 << DataChunk::REG_TONEC_L) | (1 << DataChunk::REG_TONEC_H) |
                       (1 << DataChunk::REG_TONEN) |
                       (1 << DataChunk::REG_TONEE_L) | (1 << DataChunk::REG_TONEE_H)
                       ))
      {
        Device.SetPeriods(GetToneA(), GetToneB(), GetToneC(), GetToneN(), GetToneE());
      }
      if (data.Mask & (1 << DataChunk::REG_ENV))
      {
        Device.SetEnvType(GetEnvType());
      }
      if (data.Mask & ((1 << DataChunk::REG_VOLA) | (1 << DataChunk::REG_VOLB) | (1 << DataChunk::REG_VOLC)))
      {
        Device.SetLevel(VolA, VolB, VolC);
      }
    }

    void Tick(uint_t ticks)
    {
      Device.Tick(ticks);
    }

    virtual void GetLevels(MultiSample& result) const
    {
      return Device.GetLevels(result);
    }

    void GetState(uint64_t ticksPerSec, ChannelsState& state) const
    {
      const uint_t MAX_LEVEL = 100;
      //one channel is noise
      ChanState& noiseChan = state[CHANNELS];
      noiseChan = ChanState('N');
      noiseChan.Band = GetBandByPeriod(ticksPerSec, GetToneN());
      //one channel is envelope    
      ChanState& envChan = state[CHANNELS + 1];
      envChan = ChanState('E');
      envChan.Band = GetBandByPeriod(ticksPerSec, 16 * GetToneE());
      const uint_t mixer = ~Mixer;
      for (uint_t chan = 0; chan != CHANNELS; ++chan) 
      {
        const uint_t volReg = State.Data[DataChunk::REG_VOLA + chan];
        const bool hasNoise = 0 != (mixer & (uint_t(DataChunk::REG_MASK_NOISEA) << chan));
        const bool hasTone = 0 != (mixer & (uint_t(DataChunk::REG_MASK_TONEA) << chan));
        const bool hasEnv = 0 != (volReg & DataChunk::REG_MASK_ENV);
        //accumulate level in noise channel
        if (hasNoise)
        {
          noiseChan.Enabled = true;
          noiseChan.LevelInPercents += MAX_LEVEL / CHANNELS;
        }
        //accumulate level in envelope channel      
        if (hasEnv)
        {        
          envChan.Enabled = true;
          envChan.LevelInPercents += MAX_LEVEL / CHANNELS;
        }
        //calculate tone channel
        ChanState& channel = state[chan];
        channel.Name = static_cast<Char>('A' + chan);
        if (hasTone)
        {
          channel.Enabled = true;
          channel.LevelInPercents = (volReg & DataChunk::REG_MASK_VOL) * MAX_LEVEL / 15;
          const uint_t chanTone = 256 * State.Data[DataChunk::REG_TONEA_H + chan * 2] +
            State.Data[DataChunk::REG_TONEA_L + chan * 2];
          channel.Band = GetBandByPeriod(ticksPerSec, chanTone);
        }
      } 
    }

  private:
    static uint_t GetBandByPeriod(uint64_t ticksPerSec, uint_t period)
    {
      const uint_t FREQ_MULTIPLIER = 100;
      //table in Hz * FREQ_MULTIPLIER
      static const uint_t FREQ_TABLE[] =
      {
        //octave1
        3270,   3465,   3671,   3889,   4120,   4365,   4625,   4900,   5191,   5500,   5827,   6173,
        //octave2
        6541,   6929,   7342,   7778,   8241,   8730,   9250,   9800,  10382,  11000,  11654,  12346,
        //octave3
        13082,  13858,  14684,  15556,  16482,  17460,  18500,  19600,  20764,  22000,  23308,  24692,
        //octave4
        26164,  27716,  29368,  31112,  32964,  34920,  37000,  39200,  41528,  44000,  46616,  49384,
        //octave5
        52328,  55432,  58736,  62224,  65928,  69840,  74000,  78400,  83056,  88000,  93232,  98768,
        //octave6
        104650, 110860, 117470, 124450, 131860, 139680, 148000, 156800, 166110, 176000, 186460, 197540,
        //octave7
        209310, 221720, 234940, 248890, 263710, 279360, 296000, 313600, 332220, 352000, 372930, 395070,
        //octave8
        418620, 443460, 469890, 497790, 527420, 558720, 592000, 627200, 664450, 704000, 745860, 790140,
        //octave9
        837200, 886980, 939730, 995610,1054800,1117500,1184000,1254400,1329000,1408000,1491700,1580400
      };
      const uint_t freq = static_cast<uint_t>(ticksPerSec * FREQ_MULTIPLIER / (2 * AYM_CLOCK_DIVISOR * (period ? period : 1)));
      const uint_t maxBand = static_cast<uint_t>(ArraySize(FREQ_TABLE) - 1);
      const uint_t currentBand = static_cast<uint_t>(std::lower_bound(FREQ_TABLE, ArrayEnd(FREQ_TABLE), freq) - FREQ_TABLE);
      return std::min(currentBand, maxBand);
    }
  private:
    uint_t GetToneA() const
    {
      return 256 * State.Data[DataChunk::REG_TONEA_H] + State.Data[DataChunk::REG_TONEA_L];
    }

    uint_t GetToneB() const
    {
      return 256 * State.Data[DataChunk::REG_TONEB_H] + State.Data[DataChunk::REG_TONEB_L];
    }

    uint_t GetToneC() const
    {
      return 256 * State.Data[DataChunk::REG_TONEC_H] + State.Data[DataChunk::REG_TONEC_L];
    }

    uint_t GetToneN() const
    {
      return 2 * State.Data[DataChunk::REG_TONEN];//for optimization
    }

    uint_t GetToneE() const
    {
      return 256 * State.Data[DataChunk::REG_TONEE_H] + State.Data[DataChunk::REG_TONEE_L];
    }

    uint_t GetEnvType() const
    {
      return State.Data[DataChunk::REG_ENV];
    }
  private:
    //registers state
    DataChunk State;
    //aliases for registers
    uint8_t& Mixer;
    uint8_t& VolA;
    uint8_t& VolB;
    uint8_t& VolC;
    //device
    AYMDevice Device;
  };

  class BeeperRenderer : public Renderer
  {
  public:
    BeeperRenderer()
      : Beeper()
      , VolumeTable(&AYVolumeTab)
    {
    }

    void SetVolumeTable(const VolTable& table)
    {
      VolumeTable = &table;
    }

    void Reset()
    {
      Beeper = 0;
      VolumeTable = &AYVolumeTab;
    }

    void SetNewData(const DataChunk& data)
    {
      if (data.Mask & (1 << DataChunk::REG_BEEPER))
      {
        const std::size_t inLevel = ((data.Data[DataChunk::REG_BEEPER] & DataChunk::REG_MASK_VOL) << 1) + 1;
        Beeper = VolumeTable->at(inLevel);
      }
    }

    virtual void GetLevels(MultiSample& result) const
    {
      std::fill(result.begin(), result.end(), Beeper);
    }
  private:
    Sample Beeper;
    const VolTable* VolumeTable;
  };

  static Sample Average(Sample first, Sample second)
  {
    return static_cast<Sample>((uint_t(first) + second) / 2);
  }

  class MixedRenderer : public Renderer
  {
  public:
    MixedRenderer(Renderer& first, Renderer& second)
      : First(first)
      , Second(second)
    {
    }

    virtual void GetLevels(MultiSample& result) const
    {
      MultiSample firstResult;
      MultiSample secondResult;
      First.GetLevels(firstResult);
      Second.GetLevels(secondResult);
      std::transform(firstResult.begin(), firstResult.end(), secondResult.begin(), result.begin(), &Average);
    }
  private:
    Renderer& First;
    Renderer& Second;
  };

  class InterpolatedRenderer : public Renderer
  {
  public:
    InterpolatedRenderer()
      : Delegate()
      , PrevValues()
      , CurValues()
    {
    }

    void SetSource(Renderer* delegate)
    {
      assert(delegate);
      Delegate = delegate;
    }

    virtual void GetLevels(MultiSample& result) const
    {
      PrevValues = CurValues;
      Delegate->GetLevels(CurValues);
      std::transform(PrevValues.begin(), PrevValues.end(), CurValues.begin(), result.begin(), &Average);
    }
  private:
    Renderer* Delegate;
    MultiSample Levels;
    mutable MultiSample PrevValues;
    mutable MultiSample CurValues;
  };

  class RelayoutRenderer : public Renderer
  {
  public:
    RelayoutRenderer(Renderer& delegate, LayoutType layout)
      : Delegate(delegate)
      , Layout(LAYOUTS[layout])
    {
    }

    virtual void GetLevels(MultiSample& result) const
    {
      MultiSample tmp;
      Delegate.GetLevels(tmp);
      for (uint_t idx = 0; idx < Devices::AYM::CHANNELS; ++idx)
      {
        const uint_t chipChannel = Layout[idx];
        result[idx] = tmp[chipChannel];
      }
    }
  private:
    Renderer& Delegate;
    const LayoutData Layout;
  };

  class MonoRenderer : public Renderer
  {
  public:
    MonoRenderer(Renderer& delegate)
      : Delegate(delegate)
    {
    }

    virtual void GetLevels(MultiSample& result) const
    {
      Delegate.GetLevels(result);
      const Sample average = static_cast<Sample>(std::accumulate(result.begin(), result.end(), uint_t(0)) / result.size());
      std::fill(result.begin(), result.end(), average);
    }
  private:
    Renderer& Delegate;
  };

  class ClockSource
  {
  public:
    void SetFrequency(uint64_t clockFreq, uint_t soundFreq)
    {
      SndOscillator.SetFrequency(soundFreq);
      PsgOscillator.SetFrequency(clockFreq / AYM_CLOCK_DIVISOR);
    }

    void Reset()
    {
      PsgOscillator.Reset();
      SndOscillator.Reset();
    }

    Stamp GetCurrentTime() const
    {
      return PsgOscillator.GetCurrentTime();
    }

    Stamp GetNextSampleTime() const
    {
      return SndOscillator.GetCurrentTime();
    }

    void NextSample()
    {
      SndOscillator.AdvanceTick();
    }

    uint_t NextTime(const Stamp& stamp)
    {
      const Stamp prevStamp = PsgOscillator.GetCurrentTime();
      const uint64_t prevTick = PsgOscillator.GetCurrentTick();
      PsgOscillator.AdvanceTime(stamp.Get() - prevStamp.Get());
      return static_cast<uint_t>(PsgOscillator.GetCurrentTick() - prevTick);
    }
  private:
    Time::Oscillator<Stamp> SndOscillator;
    Time::TimedOscillator<Stamp> PsgOscillator;
  };

  class DataCache
  {
  public:
    DataCache()
      : CumulativeMask()
    {
    }
    
    void Add(const DataChunk& src)
    {
      Buffer.push_back(src);
      CumulativeMask |= src.Mask;
    }

    const DataChunk* GetBegin() const
    {
      return &Buffer.front();
    }
    
    const DataChunk* GetEnd() const
    {
      return &Buffer.back() + 1;
    }

    void Reset()
    {
      Buffer.clear();
      CumulativeMask = 0;
    }

    bool HasBeeperData() const
    {
      return 0 != (CumulativeMask & (1 << DataChunk::REG_BEEPER));
    }

    bool HasPSGData() const
    {
      return 0 != (CumulativeMask & ((1 << DataChunk::REG_BEEPER) - 1));
    }
  private:
    std::vector<DataChunk> Buffer;
    uint_t CumulativeMask;
  };

  class RegularAYMChip : public Chip
  {
  public:
    RegularAYMChip(ChipParameters::Ptr params, Receiver::Ptr target)
      : Params(params)
      , Target(target)
      , PSGWithBeeper(PSG, Beeper)
      , Clock()
    {
      Reset();
    }

    virtual void RenderData(const DataChunk& src)
    {
      BufferedData.Add(src);
    }

    virtual void Flush()
    {
      ApplyParameters();
      Renderer& targetDevice = GetTargetDevice();
      Filter.SetSource(&targetDevice);
      Renderer& targetRender = Params->Interpolate()
        ? static_cast<Renderer&>(Filter)
        : targetDevice;
      const LayoutType layout = Params->Layout();
      if (LAYOUT_ABC == layout)
      {
        RenderChunks(targetRender);
      }
      else if (LAYOUT_MONO == layout)
      {
        MonoRenderer monoRender(targetRender);
        RenderChunks(monoRender);
      }
      else
      {
        RelayoutRenderer relayoutRender(targetRender, layout);
        RenderChunks(relayoutRender);
      }
      Target->Flush();
    }

    virtual void GetState(ChannelsState& state) const
    {
      return PSG.GetState(Params->ClockFreq(), state);
    }

    virtual void Reset()
    {
      PSG.Reset();
      Beeper.Reset();
      Clock.Reset();
      BufferedData.Reset();
    }
  private:
    void ApplyParameters()
    {
      PSG.SetVolumeTable(Params->VolumeTable());
      PSG.SetDutyCycle(Params->DutyCycleValue(), Params->DutyCycleMask());
      Beeper.SetVolumeTable(Params->VolumeTable());
      Clock.SetFrequency(Params->ClockFreq(), Params->SoundFreq());
    }

    Renderer& GetTargetDevice()
    {
      const bool hasPSG = BufferedData.HasPSGData();
      const bool hasBeeper = BufferedData.HasBeeperData();
      return hasBeeper
      ? (hasPSG
         ? static_cast<Renderer&>(PSGWithBeeper)
         : static_cast<Renderer&>(Beeper)
         )
      : static_cast<Renderer&>(PSG);
    }

    void RenderChunks(Renderer& render)
    {
      for (const DataChunk* it = BufferedData.GetBegin(), *lim = BufferedData.GetEnd(); it != lim; ++it)
      {
        const DataChunk& src = *it;
        if (Clock.GetCurrentTime() < src.TimeStamp)
        {
          RenderSingleChunk(src, render);
        }
        PSG.SetNewData(src);
        Beeper.SetNewData(src);
      }
      BufferedData.Reset();
    }

    void RenderSingleChunk(const DataChunk& src, Renderer& render)
    {
      MultiSample result;
      Receiver& target = *Target;
      while (Clock.GetNextSampleTime() < src.TimeStamp)
      {
        if (const uint_t ticksPassed = Clock.NextTime(Clock.GetNextSampleTime()))
        {
          PSG.Tick(ticksPassed);
        }
        render.GetLevels(result);
        target.ApplyData(result);
        Clock.NextSample();
      }
      if (const uint_t ticksPassed = Clock.NextTime(src.TimeStamp))
      {
        PSG.Tick(ticksPassed);
      }
    }
  private:
    const ChipParameters::Ptr Params;
    const Receiver::Ptr Target;
    AYMRenderer PSG;
    BeeperRenderer Beeper;
    MixedRenderer PSGWithBeeper;
    InterpolatedRenderer Filter;
    ClockSource Clock;
    DataCache BufferedData;
  };
}

namespace Devices
{
  namespace AYM
  {
    const VolTable& GetAY38910VolTable()
    {
      return AYVolumeTab;
    }

    const VolTable& GetYM2149FVolTable()
    {
      return YMVolumeTab;
    }

    Chip::Ptr CreateChip(ChipParameters::Ptr params, Receiver::Ptr target)
    {
      return Chip::Ptr(new RegularAYMChip(params, target));
    }
  }
}