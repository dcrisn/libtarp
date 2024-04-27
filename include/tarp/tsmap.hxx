#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <optional>

#include <tarp/cxxcommon.hxx>

namespace tarp {
/*
 * Thread-safe map implementation.
 *
 * This is a simple wrapper around an STL map type that wraps
 * its methods so that they are protected by mutex guards to ensure
 * thread safety.
 */
template<typename key_type,
         typename value_type,
         template<typename, typename> class map_t = std::map>
class tsmap final {
public:
    DISALLOW_COPY_AND_MOVE(tsmap);

    tsmap(void) = default;

    std::size_t size(void) const {
        LOCK(m_mtx);
        return m_map.size();
    }

    bool empty(void) const {
        LOCK(m_mtx);
        return m_map.empty();
    };

    void clear(void) {
        LOCK(m_mtx);
        m_map.clear();
    }

    bool has(const key_type &k) const {
        LOCK(m_mtx);
        auto found = m_map.find(k);
        if (found == m_map.end()) {
            return false;
        }
        return true;
    }

    std::optional<value_type> get(const key_type &k) {
        LOCK(m_mtx);
        auto found = m_map.find(k);
        if (found == m_map.end()) {
            return {};
        }

        std::optional<value_type> res = std::move(found->second);
        m_map.erase(found);
        return res;
    }

    template<typename F>
    bool insert_if_not_found(const key_type &k, F &&f) {
        LOCK(m_mtx);

        auto found = m_map.find(k);
        if (found != m_map.end()) {
            return false;  // not inserted
        }

        auto res = m_map.emplace(f());
        return res.second;
    }

    template<typename key_t, typename value_t>
    std::enable_if_t<std::is_same_v<key_t, key_type>, bool>
    insert(key_t &&k, value_t &&v, bool replace_if_exists = false) {
        return insert_pair(std::make_pair(std::forward<key_type>(k),
                                          std::forward<value_type>(v)),
                           replace_if_exists);
    }

    bool insert(std::pair<key_type, value_type> &&entry,
                bool replace_if_exists) {
        return insert_pair(std::move(entry), replace_if_exists);
    }

    bool insert(const std::pair<key_type, value_type> &entry,
                bool replace_if_exists) {
        return insert_pair(entry, replace_if_exists);
    }

    // call func(k) if k is found;
    template<typename F>
    bool apply(const key_type &k, F &&func) {
        return apply(this, k, func);
    }

    template<typename F>
    bool apply(const key_type &k, F &&func) const {
        return apply(this, k, func);
    }

    // call for each element in map; must not remove or insert into the map
    template<typename F>
    bool apply(F &&func) const {
        return apply(this, func);
    }

    // call for each element in map; must not remove or insert into the map
    template<typename F>
    bool apply(F &&func) {
        return apply(this, func);
    }

    bool erase(const key_type &k) {
        LOCK(m_mtx);

        bool erased {false};

        auto found = m_map.find(k);
        if (found != m_map.end()) {
            m_map.erase(found);
            erased = true;
        }

        return erased;
    }

    template<typename F>
    bool maybe_erase(const key_type &k, F &&func) {
        LOCK(m_mtx);

        auto found = m_map.find(k);
        if (found == m_map.end()) {
            return false;
        }

        if (func(found->second) == false) {
            return false;
        }

        m_map.erase(found);
        return true;
    }

    template<typename F>
    std::size_t maybe_erase(F &&func) {
        LOCK(m_mtx);

        std::size_t erased_count {0};

        for (auto it = m_map.begin(); it != m_map.end();) {
            if (func(*it) == false) {
                ++it;
                continue;
            }
            it = m_map.erase(it);
            ++erased_count;
        }

        return erased_count;
    }

    template<typename ThisPtr, typename F>
    static bool apply(ThisPtr this_ptr, const key_type &k, F &&func) {
        LOCK(this_ptr->m_mtx);

        auto found = this_ptr->m_map.find(k);
        if (found == this_ptr->m_map.end()) {
            return false;
        }

        std::invoke(func, found->second);
        return true;
    }

    template<typename ThisPtr, typename F>
    static bool apply(ThisPtr this_ptr, F &&func) {
        LOCK(this_ptr->m_mtx);

        // IDEA: instead of void, we can aggregate the return values of func in
        // some way, or return the last value etc; same issue you get in a
        // signal framework when a signal invoked mutiple callbacks that return
        // values.

        if (this_ptr->m_map.empty()) {
            return false;
        }

        std::for_each(this_ptr->m_map.begin(), this_ptr->m_map.end(), func);
        return true;
    }

private:
    template<typename T>
    bool insert_pair(T &&entry, bool replace_if_exists) {
        LOCK(m_mtx);

        if (replace_if_exists) {
            auto found = m_map.find(std::get<key_type>(entry));
            if (found != m_map.end()) {
                m_map.erase(found);
            }
        }

        auto [it, ok] = m_map.emplace(std::forward<decltype(entry)>(entry));
        return ok;
    }

    map_t<key_type, value_type> m_map;
    mutable std::mutex m_mtx;
};

} /* namespace tarp */
