/*
Abstract:
  Win32 waveout backend implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#ifdef WIN32_WAVEOUT_SUPPORT

//local includes
#include "backend_impl.h"
#include "enumerator.h"
//common includes
#include <tools.h>
#include <error_tools.h>
//library includes
#include <sound/backend_attrs.h>
#include <sound/backends_parameters.h>
#include <sound/error_codes.h>
#include <sound/sound_parameters.h>
//platform-dependent includes
#include <windows.h>
//std includes
#include <algorithm>
//boost includes
#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ref.hpp>
//text includes
#include <sound/text/backends.h>
#include <sound/text/sound.h>

#define FILE_TAG 5E3F141A

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::Sound;

  const uint_t MAX_WIN32_VOLUME = 0xffff;
  const uint_t BUFFERS_MIN = 3;
  const uint_t BUFFERS_MAX = 10;

  const Char WIN32_BACKEND_ID[] = {'w', 'i', 'n', '3', '2', 0};
  const String WIN32_BACKEND_VERSION(FromStdString("$Rev$"));

  inline void CheckMMResult(::MMRESULT res, Error::LocationRef loc)
  {
    if (MMSYSERR_NOERROR != res)
    {
      std::vector<char> buffer(1024);
      if (MMSYSERR_NOERROR == ::waveOutGetErrorText(res, &buffer[0], static_cast<UINT>(buffer.size())))
      {
        throw MakeFormattedError(loc, BACKEND_PLATFORM_ERROR, Text::SOUND_ERROR_WIN32_BACKEND_ERROR,
          String(buffer.begin(), std::find(buffer.begin(), buffer.end(), '\0')));
      }
      else
      {
        throw MakeFormattedError(loc, BACKEND_PLATFORM_ERROR, Text::SOUND_ERROR_WIN32_BACKEND_ERROR, res);
      }
    }
  }

  inline void CheckPlatformResult(bool val, Error::LocationRef loc)
  {
    if (!val)
    {
      //TODO: convert code to string
      throw MakeFormattedError(loc, BACKEND_PLATFORM_ERROR, Text::SOUND_ERROR_WIN32_BACKEND_ERROR, ::GetLastError());
    }
  }

  // buffer wrapper
  class WaveBuffer
  {
  public:
    WaveBuffer()
      : Header(), Buffer(), Handle(0), Event(INVALID_HANDLE_VALUE)
    {
    }

    ~WaveBuffer()
    {
      assert(0 == Handle);
    }

    void Allocate(::HWAVEOUT handle, ::HANDLE event, const RenderParameters& params)
    {
      assert(0 == Handle && INVALID_HANDLE_VALUE == Event);
      const std::size_t bufSize = params.SamplesPerFrame();
      Buffer.resize(bufSize);
      Header.lpData = ::LPSTR(&Buffer[0]);
      Header.dwBufferLength = ::DWORD(bufSize) * sizeof(Buffer.front());
      Header.dwUser = Header.dwLoops = Header.dwFlags = 0;
      CheckMMResult(::waveOutPrepareHeader(handle, &Header, sizeof(Header)), THIS_LINE);
      //mark as free
      Header.dwFlags |= WHDR_DONE;
      Handle = handle;
      Event = event;
    }

    void Release()
    {
      Wait();
      CheckMMResult(::waveOutUnprepareHeader(Handle, &Header, sizeof(Header)), THIS_LINE);
      std::vector<MultiSample>().swap(Buffer);
      Handle = 0;
      Event = INVALID_HANDLE_VALUE;
    }

    void Process(const std::vector<MultiSample>& buf)
    {
      Wait();
      assert(Header.dwFlags & WHDR_DONE);
      assert(buf.size() <= Buffer.size());
      Header.dwBufferLength = static_cast< ::DWORD>(std::min<std::size_t>(buf.size(), Buffer.size())) * sizeof(Buffer.front());
      std::memcpy(Header.lpData, &buf[0], Header.dwBufferLength);
      Header.dwFlags &= ~WHDR_DONE;
      CheckMMResult(::waveOutWrite(Handle, &Header, sizeof(Header)), THIS_LINE);
    }
  private:
    void Wait()
    {
      while (!(Header.dwFlags & WHDR_DONE))
      {
        CheckPlatformResult(WAIT_OBJECT_0 == ::WaitForSingleObject(Event, INFINITE), THIS_LINE);
      }
    }
  private:
    ::WAVEHDR Header;
    std::vector<MultiSample> Buffer;
    ::HWAVEOUT Handle;
    ::HANDLE Event;
  };

  // volume controller implementation
  class Win32VolumeController : public VolumeControl
  {
  public:
    Win32VolumeController(boost::mutex& stateMutex, int_t& device)
      : StateMutex(stateMutex), Device(device)
    {
    }

    virtual Error GetVolume(MultiGain& volume) const
    {
      // use exceptions for simplification
      try
      {
        boost::mutex::scoped_lock lock(StateMutex);
        boost::array<uint16_t, OUTPUT_CHANNELS> buffer;
        BOOST_STATIC_ASSERT(sizeof(buffer) == sizeof(DWORD));
        CheckMMResult(::waveOutGetVolume(reinterpret_cast< ::HWAVEOUT>(Device), safe_ptr_cast<LPDWORD>(&buffer[0])), THIS_LINE);
        std::transform(buffer.begin(), buffer.end(), volume.begin(), std::bind2nd(std::divides<Gain>(), MAX_WIN32_VOLUME));
        return Error();
      }
      catch (const Error& e)
      {
        return e;
      }
    }

    virtual Error SetVolume(const MultiGain& volume)
    {
      if (volume.end() != std::find_if(volume.begin(), volume.end(), std::bind2nd(std::greater<Gain>(), Gain(1.0))))
      {
        return Error(THIS_LINE, BACKEND_INVALID_PARAMETER, Text::SOUND_ERROR_BACKEND_INVALID_GAIN);
      }
      // use exceptions for simplification
      try
      {
        boost::mutex::scoped_lock lock(StateMutex);
        boost::array<uint16_t, OUTPUT_CHANNELS> buffer;
        std::transform(volume.begin(), volume.end(), buffer.begin(), std::bind2nd(std::multiplies<Gain>(), Gain(MAX_WIN32_VOLUME)));
        BOOST_STATIC_ASSERT(sizeof(buffer) == sizeof(DWORD));
        CheckMMResult(::waveOutSetVolume(reinterpret_cast< ::HWAVEOUT>(Device), *safe_ptr_cast<LPDWORD>(&buffer[0])), THIS_LINE);
        return Error();
      }
      catch (const Error& e)
      {
        return e;
      }
    }

  private:
    boost::mutex& StateMutex;
    int_t& Device;
  };

  class Win32BackendParameters
  {
  public:
    explicit Win32BackendParameters(const Parameters::Accessor& accessor)
      : Accessor(accessor)
    {
    }

    int_t GetDevice() const
    {
      Parameters::IntType device = Parameters::ZXTune::Sound::Backends::Win32::DEVICE_DEFAULT;
      Accessor.FindIntValue(Parameters::ZXTune::Sound::Backends::Win32::DEVICE, device);
      return static_cast<int_t>(device);
    }

    std::size_t GetBuffers() const
    {
      Parameters::IntType buffers = Parameters::ZXTune::Sound::Backends::Win32::BUFFERS_DEFAULT;
      if (Accessor.FindIntValue(Parameters::ZXTune::Sound::Backends::Win32::BUFFERS, buffers) &&
          !in_range<Parameters::IntType>(buffers, BUFFERS_MIN, BUFFERS_MAX))
      {
        throw MakeFormattedError(THIS_LINE, BACKEND_INVALID_PARAMETER,
          Text::SOUND_ERROR_WIN32_BACKEND_INVALID_BUFFERS, static_cast<int_t>(buffers), BUFFERS_MIN, BUFFERS_MAX);
      }
      return static_cast<std::size_t>(buffers);
    }
  private:
    const Parameters::Accessor& Accessor;
  };

  class Win32BackendWorker : public BackendWorker
                           , private boost::noncopyable
  {
  public:
    explicit Win32BackendWorker(Parameters::Accessor::Ptr params)
      : BackendParams(params)
      , RenderingParameters(RenderParameters::Create(BackendParams))
      , Event(::CreateEvent(0, FALSE, FALSE, 0))
      //device identifier used for opening and volume control
      , Device(0)
      , WaveHandle(0)
      , VolumeController(new Win32VolumeController(StateMutex, Device))
    {
    }

    virtual ~Win32BackendWorker()
    {
      assert(0 == WaveHandle || !"Win32Backend::Stop should be called before exit");
      ::CloseHandle(Event);
    }

    virtual void Test()
    {
      ::WAVEFORMATEX format;
      SetupFormat(format);
      const Win32BackendParameters params(*BackendParams);
      const int_t device = params.GetDevice();
      ::HWAVEOUT handle;
      CheckMMResult(::waveOutOpen(&handle, static_cast< ::UINT>(device), &format, 0, 0,
        WAVE_FORMAT_DIRECT), THIS_LINE);
      CheckMMResult(::waveOutClose(handle), THIS_LINE);
    }

    virtual void OnStartup(const Module::Holder& /*module*/)
    {
      assert(0 == WaveHandle);

      const Win32BackendParameters params(*BackendParams);
      const boost::mutex::scoped_lock lock(StateMutex);
      Device = params.GetDevice();
      Buffers.resize(params.GetBuffers());
      CurrentBuffer = CycledIterator<WaveBuffer*>(&Buffers.front(), &Buffers.back() + 1);

      SetupFormat(Format);
      CheckMMResult(::waveOutOpen(&WaveHandle, static_cast< ::UINT>(Device), &Format, DWORD_PTR(Event), 0,
        CALLBACK_EVENT | WAVE_FORMAT_DIRECT), THIS_LINE);
      std::for_each(Buffers.begin(), Buffers.end(), boost::bind(&WaveBuffer::Allocate, _1, WaveHandle, Event, boost::cref(*RenderingParameters)));
      CheckPlatformResult(0 != ::ResetEvent(Event), THIS_LINE);
    }

    virtual void OnShutdown()
    {
      if (0 != WaveHandle)
      {
        std::for_each(Buffers.begin(), Buffers.end(), boost::mem_fn(&WaveBuffer::Release));
        CheckMMResult(::waveOutReset(WaveHandle), THIS_LINE);
        CheckMMResult(::waveOutClose(WaveHandle), THIS_LINE);
        WaveHandle = 0;
      }
    }

    virtual void OnPause()
    {
      if (0 != WaveHandle)
      {
        CheckMMResult(::waveOutPause(WaveHandle), THIS_LINE);
      }
    }

    virtual void OnResume()
    {
      if (0 != WaveHandle)
      {
        CheckMMResult(::waveOutRestart(WaveHandle), THIS_LINE);
      }
    }

    virtual void OnFrame(const Module::TrackState& /*state*/)
    {
    }

    virtual void OnBufferReady(std::vector<MultiSample>& buffer)
    {
      // buffer is just sent to playback, so we can safely lock here
      CurrentBuffer->Process(buffer);
      ++CurrentBuffer;
    }

    virtual VolumeControl::Ptr GetVolumeControl() const
    {
      return VolumeController;
    }
  private:
    void SetupFormat(::WAVEFORMATEX& format) const
    {
      std::memset(&format, 0, sizeof(format));
      format.wFormatTag = WAVE_FORMAT_PCM;
      format.nChannels = OUTPUT_CHANNELS;
      format.nSamplesPerSec = static_cast< ::DWORD>(RenderingParameters->SoundFreq());
      format.nBlockAlign = sizeof(MultiSample);
      format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
      format.wBitsPerSample = 8 * sizeof(Sample);
    }
  private:
    const Parameters::Accessor::Ptr BackendParams;
    const RenderParameters::Ptr RenderingParameters;
    boost::mutex StateMutex;
    std::vector<WaveBuffer> Buffers;
    CycledIterator<WaveBuffer*> CurrentBuffer;
    ::HANDLE Event;
    int_t Device;
    ::HWAVEOUT WaveHandle;
    ::WAVEFORMATEX Format;
    const VolumeControl::Ptr VolumeController;
  };

  class Win32BackendCreator : public BackendCreator
  {
  public:
    virtual String Id() const
    {
      return WIN32_BACKEND_ID;
    }

    virtual String Description() const
    {
      return Text::WIN32_BACKEND_DESCRIPTION;
    }

    virtual String Version() const
    {
      return WIN32_BACKEND_VERSION;
    }

    virtual uint_t Capabilities() const
    {
      return CAP_TYPE_SYSTEM | CAP_FEAT_HWVOLUME;
    }

    virtual Error CreateBackend(CreateBackendParameters::Ptr params, Backend::Ptr& result) const
    {
      try
      {
        const Parameters::Accessor::Ptr allParams = params->GetParameters();
        const BackendWorker::Ptr worker(new Win32BackendWorker(allParams));
        result = Sound::CreateBackend(params, worker);
        return Error();
      }
      catch (const Error& e)
      {
        return MakeFormattedError(THIS_LINE, BACKEND_FAILED_CREATE,
          Text::SOUND_ERROR_BACKEND_FAILED, Id()).AddSuberror(e);
      }
      catch (const std::bad_alloc&)
      {
        return Error(THIS_LINE, BACKEND_NO_MEMORY, Text::SOUND_ERROR_BACKEND_NO_MEMORY);
      }
    }
  };
}

namespace ZXTune
{
  namespace Sound
  {
    void RegisterWin32Backend(BackendsEnumerator& enumerator)
    {
      const BackendCreator::Ptr creator(new Win32BackendCreator());
      enumerator.RegisterCreator(creator);
    }
  }
}

#else //not supported

namespace ZXTune
{
  namespace Sound
  {
    void RegisterWin32Backend(class BackendsEnumerator& /*enumerator*/)
    {
      //do nothing
    }
  }
}

#endif
