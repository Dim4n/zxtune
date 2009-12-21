/*
Abstract:
  Plugins enumerator interface and related

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#ifndef __CORE_PLUGINS_ENUMERATOR_H_DEFINED__
#define __CORE_PLUGINS_ENUMERATOR_H_DEFINED__

#include "../player.h"

#include <boost/function.hpp>

namespace ZXTune
{
  struct ModuleRegion
  {
    ModuleRegion() : Offset(), Size()
    {
    }
    std::size_t Offset;
    std::size_t Size;
  };

  struct MetaContainer
  {
    IO::DataContainer::Ptr Data;
    String Path;
    StringArray PluginsChain;
  };
  
  void ExtractMetaProperties(const MetaContainer& container, const ModuleRegion& region, 
                             ParametersMap& properties, Dump& rawData);
  
  //in: metacontainer
  //out: holder, region
  typedef boost::function<bool(const MetaContainer&, Module::Holder::Ptr&, ModuleRegion&)> CreatePlayerFunc;
  //in: data
  //output: data, region
  typedef boost::function<bool(const IO::DataContainer&, IO::DataContainer::Ptr&, ModuleRegion&)> ProcessImplicitFunc;
  //in: metacontainer, parameters+callback
  //out: region
  typedef boost::function<Error(const MetaContainer&, const DetectParameters&, ModuleRegion&)> ProcessContainerFunc;
  //in: container, path
  //out: container, rest path
  typedef boost::function<bool(const IO::DataContainer&, const String&, IO::DataContainer::Ptr&, String&)> OpenContainerFunc;

  class PluginsEnumerator
  {
  public:
    virtual ~PluginsEnumerator() {}

    //endpoint modules support
    virtual void RegisterPlayerPlugin(const PluginInformation& info, const CreatePlayerFunc& func) = 0;
    //implicit containers support
    virtual void RegisterImplicitPlugin(const PluginInformation& info, const ProcessImplicitFunc& func) = 0;
    //nested containers support
    virtual void RegisterContainerPlugin(const PluginInformation& info, 
      const OpenContainerFunc& opener, const ProcessContainerFunc& processor) = 0;
    
    //public interface
    virtual void EnumeratePlugins(std::vector<PluginInformation>& plugins) const = 0;
    
    //private interface
    //resolve subpath
    virtual Error ResolveSubpath(IO::DataContainer::Ptr data, const String& subpath, const DetectParameters::LogFunc& logger,
      MetaContainer& result) const = 0;
    //full module detection
    virtual Error DetectModules(const DetectParameters& params, const MetaContainer& data, 
      ModuleRegion& region) const = 0;
    //nested containers detection
    virtual Error DetectContainer(const DetectParameters& params, const MetaContainer& input,
      ModuleRegion& region) const = 0;
    //implicit containers detection
    virtual Error DetectImplicit(const DetectParameters::FilterFunc& filter, const IO::DataContainer& input,
      IO::DataContainer::Ptr& output, ModuleRegion& region, String& pluginId) const = 0;
    //modules detection
    virtual Error DetectModule(const DetectParameters::FilterFunc& filter, const MetaContainer& data,
      Module::Holder::Ptr& player, ModuleRegion& region, String& pluginId) const = 0;
                                        
    //instantiator
    static PluginsEnumerator& Instance();
  };
}

#endif //__CORE_PLUGINS_ENUMERATOR_H_DEFINED__
