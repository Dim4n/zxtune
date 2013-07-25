/*
Abstract:
  ST1 modules playback support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "soundtracker.h"
#include "core/plugins/registrator.h"
//library includes
#include <core/plugin_attrs.h>
#include <core/conversion/aym.h>

namespace ZXTune
{
  void RegisterST1Support(PlayerPluginsRegistrator& registrator)
  {
    //plugin attributes
    const Char ID[] = {'S', 'T', '1', 0};
    const uint_t CAPS = CAP_STOR_MODULE | CAP_DEV_AYM | CAP_CONV_RAW | Module::AYM::SupportedFormatConvertors;

    const Formats::Chiptune::SoundTracker::Decoder::Ptr decoder = Formats::Chiptune::SoundTracker::Ver1::CreateUncompiledDecoder();
    const Module::Factory::Ptr factory = Module::SoundTracker::CreateModulesFactory(decoder);
    const PlayerPlugin::Ptr plugin = CreatePlayerPlugin(ID, CAPS, decoder, factory);
    registrator.RegisterPlugin(plugin);
  }
}
