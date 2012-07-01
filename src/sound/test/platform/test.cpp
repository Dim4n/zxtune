#include <sound/backends/alsa.h>
#include <iostream>
#include <format.h>

namespace
{
  void ShowAlsaDevicesAndMixers()
  {
    using namespace ZXTune::Sound::ALSA;
    std::cout << "ALSA devices and mixers:" << std::endl;
    for (const Device::Iterator::Ptr devices = EnumerateDevices(); devices->IsValid(); devices->Next())
    {
      const Device::Ptr device = devices->Get();
      std::cout << Strings::Format("Name: '%1%' Card: '%2%' Id: '%3%'\n", device->Name(), device->CardName(), device->Id());
      const StringArray& mixers = device->Mixers();
      for (StringArray::const_iterator mit = mixers.begin(), mlim = mixers.end(); mit != mlim; ++mit)
      {
        std::cout << ' ' << *mit << std::endl;
      }
    }
  }
}

int main()
{
  ShowAlsaDevicesAndMixers();
}
