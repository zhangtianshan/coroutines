#ifndef INC_COROUTINES_LIST_H_
#define INC_COROUTINES_LIST_H_

#include <cassert>

namespace Coroutines {

  struct TListItem {
    TListItem* prev = nullptr;
    TListItem* next = nullptr;
  };

  struct TList {
    TListItem* first = nullptr;
    TListItem* last = nullptr;
    bool empty() const { return first == nullptr; }
    void append(TListItem* item) {
      assert(item);
      assert(item->next == nullptr);
      assert(item->prev == nullptr);
      if (!first) {
        assert(last == nullptr);
        first = item;
      } else {
        assert(last->next == nullptr);
        last->next = item;
      }
      item->prev = last;
      last = item;
    }
    void detach(TListItem* item) {
      if (item->prev)
        item->prev->next = item->next;
      else if( first == item )
        first = item->next;
      if (item->next)
        item->next->prev = item->prev;
      else if (last == item)
        last = item->prev;
    }
    template< class T >
    T* detachFirst() {
      if (!first)
        return nullptr;
      auto item = first;
      detach(item);
      return static_cast<T*>(item);
    }
  };

}

#endif
