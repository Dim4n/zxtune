/*
Abstract:
  Playlist model implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "model.h"
#include "ui/utils.h"
//common includes
#include <logging.h>
#include <template_parameters.h>
//library includes
#include <core/module_attrs.h>
//boost includes
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/iterator/counting_iterator.hpp>
//qt includes
#include <QtCore/QMimeData>
#include <QtCore/QMutex>
#include <QtCore/QSet>
#include <QtGui/QIcon>
//text includes
#include "text/text.h"

namespace
{
  const std::string THIS_MODULE("Playlist::Model");

  const QModelIndex EMPTY_INDEX = QModelIndex();

  class RowDataProvider
  {
  public:
    virtual ~RowDataProvider() {}

    virtual QVariant GetHeader(unsigned column) const = 0;
    virtual QVariant GetData(const Playlist::Item::Data& item, unsigned column) const = 0;
  };

  class DummyDataProvider : public RowDataProvider
  {
  public:
    virtual QVariant GetHeader(unsigned /*column*/) const
    {
      return QVariant();
    }

    virtual QVariant GetData(const Playlist::Item::Data& /*item*/, unsigned /*column*/) const
    {
      return QVariant();
    }
  };

  class DecorationDataProvider : public RowDataProvider
  {
  public:
    virtual QVariant GetHeader(unsigned /*column*/) const
    {
      return QVariant();
    }

    virtual QVariant GetData(const Playlist::Item::Data& item, unsigned column) const
    {
      switch (column)
      {
      case Playlist::Model::COLUMN_TYPEICON:
        {
          const QString iconPath = ToQString(
            Text::TYPEICONS_RESOURCE_PREFIX + item.GetType());
          return QIcon(iconPath);
        }
      default:
        return QVariant();
      };
    }
  };

  class DisplayDataProvider : public RowDataProvider
  {
  public:
    virtual QVariant GetHeader(unsigned column) const
    {
      switch (column)
      {
      case Playlist::Model::COLUMN_TITLE:
        return Playlist::Model::tr("Author - Title");
      case Playlist::Model::COLUMN_DURATION:
        return Playlist::Model::tr("Duration");
      default:
        return QVariant();
      };
    }

    virtual QVariant GetData(const Playlist::Item::Data& item, unsigned column) const
    {
      switch (column)
      {
      case Playlist::Model::COLUMN_TITLE:
        return ToQString(item.GetTitle());
      case Playlist::Model::COLUMN_DURATION:
        return ToQString(item.GetDurationString());
      default:
        return QVariant();
      };
    }
  };

  class TooltipDataProvider : public RowDataProvider
  {
  public:
    virtual QVariant GetHeader(unsigned /*column*/) const
    {
      return QVariant();
    }

    virtual QVariant GetData(const Playlist::Item::Data& item, unsigned /*column*/) const
    {
      return ToQString(item.GetTooltip());
    }
  };

  class DataProvidersSet
  {
  public:
    DataProvidersSet()
      : Decoration()
      , Display()
      , Tooltip()
      , Dummy()
    {
    }

    const RowDataProvider& GetProvider(int role) const
    {
      switch (role)
      {
      case Qt::DecorationRole:
        return Decoration;
      case Qt::DisplayRole:
        return Display;
      case Qt::ToolTipRole:
        return Tooltip;
      default:
        return Dummy;
      }
    }
  private:
    const DecorationDataProvider Decoration;
    const DisplayDataProvider Display;
    const TooltipDataProvider Tooltip;
    const DummyDataProvider Dummy;
  };

  template<class Container, class IteratorType>
  void GetChoosenItems(Container& items, const Playlist::Model::IndexSet& indexes, std::vector<IteratorType>& result)
  {
    if (items.empty() || indexes.empty())
    {
      result.clear();
      return;
    }
    assert(*indexes.rbegin() < items.size());
    std::vector<IteratorType> choosenItems;
    choosenItems.reserve(indexes.size());
    if (indexes.size() == items.size())
    {
      //all items
      for (IteratorType it = items.begin(), lim = items.end(); it != lim; ++it)
      {
        choosenItems.push_back(it);
      }
    }
    else
    {
      Playlist::Model::IndexType lastIndex = 0;
      IteratorType lastIterator = items.begin();
      for (Playlist::Model::IndexSet::const_iterator idxIt = indexes.begin(), idxLim = indexes.end(); idxIt != idxLim; ++idxIt)
      {
        const Playlist::Model::IndexType curIndex = *idxIt;
        assert(curIndex >= lastIndex);
        if (const Playlist::Model::IndexType delta = curIndex - lastIndex)
        {
          std::advance(lastIterator, delta);
        }
        choosenItems.push_back(lastIterator);
        lastIndex = curIndex;
      }
    }
    choosenItems.swap(result);
  }

  typedef std::vector<Playlist::Item::Data::Ptr> ItemsArray;

  class PlayitemsContainer
  {
  public:
    void AddItem(Playlist::Item::Data::Ptr item)
    {
      const IndexedItem idxItem(item, static_cast<Playlist::Model::IndexType>(Items.size()));
      Items.push_back(idxItem);
    }

    std::size_t CountItems() const
    {
      return Items.size();
    }

    Playlist::Item::Data::Ptr GetItemByIndex(Playlist::Model::IndexType idx) const
    {
      if (idx >= Playlist::Model::IndexType(Items.size()))
      {
        return Playlist::Item::Data::Ptr();
      }
      const ItemsContainer::const_iterator it = GetIteratorByIndex(idx);
      return it->first;
    }

    void ForAllItems(Playlist::Model::Visitor& visitor) const
    {
      for (ItemsContainer::const_iterator it = Items.begin(), lim = Items.end(); it != lim; ++it)
      {
        visitor.OnItem(it->second, it->first);
      }
    }

    virtual void ForSpecifiedItems(const Playlist::Model::IndexSet& indices, Playlist::Model::Visitor& visitor) const
    {
      std::vector<ItemsContainer::const_iterator> choosenIterators;
      GetChoosenItems(Items, indices, choosenIterators);
      for (std::vector<ItemsContainer::const_iterator>::const_iterator it = choosenIterators.begin(), lim = choosenIterators.end(); it != lim; ++it)
      {
        const IndexedItem& item = **it;
        visitor.OnItem(item.second, item.first);
      }
    }

    void RemoveItems(const Playlist::Model::IndexSet& indexes)
    {
      if (indexes.empty())
      {
        return;
      }
      std::vector<ItemsContainer::iterator> itersToRemove;
      GetChoosenItems(Items, indexes, itersToRemove);
      assert(indexes.size() == itersToRemove.size());
      std::for_each(itersToRemove.begin(), itersToRemove.end(),
        boost::bind(&ItemsContainer::erase, &Items, _1));
    }

    void MoveItems(const Playlist::Model::IndexSet& indexes, Playlist::Model::IndexType destination)
    {
      if (!indexes.count(destination))
      {
        MoveItemsInternal(indexes, destination);
      }
      else
      {
        //try to move at the first nonselected and place before it
        for (Playlist::Model::IndexType moveAfter = destination; moveAfter != Items.size(); ++moveAfter)
        {
          if (!indexes.count(moveAfter))
          {
            MoveItemsInternal(indexes, moveAfter);
            return;
          }
        }
        MoveItemsInternal(indexes, static_cast<Playlist::Model::IndexType>(Items.size()));
      }
    }

    class Comparer
    {
    public:
      typedef boost::shared_ptr<const Comparer> Ptr;
      virtual ~Comparer() {}

      virtual bool CompareItems(const Playlist::Item::Data& lh, const Playlist::Item::Data& rh) const = 0;
    };

    void Sort(const Comparer& cmp)
    {
      Items.sort(ComparerWrapper(cmp));
    }

    void GetIndexRemapping(Playlist::Model::OldToNewIndexMap& idxMap) const
    {
      Playlist::Model::OldToNewIndexMap result;
      std::transform(Items.begin(), Items.end(), boost::counting_iterator<Playlist::Model::IndexType>(0), std::inserter(result, result.end()),
        boost::bind(&MakeIndexPair, _1, _2));
      idxMap.swap(result);
    }

    void ResetIndexes()
    {
      std::transform(Items.begin(), Items.end(), boost::counting_iterator<Playlist::Model::IndexType>(0), Items.begin(),
        boost::bind(&UpdateItemIndex, _1, _2));
    }
  private:
    typedef std::pair<Playlist::Item::Data::Ptr, Playlist::Model::IndexType> IndexedItem;
    typedef std::list<IndexedItem> ItemsContainer;

    static Playlist::Model::OldToNewIndexMap::value_type MakeIndexPair(const IndexedItem& item, Playlist::Model::IndexType idx)
    {
      return Playlist::Model::OldToNewIndexMap::value_type(item.second, idx);
    }

    static IndexedItem UpdateItemIndex(const IndexedItem& item, Playlist::Model::IndexType idx)
    {
      return IndexedItem(item.first, idx);
    }

    class ComparerWrapper : public std::binary_function<ItemsContainer::value_type, ItemsContainer::value_type, bool>
    {
    public:
      explicit ComparerWrapper(const Comparer& cmp)
        : Cmp(cmp)
      {
      }

      result_type operator()(first_argument_type lh, second_argument_type rh) const
      {
        return Cmp.CompareItems(*lh.first, *rh.first);
      }
    private:
      const Comparer& Cmp;
    };

    ItemsContainer::const_iterator GetIteratorByIndex(Playlist::Model::IndexType idx) const
    {
      ItemsContainer::const_iterator it = Items.begin();
      std::advance(it, idx);
      return it;
    }

    ItemsContainer::iterator GetIteratorByIndex(Playlist::Model::IndexType idx)
    {
      ItemsContainer::iterator it = Items.begin();
      std::advance(it, idx);
      return it;
    }

    void MoveItemsInternal(const Playlist::Model::IndexSet& indexes, Playlist::Model::IndexType destination)
    {
      if (indexes.empty())
      {
        return;
      }
      assert(!indexes.count(destination));
      ItemsContainer::iterator delimiter = GetIteratorByIndex(destination);

      std::vector<ItemsContainer::iterator> movedIters;
      GetChoosenItems(Items, indexes, movedIters);
      assert(indexes.size() == movedIters.size());

      ItemsContainer movedItems;
      std::for_each(movedIters.begin(), movedIters.end(), 
        boost::bind(&ItemsContainer::splice, &movedItems, movedItems.end(), boost::ref(Items), _1));
      
      ItemsContainer afterItems;
      afterItems.splice(afterItems.begin(), Items, delimiter, Items.end());

      //gathering back
      Items.splice(Items.end(), movedItems);
      Items.splice(Items.end(), afterItems);
    }
  private:
    ItemsContainer Items;
  };

  template<class T>
  class TypedPlayitemsComparer : public PlayitemsContainer::Comparer
  {
  public:
    typedef T (Playlist::Item::Data::*Functor)() const;
    TypedPlayitemsComparer(Functor fn, bool ascending)
      : Getter(fn)
      , Ascending(ascending)
    {
    }

    bool CompareItems(const Playlist::Item::Data& lh, const Playlist::Item::Data& rh) const
    {
      const T val1 = (lh.*Getter)();
      const T val2 = (rh.*Getter)();
      return Ascending
        ? val1 < val2
        : val1 > val2;
    }
  private:
    const Functor Getter;
    const bool Ascending;
  };

  PlayitemsContainer::Comparer::Ptr CreateComparerByColumn(int column, bool ascending)
  {
    switch (column)
    {
    case Playlist::Model::COLUMN_TYPEICON:
      return PlayitemsContainer::Comparer::Ptr(new TypedPlayitemsComparer<String>(&Playlist::Item::Data::GetType, ascending));
    case Playlist::Model::COLUMN_TITLE:
      return PlayitemsContainer::Comparer::Ptr(new TypedPlayitemsComparer<String>(&Playlist::Item::Data::GetTitle, ascending));
    case Playlist::Model::COLUMN_DURATION:
      return PlayitemsContainer::Comparer::Ptr(new TypedPlayitemsComparer<unsigned>(&Playlist::Item::Data::GetDurationValue, ascending));
    default:
      return PlayitemsContainer::Comparer::Ptr();
    }
  }

  const char ITEMS_MIMETYPE[] = "application/playlist.indexes";

  class ModelImpl : public Playlist::Model
  {
  public:
    explicit ModelImpl(QObject& parent)
      : Playlist::Model(parent)
      , Providers()
      , Synchronizer(QMutex::Recursive)
      , FetchedItemsCount()
      , Container(new PlayitemsContainer())
    {
      Log::Debug(THIS_MODULE, "Created at %1%", this);
    }

    virtual ~ModelImpl()
    {
      Log::Debug(THIS_MODULE, "Destroyed at %1%", this);
    }

    //new virtuals
    virtual unsigned CountItems() const
    {
      return static_cast<unsigned>(Container->CountItems());
    }

    virtual Playlist::Item::Data::Ptr GetItem(IndexType index) const
    {
      QMutexLocker locker(&Synchronizer);
      return Container->GetItemByIndex(index);
    }

    virtual void ForAllItems(Visitor& visitor) const
    {
      QMutexLocker locker(&Synchronizer);
      return Container->ForAllItems(visitor);
    }

    virtual void ForSpecifiedItems(const IndexSet& items, Visitor& visitor) const
    {
      QMutexLocker locker(&Synchronizer);
      return Container->ForSpecifiedItems(items, visitor);
    }

    virtual void Clear()
    {
      QMutexLocker locker(&Synchronizer);
      Container.reset(new PlayitemsContainer());
      NotifyAboutIndexChanged();
      FetchedItemsCount = 0;
    }

    virtual void RemoveItems(const Playlist::Model::IndexSet& items)
    {
      QMutexLocker locker(&Synchronizer);
      Container->RemoveItems(items);
      NotifyAboutIndexChanged();
      FetchedItemsCount = static_cast<int>(Container->CountItems());
    }

    virtual void MoveItems(const IndexSet& items, IndexType target)
    {
      Log::Debug(THIS_MODULE, "Moving %1% items to row %2%", items.size(), target);
      QMutexLocker locker(&Synchronizer);
      Container->MoveItems(items, target);
      NotifyAboutIndexChanged();
    }

    virtual void AddItem(Playlist::Item::Data::Ptr item)
    {
      QMutexLocker locker(&Synchronizer);
      Container->AddItem(item);
    }

    //base model virtuals

    /* Drag'n'Drop support*/
    virtual Qt::DropActions supportedDropActions() const
    {
      return Qt::MoveAction;
    }

    virtual Qt::ItemFlags flags(const QModelIndex& index) const
    {
      const Qt::ItemFlags defaultFlags = Playlist::Model::flags(index);

      const Playlist::Item::Data* const indexItem = index.isValid()
        ? static_cast<const Playlist::Item::Data*>(index.internalPointer())
        : 0;
      if (indexItem && indexItem->IsValid())
      {
         return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
      }
      else
      {
        return Qt::ItemIsDropEnabled | defaultFlags;
      }
    }

    virtual QStringList mimeTypes() const
    {
      QStringList types;
      types << ITEMS_MIMETYPE;
      return types;
    }

    virtual QMimeData* mimeData(const QModelIndexList& indexes) const
    {
      QMimeData* const mimeData = new QMimeData();
      QByteArray encodedData;
      QDataStream stream(&encodedData, QIODevice::WriteOnly);

      foreach (const QModelIndex& index, indexes)
      {
        if (index.isValid())
        {
          stream << index.row();
        }
      }

      mimeData->setData(ITEMS_MIMETYPE, encodedData);
      return mimeData;
    }

    virtual bool dropMimeData(const QMimeData* data, Qt::DropAction action, int /*row*/, int /*column*/, const QModelIndex& parent)
    {
      if (action == Qt::IgnoreAction)
      {
        return true;
      }

      if (!data->hasFormat(ITEMS_MIMETYPE))
      {
        return false;
      }

      const unsigned beginRow = parent.isValid()
        ? parent.row()
        : rowCount(EMPTY_INDEX);

      QByteArray encodedData = data->data(ITEMS_MIMETYPE);
      QDataStream stream(&encodedData, QIODevice::ReadOnly);
      Playlist::Model::IndexSet movedItems;
      while (!stream.atEnd())
      {
        IndexType idx;
        stream >> idx;
        movedItems.insert(idx);
      }
      if (movedItems.empty())
      {
        return false;
      }
      MoveItems(movedItems, beginRow);
      return true;
    }

    /* end of Drag'n'Drop support */

    virtual bool canFetchMore(const QModelIndex& /*index*/) const
    {
      QMutexLocker locker(&Synchronizer);
      return FetchedItemsCount < static_cast<int>(Container->CountItems());
    }

    virtual void fetchMore(const QModelIndex& /*index*/)
    {
      QMutexLocker locker(&Synchronizer);
      const int nextCount = static_cast<int>(Container->CountItems());
      beginInsertRows(EMPTY_INDEX, FetchedItemsCount, nextCount - 1);
      FetchedItemsCount = nextCount;
      endInsertRows();
    }

    virtual QModelIndex index(int row, int column, const QModelIndex& parent) const
    {
      if (parent.isValid())
      {
        return EMPTY_INDEX;
      }
      QMutexLocker locker(&Synchronizer);
      if (const Playlist::Item::Data::Ptr item = Container->GetItemByIndex(row))
      {
        const void* const data = static_cast<const void*>(item.get());
        return createIndex(row, column, const_cast<void*>(data));
      }
      return EMPTY_INDEX;
    }

    virtual QModelIndex parent(const QModelIndex& /*index*/) const
    {
      return EMPTY_INDEX;
    }

    virtual int rowCount(const QModelIndex& index) const
    {
      QMutexLocker locker(&Synchronizer);
      return index.isValid()
        ? 0
        : FetchedItemsCount;
    }

    virtual int columnCount(const QModelIndex& index) const
    {
      return index.isValid()
        ? 0
        : COLUMNS_COUNT;
    }

    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const
    {
      if (Qt::Horizontal == orientation)
      {
        const RowDataProvider& provider = Providers.GetProvider(role);
        return provider.GetHeader(section);
      }
      else if (Qt::Vertical == orientation && Qt::DisplayRole == role)
      {
        //item number is 1-based
        return QVariant(section + 1);
      }
      return QVariant();
    }

    virtual QVariant data(const QModelIndex& index, int role) const
    {
      if (!index.isValid())
      {
        return QVariant();
      }
      const int_t fieldNum = index.column();
      const int_t itemNum = index.row();
      QMutexLocker locker(&Synchronizer);
      if (const Playlist::Item::Data::Ptr item = Container->GetItemByIndex(itemNum))
      {
        const RowDataProvider& provider = Providers.GetProvider(role);
        //assert(static_cast<const void*>(item.get()) == index.internalPointer());
        return provider.GetData(*item, fieldNum);
      }
      return QVariant();
    }

    virtual void sort(int column, Qt::SortOrder order)
    {
      Log::Debug(THIS_MODULE, "Sort data in column=%1% by order=%2%",
        column, order);
      const bool ascending = order == Qt::AscendingOrder;
      if (PlayitemsContainer::Comparer::Ptr comparer = CreateComparerByColumn(column, ascending))
      {
        AsyncSorter = boost::thread(std::mem_fun(&ModelImpl::PerformSort), this, comparer);
      }
    }
  private:
    void PerformSort(PlayitemsContainer::Comparer::Ptr comparer)
    {
      OnSortStart();
      boost::scoped_ptr<PlayitemsContainer> tmpContainer;
      {
        QMutexLocker locker(&Synchronizer);
        tmpContainer.reset(new PlayitemsContainer(*Container));
      }
      tmpContainer->Sort(*comparer);
      {
        QMutexLocker locker(&Synchronizer);
        Container.swap(tmpContainer);
        NotifyAboutIndexChanged();
      }
      OnSortStop();
    }

    void NotifyAboutIndexChanged()
    {
      Playlist::Model::OldToNewIndexMap remapping;
      Container->GetIndexRemapping(remapping);
      Container->ResetIndexes();
      OnIndexesChanged(remapping);
      reset();
    }
  private:
    const DataProvidersSet Providers;
    mutable QMutex Synchronizer;
    int FetchedItemsCount;
    boost::scoped_ptr<PlayitemsContainer> Container;
    mutable boost::thread AsyncSorter;
  };
}

namespace Playlist
{
  Model::Model(QObject& parent) : QAbstractItemModel(&parent)
  {
  }

  Model::Ptr Model::Create(QObject& parent)
  {
    REGISTER_METATYPE(Playlist::Model::OldToNewIndexMap);
    return new ModelImpl(parent);
  }
}
