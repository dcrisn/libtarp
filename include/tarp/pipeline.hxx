#pragma once

#include <algorithm>
#include <cassert>
#include <deque>
#include <initializer_list>
#include <numeric>
#include <optional>
#include <queue>
#include <stdexcept>
#include <type_traits>

#include <tarp/floats.h>
#include <tarp/signal.hxx>

#include <iostream>

namespace tarp {

//
// A pipeline is comprised of one or more stages. The input is passed to the
// first stage and is normally read from the last stage, after all the
// transformations applied by all the intermediate stages.
//
// Intermediate outputs (INTM) may also be read after any stage. This
// conceptually resembles a multi-stage electronic circuit (e.g. flip-flop
// shift register) with a test point at the output of each stage.
//
// clang-format off
//
// The following diagram illustrates the concept.
//
// .-------------------------------------------------------------------------------.
// |         ___________       __________               __________                 |
// |        |           |     |          |             |          |                |
// |  .-->  | STAGE 1   | ==> | STAGE 2  | ... ==> ... | STAGE N  |  <--.          |
// |  |     *-----------*  |  *----------*      |      *----------*     |          |
// |  |                    |                    |                       |          |
// |  |                    |                    |                       |          |
// | INPUT               INTM 1               INTM2                   OUTPUT       |
// |                                                                               |
// |_______________________________________________________________________________|
//
// clang-format on
//
// NOTE: not every input corresponds to an immediate output. Transformations by
// any given stage are largely arbitrary; an input can correspond to multiple
// outputs and vice-versa.
//
// NOTE: stages must be consistent in terms of their inputs and outputs. That is
// to say, stage 2 must take as input the output of stage 1. So arbitrary stages
// cannot be linked at random.
//

// IN=input type, OUT=output type
template<typename IN, typename OUT = IN>
class PipelineStage {
public:
    /*
     * --> buffer_outputs
     * If buffer_outputs=true, then the outputs are kept in a FIFO; get()
     * must be called for each element in turn to remove it. Otherwise if
     * buffer_outputs=false, then each output is signaled and propagated
     * to the next stage as appropriate *but* it is not kept in
     * the FIFO (which is therefore always empty and has_output will
     * always return false).
     *
     * --> trim_output
     * For every
     */
    PipelineStage(bool buffer_outputs = true, bool trim_output = true)
        : m_buffer_outputs(buffer_outputs)
        , m_trim_output(trim_output)
        , m_next_stage(nullptr) {}

    void set_buffered(bool buffered) {
        m_buffer_outputs = buffered;
        if (!buffered) m_output.clear();
    }

    /* connect a new stage to the output of the current one. */
    void join(PipelineStage<OUT> &stage) { m_next_stage = &stage; }

    /* Remove any connection to following stages and terminate
     * the pipeline at the curent stage. */
    void make_terminal(void) { join(nullptr); };

    auto &get_signal(void) { return m_new_output_signal; }

    bool has_output(void) const { return not m_output.empty(); }

    virtual OUT get(void) {
        if (!has_output()) {
            throw std::logic_error("Illegal .get() call: no outputs");
        }

        OUT out = m_output.front();
        m_output.pop();
        return out;
    };

    virtual const OUT &peek(void) const {
        if (!has_output()) {
            throw std::logic_error("Illegal .peek() call: no outputs");
        }
        return m_output.front();
    }

    void apply(IN value) {
        bool value_can_be_dropped = false;
        std::optional<OUT> output;
        process_input(value, output, value_can_be_dropped);

        /* no output this time */
        if (!output.has_value()) return;

        /* discardable value and class was asked not to keep these */
        if (m_trim_output && value_can_be_dropped) return;

        if (m_buffer_outputs) m_output.push(output.value());
        m_new_output_signal.emit(output.value());
        if (m_next_stage) m_next_stage->apply(output.value());
    }

    void operator()(IN value) { apply(value); };

protected:
    /*
     * --> input
     * Value to be processed.
     *
     * --> output
     * The output value obtained after processing the input value somehow.
     * NOTE this is an out-parameter and is a pointer. It can be left unset
     * if the current call does not produce an output. This may be the case when
     * the relation of input to output is not 1-to-1 (eg as with moving averages
     * and such).
     *
     * --> output_is_discardable
     * This is set to true when the output is not in fact normally of interest
     * and can be discarded when trim_output=true. Conversely, if this is false,
     * the output is *always* returned to users of this class.
     * As an example of when output_is_discardable may be set to true, consider
     * a band-pass filter that allows through values within the band-pass but
     * sets to 0 all other values. The 0s are not normally of interest and are
     * more of an artifcat that can be discarded.
     */
    virtual void process_input(IN input,
                               std::optional<OUT> &output,
                               bool &output_is_discardable) = 0;

private:
    bool m_buffer_outputs;
    bool m_trim_output;
    PipelineStage<OUT> *m_next_stage;
    std::queue<OUT> m_output;
    tarp::signal<void(OUT)> m_new_output_signal;
};

// TODO:
// a class that can hold a collection of stages and only exposes the input of
// the first and the output of the last stage.
// make each stage a unique_pointer, all stored seqentially in a
// vector<s1,s2,..slast>
class Pipeline {};

//
// A memoryless system computes the output only from the current input.
// This filter is therefore a basic current-value-only filter that filters
// out values outside the reference_value +/- tolerance range. It does *not*
// filter the values as a time series from the pov of their frequency/trend.
template<typename T>
class memoryless_bandpass_filter : public PipelineStage<T, T> {
public:
    memoryless_bandpass_filter(T low_cutoff,
                               T high_cutoff,
                               bool trim_output,
                               bool buffer_outputs)
        : PipelineStage<T, T>(buffer_outputs, trim_output)
        , m_low_cutoff(low_cutoff)
        , m_high_cutoff(high_cutoff) {
        if (low_cutoff > high_cutoff)
            throw std::logic_error(
              "Invalid memoryless_bandpass_filter configuration"
              " (low_cutoff > high_cutoff)");
    }

