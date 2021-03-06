/**
* 
* @file
*
* @brief i18n support implementation
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "language.h"
//common includes
#include <make_ptr.h>
//library includes
#include <l10n/control.h>
//std includes
#include <map>
#include <set>
//boost includes
#include <boost/bind.hpp>
//qt includes
#include <QtCore/QCoreApplication>
#include <QtCore/QLocale>
#include <QtCore/QTranslator>

namespace
{
  class QtTranslationLibrary : public L10n::Library
  {
  public:
    void AddTranslation(const L10n::Translation& trans) override
    {
      static const std::string QM_FILE("qm");

      if (trans.Type != QM_FILE)
      {
        return L10n::Library::Instance().AddTranslation(trans);
      }
      Translations[trans.Language].insert(DumpPtr(new Dump(trans.Data)));
    }

    void SelectTranslation(const std::string& translation) override
    {
      UnloadTranslators();
      const LangToDumpsSetMap::const_iterator it = Translations.find(translation);
      if (it != Translations.end())
      {
        LoadTranslators(it->second);
      }
      L10n::Library::Instance().SelectTranslation(translation);
    }

    L10n::Vocabulary::Ptr GetVocabulary(const std::string& /*domain*/) const override
    {
      return L10n::Vocabulary::Ptr();
    }

    std::vector<std::string> EnumerateLanguages() const
    {
      std::vector<std::string> res;
      std::transform(Translations.begin(), Translations.end(), std::back_inserter(res), boost::bind(&LangToDumpsSetMap::value_type::first, _1));
      return res;
    }
  private:
    typedef std::shared_ptr<const Dump> DumpPtr;
    typedef std::set<DumpPtr> DumpsSet;
    typedef std::map<std::string, DumpsSet> LangToDumpsSetMap;
    typedef std::shared_ptr<QTranslator> TranslatorPtr;
    typedef std::set<TranslatorPtr> TranslatorsSet;

    void UnloadTranslators()
    {
      std::for_each(ActiveTranslators.begin(), ActiveTranslators.end(), boost::bind(&QCoreApplication::removeTranslator, boost::bind(&TranslatorPtr::get, _1)));
      ActiveTranslators.clear();
    }

    void LoadTranslators(const DumpsSet& dumps)
    {
      TranslatorsSet newTranslators;
      std::transform(dumps.begin(), dumps.end(), std::inserter(newTranslators, newTranslators.end()), &CreateTranslator);
      std::for_each(newTranslators.begin(), newTranslators.end(), boost::bind(&QCoreApplication::installTranslator, boost::bind(&TranslatorPtr::get, _1)));
      ActiveTranslators.swap(newTranslators);
    }

    static TranslatorPtr CreateTranslator(DumpPtr dump)
    {
      const TranslatorPtr res = std::make_shared<QTranslator>();
      res->load(&dump->front(), static_cast<int>(dump->size()));
      return res;
    }
  private:
    LangToDumpsSetMap Translations;
    TranslatorsSet ActiveTranslators;
  };

  class LanguageInResources : public UI::Language
  {
  public:
    LanguageInResources()
    {
      L10n::LoadTranslationsFromResources(Lib);
    }

    QStringList GetAvailable() const override
    {
      const std::vector<std::string> lng = Lib.EnumerateLanguages();
      QStringList res;
      res << QLatin1String("en");//by default
      std::for_each(lng.begin(), lng.end(), boost::bind(&QStringList::push_back, &res, boost::bind(&QString::fromStdString, _1)));
      return res;
    }

    QString GetSystem() const override
    {
      QString curLang = QLocale::system().name();
      curLang.truncate(curLang.lastIndexOf(QLatin1Char('_')));//$(lang)_$(country)
      return curLang;
    }

    void Set(const QString& lang) override
    {
      if (lang != Lang)
      {
        Lib.SelectTranslation(lang.toStdString());
        const QLocale locale(lang);
        QLocale::setDefault(locale);
        Lang = lang;
      }
    }
  private:
    QtTranslationLibrary Lib;
    QString Lang;
  };
}

namespace UI
{
  Language::Ptr Language::Create()
  {
    //use slight caching to prevent heavy parsing
    static std::weak_ptr<Language> instance;
    if (Language::Ptr res = instance.lock())
    {
      return res;
    }
    const Language::Ptr res = MakePtr<LanguageInResources>();
    instance = res;
    return res;
  }
}
