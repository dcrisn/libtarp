#pragma once

#include <functional>
#include <type_traits>

/*
 * This header defines various functions and classes that implement, among
 * others, concepts from the realm of functional programming.
 * For example, folds, map, reduce etc.
 */

namespace tarp {

/*
 * Helper template to extract return value type from a function
 * signature by means of partial template specialization.
 * The type of the return value is stored in the typedef member
 * 'return_type'.
 */
template<typename signature>
struct func;

template<typename R, typename... args>
struct func<R(args...)> {
    using return_type = R;
};

template<typename obj, typename R, typename... args>
struct func<R (obj::*)(args...)> {
    using return_type = R;
};

template<typename signature>
using func_return_type = typename func<signature>::return_type;

///////////////////////////////////////////////////////////////



/*
 * Classes that can be used as 'reducers'. They expose a
 * .process() method that when called on each element in a
 * collection reduces the collection to a single summary value.
 * The value can then be obtained using .get();
 */
namespace reduce {

// Abstract base class defining an interface common to all reducers.
// R = return value type
// T = input value type
template<typename R, typename T>
class reducer {
public:
    virtual void process(T) = 0;
    virtual R get(void) const = 0;
};

// Reducer that processes nothing and returns nothing (void).
// This if for for the special case when the input or output types are
// void: then ignore both template parameters and make everything void.
// See tarp/signal.hxx for how it is used.
template<typename = void, typename = void>
class void_reducer {
public:
    void process(void) {};
    void get(void) const {};
};

// Return the very last element processed.
// NOTE: second param unused; only present in order to respect the
// template signature of the reduce abstract base class.
template<typename T, typename = void>
class last : public reducer<T, T> {
public:
    virtual void process(T value) override { m_value = value; }

    virtual T get(void) const { return m_value; }

private:
    T m_value {};
};

// Return the very first element processed.
// NOTE: 2nd param unused
template<typename T, typename = void>
class first : public reducer<T, T> {
public:
    virtual void process(T value) override {
        if (m_set) return;
        m_value = value;
        m_set = true;
    }

    virtual T get(void) const override { return m_value; }

private:
    bool m_set {false};
    T m_value {};
};

// Return the smallest element processed.
// NOTE: 2nd param unused
template<typename T, typename = void>
class min : public reducer<T, T> {
public:
    virtual void process(T value) override {
        if (!m_set) {
            m_set = true;
            m_value = value;
            return;
        }
        m_value = std::min(value, m_value);
    }

    virtual T get(void) const override { return m_value; }

private:
    bool m_set {false};
    T m_value {};
};

// Return the largest element processed.
// NOTE: 2nd param unused
template<typename T, typename = void>
class max : public reducer<T, T> {
public:
    virtual void process(T value) override {
        if (!m_set) {
            m_set = true;
            m_value = value;
            return;
        }

        m_value = std::max(value, m_value);
    }

    virtual T get(void) const override { return m_value; }

private:
    bool m_set {false};
    T m_value {};
};

// Return the sum of all elements procesed.
// NOTE: 2nd template param unused.
template<typename T, typename = T>
class sum : public reducer<T, T> {
public:
    virtual void process(T value) override { m_sum += value; }

    virtual T get(void) const override { return m_sum; }

private:
    bool m_set {false};
    T m_sum {};
};

/*
 * Returns a container filled with all elements processed.
 *
 * NOTE:
 * Expects R to be a container with a push_back() method and that stores
 * elements of type T.
 *
 * EXAMPLE instantiation:
 *   list<std::vector<int>, int> r;
 */
template<typename R, typename T>
class list : public reducer<R, T> {
public:
    virtual void process(T value) override { m_list.push_back(value); }

    virtual R get(void) const override { return m_list; }

private:
    R m_list {};
};


} /* namespace reduce */

/*
 * Fold a collection by running all elements through a reducer.
 *
 * (1)
 * Instantiate a reducer of the specified type;
 * The reducer's process() method takes a type T as input and
 * the reducer's get() method returns a type R as output.
 *
 * (2)
 * Initialize the reducer's state with the specified seed value.
 *
 * (3)
 * Iterate over a collection of the specified type that stores
 * elements of type T and pass each element to the reducer's
 * process() method.
 * Then return the reducer's output.
 */
template<typename R,
         typename T,
         template<typename>
         typename iterable,
         template<typename, typename>
         typename reducer>
R fold(const iterable<T> &inputs, T seed = {}) {
    reducer<R, T> r; /* (1) */
    r.process(seed); /* (2) */

    /* (3) */
    for (auto it = inputs.begin(); it != inputs.end(); ++it) {
        r.process(std::forward<decltype(*it)>(*it));
    }
    return r.get();
}

} /* namespace tarp */
