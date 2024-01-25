#pragma once

#include <unordered_map>
#include <string>

template <typename T = size_t>
class CounterMap {
public:
    CounterMap(void)                                   = default;
    CounterMap(const CounterMap<T> &other)             = delete;
    CounterMap(CounterMap<T> &&other)                  = delete;
    CounterMap &operator=(const CounterMap<T> &other)  = delete;
    CounterMap &operator=(CounterMap<T> &&other)       = delete;

    T get(const std::string &key);
    void bump(const std::string &key, size_t increment=1);
    size_t get_num_counters(void) const;
    void clear_counter(const std::string &key);
    void forget_counter(const std::string &key);
    void clear(void);

private:
    std::unordered_map<std::string, T> m_map;
};

template <typename T>
T CounterMap<T>::get(const std::string &key){
    T v = m_map[key];
    return v;
}

template <typename T>
void CounterMap<T>::bump(const std::string &key, size_t increment){
    m_map[key] += increment;
}

template <typename T>
size_t CounterMap<T>::get_num_counters(void) const{
    return m_map.size();
}

template <typename T>
void CounterMap<T>::clear(void){
    m_map.clear();
}

/*
 * Clear the counter to 0 but do not remove its entry from the map. */
template <typename T>
void CounterMap<T>::clear_counter(const std::string &key){
    m_map[key] = 0;
}

/*
 * Remove the counter from the map such that get_num_counters would
 * be decremented after its removal. */
template <typename T>
void CounterMap<T>::forget_counter(const std::string &key){
    auto found = m_map.find(key);
    if (found != m_map.end()){
        m_map.erase(found);
    }
}


