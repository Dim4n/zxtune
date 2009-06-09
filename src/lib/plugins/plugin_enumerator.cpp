#include "plugin_enumerator.h"

#include <player_attrs.h>

#include <set>
#include <cassert>
#include <algorithm>

namespace
{
  using namespace ZXTune;

  struct PluginDescriptor
  {
    CheckFunc Checker;
    FactoryFunc Creator;
    InfoFunc Descriptor;
    //cached
    unsigned Priority;
  };

  bool operator < (const PluginDescriptor& lh, const PluginDescriptor& rh)
  {
    return lh.Priority < rh.Priority ||
        lh.Checker < rh.Checker || lh.Creator < rh.Creator || lh.Descriptor < rh.Descriptor;
  }

  bool operator == (const PluginDescriptor& lh, const PluginDescriptor& rh)
  {
    return lh.Checker == rh.Checker && lh.Creator == rh.Creator && lh.Descriptor == rh.Descriptor;
  }

  class PluginEnumeratorImpl : public PluginEnumerator
  {
    typedef std::set<PluginDescriptor> PluginsStorage;
  public:
    PluginEnumeratorImpl()
    {
    }

    virtual void RegisterPlugin(CheckFunc check, FactoryFunc create, InfoFunc describe)
    {
      PluginDescriptor descr = {check, create, describe};
      assert(Plugins.end() == std::find(Plugins.begin(), Plugins.end(), descr));
      ModulePlayer::Info info;
      describe(info);
      //store multitrack plugins to beginning
      if (info.Capabilities & CAP_MULTITRACK)
      {
        descr.Priority = 0;
      }
      else if (info.Capabilities & CAP_CONTAINER)
      {
        descr.Priority = 50;
      }
      else
      {
        descr.Priority = 100;
      }
      Plugins.insert(descr);
    }

    virtual void EnumeratePlugins(std::vector<ModulePlayer::Info>& infos) const
    {
      infos.resize(Plugins.size());
      std::vector<ModulePlayer::Info>::iterator out(infos.begin());
      for (PluginsStorage::const_iterator it = Plugins.begin(), lim = Plugins.end(); it != lim; ++it, ++out)
      {
        (it->Descriptor)(*out);
      }
    }

    virtual bool CheckModule(const String& filename, const IO::DataContainer& data, ModulePlayer::Info& info) const
    {
      for (PluginsStorage::const_iterator it = Plugins.begin(), lim = Plugins.end(); it != lim; ++it)
      {
        if ((it->Checker)(filename, data))
        {
          (it->Descriptor)(info);
          return true;
        }
      }
      return false;
    }

    virtual ModulePlayer::Ptr CreatePlayer(const String& filename, const IO::DataContainer& data) const
    {
      for (PluginsStorage::const_iterator it = Plugins.begin(), lim = Plugins.end(); it != lim; ++it)
      {
        if ((it->Checker)(filename, data))
        {
          return (it->Creator)(filename, data);
        }
      }
      return ModulePlayer::Ptr(0);
    }
  private:
    PluginsStorage Plugins;
  };
}

namespace ZXTune
{
  PluginEnumerator& PluginEnumerator::Instance()
  {
    static PluginEnumeratorImpl instance;
    return instance;
  }

  ModulePlayer::Ptr ModulePlayer::Create(const String& filename, const IO::DataContainer& data)
  {
    return PluginEnumerator::Instance().CreatePlayer(filename, data);
  }

  bool ModulePlayer::Check(const String& filename, const IO::DataContainer& data, ModulePlayer::Info& info)
  {
    return PluginEnumerator::Instance().CheckModule(filename, data, info);
  }

  void GetSupportedPlayers(std::vector<ModulePlayer::Info>& infos)
  {
    return PluginEnumerator::Instance().EnumeratePlugins(infos);
  }
}
