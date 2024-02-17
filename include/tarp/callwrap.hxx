#pragma once

#include <tuple>


namespace tarp {

/*
 * Wrap a callable and bind it to the specified parameters;
 *
 * NOTE: the functionality is much like std::bind. Not much use in the
 * presence of lambdas.
 *
 * Template arguments:
 * --> CALLABLE
 * A callable object e.g. lambda, C-style function, functor, etc.
 * --> vargs
 * Any number of arbitrary parameters to be pased to the CALLABLE when
 * invoked. The CALLABLE's signature must match these parameters.
 */
#ifdef __cpp_lib_apply
template <typename CALLABLE, typename ...vargs>
class CallWrapper{
public:
    using type = CallWrapper<CALLABLE, vargs...>;

	CallWrapper(CALLABLE callable, vargs...args)
    	: m_vargs(std::forward<vargs>(args)...), m_callable(std::forward<CALLABLE>(callable))
	{}

    void operator()(void){
        std::apply(m_callable, m_vargs);
    }

    auto operator()(vargs... args){
        return m_callable(std::forward<vargs>(args)...);
    }

private:
	std::tuple<vargs...> m_vargs;
	CALLABLE m_callable;
};

#else
template <typename CALLABLE, typename ...vargs>
class CallWrapper{
public:
	CallWrapper(CALLABLE callable, vargs...args)
    	: m_vargs(std::forward<vargs>(args)...), m_callable(std::forward<CALLABLE>(callable))
	{}

	void operator()(void){
        auto tuplen = std::tuple_size<decltype(m_vargs)>();
        unpack_tuple_and_call(m_vargs, std::make_index_sequence<tuplen>());
	}

    template <typename ...params>
    auto operator()(params&&... args){
        return m_callable(std::forward<params>(args)...);
    }

private:

    template<std::size_t... INTSEQ>
    void unpack_tuple_and_call(const std::tuple<vargs...>& tuple, std::index_sequence<INTSEQ...>)
    {
      m_callable(std::get<INTSEQ>(tuple)...);
    }

	std::tuple<vargs...> m_vargs;
	CALLABLE m_callable;
};

#endif  /* ifdef __cpp_lib_apply */

template <typename CALLABLE, typename ...vargs>
auto wrap(CALLABLE &&f, vargs&&... args){
    CallWrapper<CALLABLE, decltype(args)...> cw(std::forward<CALLABLE>(f), std::forward<vargs>(args)...);
    return cw;
}

template <typename CLASS, typename MEMBF>
auto wrapmf(MEMBF f, CLASS *obj){
    return [=](auto&&... lambda_params){
        return (obj->*f)(std::forward<decltype(lambda_params)>(lambda_params)...);
    };
}

}  /* namespace tarp */

