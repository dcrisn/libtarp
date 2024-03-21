#pragma once

#include <cstdlib>
#include <vector>

namespace tarp {

/*
 * Split the inputs sequence into separate ranges i.e. chunks
 * of the specified length. Each chunk will be stored in a separate
 * sequence of the same type as the inputs sequence and CB will
 * be called with each chunk sequence as an r-value reference.
 *
 * If pad_rest = true, then if the length of the inputs sequence is
 * not a multiple of chunksz the very last chunk will be padded to
 * make up the length. Otherwise no padding is applied and the last
 * chunk may therefore be shorted than the others.
 * NOTE: If padding is applied, then the padding elements will be
 * default-constructed and value-initialized -- or, if pad_elem
 * is specified, with copies of pad_elem.
 */
template<template<typename> typename sequence,
         typename T,
         typename CB,
         typename = std::enable_if_t<std::is_invocable_v<CB, sequence<T> &&>>>
void chunk_sequence(const sequence<T> &inputs,
                    size_t chunksz,
                    CB callback,
                    bool pad_rest = false,
                    T pad_elem = {}) {
    size_t num_chunks = inputs.size() / chunksz;
    num_chunks += inputs.size() % chunksz ? 1 : 0;

    size_t num_left = inputs.size();

    for (size_t i = 0; i < num_chunks; ++i) {
        size_t num_to_write = num_left > chunksz ? chunksz : num_left;
        size_t offset = chunksz * i;

        sequence<T> arr {inputs.begin() + offset,
                         inputs.begin() + offset + num_to_write};

        size_t num_to_pad = chunksz - num_to_write;
        if (pad_rest && num_to_pad > 0) {
            for (size_t i = 0; i < num_to_pad; ++i) {
                arr.push_back(pad_elem);
            }
        }

        callback(std::move(arr));

        num_left -= num_to_write;
    }
}

template<template<typename> typename sequence, typename T>
std::vector<sequence<T>> chunk_sequence(const sequence<T> &inputs,
                                        size_t chunksz,
                                        size_t pad_rest = false,
                                        T pad_elem = {}) {
    std::vector<sequence<T>> outputs;

    auto chunk_handler = [&outputs](auto &&chunk) {
        outputs.push_back(std::forward<decltype(chunk)>(chunk));
    };

    chunk_sequence(inputs, chunksz, chunk_handler, pad_rest, pad_elem);

    return outputs;
}

}  // namespace tarp
