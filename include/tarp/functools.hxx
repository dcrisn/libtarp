#pragma once

#include <functional>
#include <type_traits>

/*
 * This header defines various functions and classes that implement, among
 * others, concepts from the realm of functional programming.
 * For example, folds, map, reduce etc.
 */

namespace tarp {

//

// --------------------------------------------------------------------
// Signature composition.
// Template that lets you compose a function signature (R(PARAMS))
// from discrete elements. This is useful for example when the types of the
// parameters and/or return value are generated dynamically.
// Another special use case is when specifying the signature of a function
// that takes void as an argument. 'void' is not an actual type that is
// valid as a function argument ( e.g. int(void) ) and will produce a
// compilation error. The template is specialized so that <R, void>
// produces R() instead of R(void) to placate the compiler.
// --------------------------------------------------------------------

// Signature of a function that return R and takes an arbitrary number of
// arguments of arbitrary types.
template<typename R, typename... args>
struct signature_comp {
    using signature = R(args...);
};

// Signature of a function that takes a single argument T and returns R.
template<typename R, typename T>
struct signature_comp<R, T> {
    using return_type = R;
    using signature = R(T);
};

// Signature of a function that takes void (i.e. no arguments) amd returns R.
template<typename R>
struct signature_comp<R, void> {
    using signature = R();
};

template<typename R, typename... args>
using signature_comp_t = typename signature_comp<R, args...>::signature;

//

// --------------------------------------------------------------------
// Signature decomposition.
// Template that lets you decompose a function signature (R(PARAMS)) into
// components.
// --------------------------------------------------------------------

/*
 * Helper template to extract return value type from a function
 * signature by means of partial template specialization.
 * The type of the return value is stored in the type member
 * 'return_type'.
 * The function signature is stored in the type member 'signature'. For member
 * functions, it omits the type of the instance and converts the signature
 * to a normal non-member function signature. That is:
 *     int(T::*(std::string)) => int(std::string).
 */
template<typename signature>
struct signature_decomp;

template<typename R, typename... args>
struct signature_decomp<R(args...)> {
    using return_type = R;
    using signature = R(args...);
};

// member function
template<typename obj, typename R, typename... args>
struct signature_decomp<R(obj::*)(args...)> {
    using return_type = R;
    using signature = R(args...);
    using constness = std::false_type;
};

// Const member function
template<typename obj, typename R, typename... args>
struct signature_decomp<R(obj::*)(args...) const> {
    using return_type = R;
    using signature = R(args...);
    using constness = std::true_type;
};

template<typename signature>
using signature_decomp_return_t =
  typename signature_decomp<signature>::return_type;

template<typename signature>
using signature_decomp_signature_t =
  typename signature_decomp<signature>::signature;

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