    memoryless_bandpass_filter(T reference_value, T tolerance, bool trim_output)
        : memoryless_bandpass_filter(reference_value - tolerance,
                                     reference_value + tolerance,
                                     trim_output,
                                     false) {
        if (std::is_unsigned<T>()) {
            // a configuration with tolerance > reference_value would be
            // nonsensical; consider reference_value=2, with tolerance=3. The
            // low cutoff would underflow and wrap around for unsigned integers.
            if (tolerance < reference_value)
                std::logic_error(
                  "Invalid memoryless_bandpass_filter configuration "
                  "tolerance > reference_value");
        }
    }

private:
    virtual void
    process_input(T input, std::optional<T> &output, bool &discardable) {
        if (input >= m_low_cutoff and input <= m_high_cutoff) {
            output = input;
            discardable = false;
            return;
        }

        output = 0;
        discardable = true;
    }

    T m_low_cutoff;
    T m_high_cutoff;
};

/*
 * weighted moving average */
template<typename IN, typename OUT>
class wma : public PipelineStage<IN, OUT> {
public:
    wma(size_t window_width, std::initializer_list<float> weights) {
        if (window_width == 0) {
            throw std::logic_error("Invalid window of 0 width");
        }

        if (weights.size() > window_width) {
            throw std::invalid_argument("The number of specified weights "
                                        "exceeds the width of the window");
        }

        for (auto &weight : weights) {
            m_weights.push_back(weight);
        }

        auto &v = m_weights;
        float total_weight = std::accumulate(v.begin(), v.end(), 0.0);

        // if this is the case, equally distribute remaining weight
        // over the rest of the window (assuming there is *any* weight
        // left to make up a whole; otherwise if the specified weights
        // already make up a whole, then the rest of the window will have
        // weight 0 (weird, but valid). Otherwise if we have *already* got
        // more than a whole unit, then this is a logic error.
        if (m_weights.size() < window_width) {
            if (total_weight < 1.0) {
                float remaining_weight = 1.0f - total_weight;
                unsigned elems = window_width - m_weights.size();
                for (size_t i = 0; i < elems; ++i) {
                    float w = remaining_weight / elems;
                    m_weights.push_back(w);
                    total_weight += w;
                }
            }
        }

        if (dbcmp(total_weight, 1.0, -1) != 0) {
            throw std::logic_error("Weights do not make up a whole: " +
                                   std::to_string(total_weight));
        }
    }

    virtual void process_input(IN input,
                               std::optional<OUT> &output,
                               bool &output_is_discardable) override {
        size_t window_width = m_weights.size();
        m_buffer.push_back(input);

        if (m_buffer.size() > window_width) {
            m_buffer.pop_front();
        }

        if (m_buffer.size() == window_width) {
            OUT avg = 0;
            for (size_t i = 0; i < m_buffer.size(); ++i) {
                avg += m_buffer[i] * m_weights[i];
            }
            output = avg;
            output_is_discardable = false;
        }
    }

private:
    std::deque<IN> m_buffer;
    std::vector<float> m_weights;
};

/*
 * Simple moving average */
template<typename IN, typename OUT>
class sma : public PipelineStage<IN, OUT> {
public:
    sma(size_t window_width)
        : PipelineStage<IN, OUT>(true, false)
        , m_fastpath(false)
        , m_window_width(window_width)
        , m_weight(1.0 / window_width)
        , m_sma(0.f) {
        if (window_width == 0) {
            throw std::logic_error("Invalid window of 0 width");
        }
    }

    virtual void process_input(IN input,
                               std::optional<OUT> &output,
                               bool &output_is_discardable) override {
        if (!m_fastpath) {
            if (m_buffer.size() < m_window_width) {
                m_buffer.push_back(input);
            }

            /* once we have filled the buffer and calculated the first
             * SMA, then we only need 3 pieces of information to keep the
             * SMA running: the sma and the latest and newest elements. */
            if (m_buffer.size() == m_window_width) {
                std::for_each(
                  m_buffer.begin(), m_buffer.end(), [this](IN elem) {
                      m_sma += m_weight * elem;
                  });
                output = m_sma;
                output_is_discardable = false;
                m_fastpath = true;
            }
            return;
        }

        // else fastpath
        assert(m_fastpath == true);
        m_buffer.push_back(input);
        IN oldest = m_buffer.front();
        m_buffer.pop_front();
        m_sma = (m_weight * input) + m_sma - (m_weight * oldest);
        output = m_sma;
        output_is_discardable = false;
    }

private:
    bool m_fastpath;
    size_t m_window_width;
    float m_weight;
    std::deque<IN> m_buffer;
    OUT m_sma;
};

template<typename T>
using tolerance_filter = memoryless_bandpass_filter<T>;

} /* namespace tarp */
