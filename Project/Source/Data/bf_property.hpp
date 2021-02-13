/******************************************************************************/
/*!
 * @file   bf_property.hpp
 * @author Shareef Abdoul-Raheem (https://blufedora.github.io/)
 * @brief
 *   Simple system for managing callbacks for changing data members.
 *   This is meant to be light-weight with not strong opinions on memory
 *   allocation strategy.
 *
 * @date      2021-02-13
 *
 * @copyright Copyright Shareef Abdoul-Raheem (c) 2021
 */
/******************************************************************************/
#ifndef BF_PROPERTY_HPP
#define BF_PROPERTY_HPP

namespace bf
{
  //
  // IStoragePolicy<T>(interface):
  //   copy-assignable
  //   T    get()
  //   void set(T)
  //

  namespace StoragePolicy
  {
    //
    // This is a simple storage policy where the property owns the data.
    //
    template<typename T>
    struct Inline
    {
      T value;

      Inline(const T& initial_value = {}) :
        value{initial_value}
      {
      }

      inline T    get() const { return value; }
      inline void set(const T& new_value) { value = new_value; }
    };

    //
    // References memory located somewhere else, make sure the pointer stays alive.
    //
    template<typename T>
    struct Remote
    {
      T* value;

      Remote(T* ref) :
        value{ref}
      {
      }

      inline T    get() const { return *value; }
      inline void set(const T& new_value) { *value = new_value; }
    };
  }  // namespace StoragePolicy

  template<typename T>
  struct base_property;

  //
  // Usage Notes:
  //   - A `IPropChangeListener`'s memory is managed by the user, make sure it
  //     does not die before the property it is attached to dies.
  //   - A `IPropChangeListener` can only be attached to a single property at
  //     any given point in time.
  //
  template<typename T>
  struct IPropChangeListener
  {
    using OnValueAssignedFn = void (*)(IPropChangeListener<T>* self, const T& old_value, const T& new_value);

    OnValueAssignedFn       onValueAssigned = nullptr;
    IPropChangeListener<T>* next            = nullptr;
    IPropChangeListener<T>* prev            = nullptr;
    base_property<T>*       prop            = nullptr;

    IPropChangeListener(OnValueAssignedFn callback) :
      onValueAssigned{callback},
      next{nullptr},
      prev{nullptr},
      prop{nullptr}
    {
    }
  };

  template<typename T>
  struct base_property
  {
    IPropChangeListener<T>* change_Listeners = nullptr;

    void addListener(IPropChangeListener<T>* Listener)
    {
      if (Listener->prop)
      {
        Listener->prop->removeListener(Listener);
      }

      Listener->prop = this;
      Listener->prev = nullptr;
      Listener->next = change_Listeners;

      if (change_Listeners)
      {
        change_Listeners->prev = Listener;
      }

      change_Listeners = Listener;
    }

    void removeListener(IPropChangeListener<T>* Listener)
    {
      if (Listener->prop == this)
      {
        Listener->prop = nullptr;

        IPropChangeListener<T>* const Listener_prev = Listener->prev;
        IPropChangeListener<T>* const Listener_next = Listener->next;

        if (Listener_prev)
        {
          Listener_prev->next = Listener_next;
        }
        else
        {
          change_Listeners = Listener_next;
        }

        if (Listener_next)
        {
          Listener_next->prev = Listener_prev;
        }
      }
    }

    void removeAllListeners()
    {
      while (change_Listeners)
      {
        removeListener(change_Listeners);
      }
    }
  };

  template<typename T, template<typename> class StoragePolicy = StoragePolicy::Inline>
  struct property : public base_property<T>
  {
    mutable StoragePolicy<T> storage;

    property(const StoragePolicy<T>& storage = {}) :
      base_property<T>{},
      storage{storage}
    {
    }

    property(const property& rhs)       = delete;
    property(property&& rhs)            = delete;
    property<T, StoragePolicy>& operator=(const property& rhs) = delete;
    property<T, StoragePolicy>& operator=(property&& rhs) = delete;

    ~property()
    {
      removeAllListeners();
    }

    operator T() const { return get(); }

    T get() const { return storage.get(); }

    property<T, StoragePolicy>& operator=(const T& rhs)
    {
      set(rhs);

      return *this;
    }

    void set(const T& new_value, bool notify = true)
    {
      // We only need to make a copy of the old value if there are change Listeners.
      if (this->change_Listeners && notify)
      {
        const T& old_value = get();
        storage.set(new_value);
        notifyChange(old_value, new_value);
      }
      else
      {
        storage.set(new_value);
      }
    }

    void notifyChange(const T& old_value, const T& new_value) const
    {
      IPropChangeListener<T>* Listener = this->change_Listeners;

      while (Listener)
      {
        IPropChangeListener<T>* const next = Listener->next;
        Listener->onValueAssigned(Listener, old_value, new_value);
        Listener = next;
      }
    }
  };
}  // namespace bf

#endif /* BF_PROPERTY_HPP */
