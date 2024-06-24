#pragma once

#include <cstdint>

namespace tarp::sched::filters {

template<typename filterable_t>
class Filter {
public:
    explicit Filter(std::uint32_t filter_id)
        : m_id(filter_id), m_inverted(false) {}

    std::uint32_t get_id() const { return m_id; }

    void set_inverted(bool match_is_negated) { m_inverted = match_is_negated; }

    bool matches(filterable_t &target) const {
        auto match_found = do_match(target);

        if (m_inverted) {
            return !match_found;
        }

        return match_found;
    }

private:
    virtual bool do_match(filterable_t &target) = 0;

    uint32_t m_id;
    bool m_inverted {false};
};

/* A filter that always indiscriminately matches everything. */
template<typename filterable_t>
class MatchAll : public Filter<filterable_t> {
public:
    virtual bool matches(filterable_t &) const override { return true; };
};


}  // namespace tarp::sched::filters
