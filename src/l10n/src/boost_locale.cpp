/*
Abstract:
  L10n implementation based on boost::locale

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//common includes
#include <contract.h>
#include <debug_log.h>
//library includes
#include <l10n/api.h>
//boost includes
#include <boost/make_shared.hpp>
#include <boost/locale/gnu_gettext.hpp>
//std includes
#include <map>

namespace
{
  const Debug::Stream Dbg("L10n");

  template<class K, class V>
  class MapAdapter : public std::map<K, V>
  {
  public:
    const V* Find(const K& key) const
    {
      const typename std::map<K, V>::const_iterator it = std::map<K, V>::find(key), limit = std::map<K, V>::end();
      return it != limit
        ? &it->second
        : 0;
    }
  };

  typedef boost::shared_ptr<std::locale> LocalePtr;

  class DomainVocabulary : public L10n::Vocabulary
  {
  public:
    DomainVocabulary(LocalePtr locale, const std::string& domain)
      : Locale(locale)
      , Domain(domain)
    {
    }

    virtual String GetText(const char* text) const
    {
      return boost::locale::dgettext<Char>(Domain.c_str(), text, *Locale);
    }

    virtual String GetText(const char* single, const char* plural, int count) const
    {
      return boost::locale::dngettext<Char>(Domain.c_str(), single, plural, count, *Locale);
    }

    virtual String GetText(const char* context, const char* text) const
    {
      return boost::locale::dpgettext<Char>(Domain.c_str(), context, text, *Locale);
    }

    virtual String GetText(const char* context, const char* single, const char* plural, int count) const
    {
      return boost::locale::dnpgettext<Char>(Domain.c_str(), context, single, plural, count, *Locale);
    }
  private:
    const LocalePtr Locale;
    const std::string Domain;
  };

  class BoostLocaleLibrary : public L10n::Library
  {
  public:
    BoostLocaleLibrary()
      : CurrentLocale(boost::make_shared<std::locale>())
    {
    }

    virtual void AddTranslation(const std::string& domain, const std::string& translation, const Dump& data)
    {
      using namespace boost::locale;

      static const std::string EMPTY_PATH;
      gnu_gettext::messages_info& info = Locales[translation];
      info.language = translation;
      info.domains.push_back(gnu_gettext::messages_info::domain(domain));
      info.callback = &LoadMessage;
      info.paths.assign(&EMPTY_PATH, &EMPTY_PATH + 1);

      const std::string filename = EMPTY_PATH + '/' + translation + '/' + info.locale_category + '/' + domain + ".mo";
      Translations[filename] = data;
      Dbg("Added translation %1% in %2% bytes", filename, data.size());
    }

    virtual void SelectTranslation(const std::string& translation)
    {
      using namespace boost::locale;
      try
      {
        if (const gnu_gettext::messages_info* info = Locales.Find(translation))
        {
          message_format<Char>* const facet = gnu_gettext::create_messages_facet<Char>(*info);
          Require(facet);
          *CurrentLocale = std::locale(std::locale::classic(), facet);
          Dbg("Selected translation %1%", translation);
          return;
        }
      }
      catch (const std::exception& e)
      {
        Dbg("Failed to select translation %1%: %2%", e.what());
      }
      *CurrentLocale = std::locale();
      Dbg("Selected unknown translation %1%", translation);
    }

    virtual L10n::Vocabulary::Ptr GetVocabulary(const std::string& domain) const
    {
      return boost::make_shared<DomainVocabulary>(CurrentLocale, domain);
    }

    static BoostLocaleLibrary& Instance()
    {
      static BoostLocaleLibrary self;
      return self;
    }
  private:
    static std::vector<char> LoadMessage(const std::string& file, const std::string& encoding)
    {
      const BoostLocaleLibrary& self = BoostLocaleLibrary::Instance();
      if (const Dump* data = self.Translations.Find(file))
      {
        Dbg("Loading message %1% with encoding %2%", file, encoding);
        return std::vector<char>(data->begin(), data->end());
      }
      else
      {
        Dbg("Message %1% with encoding %2% not found", file, encoding);
        return std::vector<char>();
      }
    }
  private:
    const LocalePtr CurrentLocale;
    MapAdapter<std::string, Dump> Translations;
    MapAdapter<std::string, boost::locale::gnu_gettext::messages_info> Locales;
  };
}

namespace L10n
{
  Library& Library::Instance()
  {
    return BoostLocaleLibrary::Instance();
  }
}
