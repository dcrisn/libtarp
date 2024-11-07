#pragma once

#include <functional>
#include <string>
#include <stdexcept>

#include <cstdint>
#include <cmath>

#include <tarp/math.h>

/*
 * This header defines various functions and classes that implement, among
 * others, concepts from the realm of functional programming.
 * For example, folds, map, reduce etc.
 */

namespace tarp {

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

//

/**
 * Clears a specified number of low-order contiguous digits.
 *
 * @param T The type of the input. Must be integral type.
 */
template<typename T>
class clear_digits {
    static_assert(std::is_integral_v<T> or std::is_floating_point_v<T>,
                  "invalid input type T, must be integral");

public:
    /**
     * @param num_low_order_digits_to_clear How many digits
     *  (starting from rightmost) to zero out.
     */
    clear_digits(std::uint8_t num_low_order_digits_to_clear)
        : m_num_digits(num_low_order_digits_to_clear) {}

    /**
     * @param input Integer to transform
     * @return input with num_low_order_digits_to_clear zeroed out.
     *
     * NOTE: this **will** wrap around and return unexpected results for
     * large values of m_num_digits. Specifically, if 10^m_num_digits
     * does not fit in the type T.
     */
    T operator()(T input) const {
        std::string s {std::to_string(input)};

        // clear all the digits --> avoid potential intpow wraparound
        // if the value of num_digits_to_clear is gt
        // std::numeric_limits<std::uint32_t>::max.
        if (m_num_digits > s.size()) {
            return 0;
        }

        // Get number of bits needed to represent 10^m_num_digits.
        // log_2(x) = log_10(x) / log_10(2); here log_10(x) is obviously
        // simply m_num_digits, since log_10(10^m_num_digits) = m_num_digits.
        double num_bits_needed = std::ceil(m_num_digits / std::log10(2.0f));

        double num_bits_avail = sizeof(T) * 8;
        if (num_bits_needed > num_bits_avail) {
            throw std::invalid_argument(
              "Excessive input values would cause wraparound");
        }

        unsigned divisor = tarp::math::intpow(10, m_num_digits);

        // this will truncate the num_digits_to_clear leas-significant digits.
        // The division removes these digits; the multiplication reinserts
        // them, but zeroed out.
        input = (input / divisor) * divisor;
        return input;
    }

private:
    std::uint8_t m_num_digits;
};

// Multiply input by a scale value.
template<typename T>
class scaler {
public:
    scaler(T multiplier) : m_scaler(multiplier) {}

    T operator()(T input) const { return input * m_scaler; }

private:
    T m_scaler;
};



} /* namespace tarp */
