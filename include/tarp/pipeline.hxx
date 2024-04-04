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
// There can be parallel stages such that the pipeline can split up after
// a certain stage into many parallel paths that then follow completely
// independent courses: the paths can terminate at different points, split up
// into multiple paths as well, etc.
//
// Intermediate outputs (INTM) may also be read after any (non-terminal i.e.
// intermediate) stage. This conceptually resembles a multi-stage electronic
// circuit (e.g. flip-flop shift register) with a test point at the output
// of each stage.
//
// clang-format off
//
// The following diagram illustrates the concept.
//
// .------------------------------------------------------------------------------------.
// |         ___________       _____________                __________                  |
// |        |           |     |             |              |          |                 |
// |  .-->  | STAGE 1   | ==> | STAGE 2 (A) | ...  ==> ... | STAGE N  |  <---------.    |
// |  |     *-----------*  |  *-------------*       |      *----------*            |    |
// |  |                    |   _____________        |                              |    |
// |  |                   ==> |             |       |                              |    |
// |  |                    |  | STAGE 2 (B) | <--.  |                              |    |
// |  |                    |  *-------------*    |  |                              |    |
// |  |                    |   _____________     |  |       __________             |    |
// |  |                   ==> |             |    |  |      |          |            |    |
// |  |                    |  | STAGE 2 (C) | ...| ==> ... | STAGE N  | <--.       |    |
// |  |                    |  *-------------*    |  |      *----------*    |       |    |
// |  |                    |                     |  |                      |       |    |
// |  |                 INTM1                    | INTM2                   |       |    |
// |  |                                          |                         |       |    |
// | INPUT                                    OUTPUT                    OUTPUT   OUTPUT |
// |____________________________________________________________________________________|
//
// clang-format on
//
// NOTE: not every input corresponds to an output. Transformations by any
// given stage are largely arbitrary; an input can correspond to multiple
// outputs and vice-versa. An extreme example may be a black-hole stage that
// takes any number of inputs and never outputs anything.
//
// NOTE: stages must be consistent in terms of their inputs and outputs. That is
// to say, stage 2 must take as input the output of stage 1. So arbitrary stages
// cannot be linked at random.
//

template<typename output_t>
class PipelineStageOutputInterface {
public:
    // hook for subsequent stages (or any arbitrary sink) to connect to
    tarp::signal<void(output_t)> output;
};

template<typename input_t, typename output_t = input_t>
class PipelineStage : public PipelineStageOutputInterface<output_t> {
public:
    PipelineStage() : m_is_terminal(false) {}

    // virtual ~PipelineStage() { m_prev_stage->disconnect(); }

    /* connect current stage to the output of a previous one */
    void join(PipelineStageOutputInterface<input_t> &prev) {
        m_prev_stage = prev.output.connect([this](input_t value) {
            this->process(value);
        });
    }

    /* Stop emitting signals for new outputs; terminate the pipeline
     * at the current stage. */
    void make_terminal(void) { m_is_terminal = true; }

    void process(input_t value) {
        std::optional<output_t> result;
        process_input(value, result);

        /* no output this time */
        if (!result.has_value()) return;

        if (m_is_terminal) return;

        this->output.emit(result.value());
    }

    void operator()(input_t value) { process(value); };

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
    virtual void process_input(input_t input,
                               std::optional<output_t> &output) = 0;

private:
    std::unique_ptr<tarp::signal_connection> m_prev_stage;
    bool m_is_terminal;
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
    // The parameters can be specified in 2 ways:
    // a=low_cutoff, b=high_cutoff, is_tolerance=false
    // OR
    // a=reference_value, b_tolerance, is_tolerance=true
    //
    // The latter case is converted to the former like this:
    //   low_cutoff  = reference_value - tolerance
    //   high_cutoff = reference_value + tolerance
    memoryless_bandpass_filter(T a, T b, bool is_tolerance)
        : PipelineStage<T, T>() {
        m_low_cutoff = a;
        m_high_cutoff = b;

        if (is_tolerance) {
            m_low_cutoff = a - b;
            m_high_cutoff = a + b;

            if (std::is_unsigned<T>()) {
                // a configuration with tolerance > reference_value would be
                // nonsensical; consider reference_value=2, with tolerance=3.
                // The low cutoff would underflow and wrap around for unsigned
                // integers.
                if (b < a)
                    std::logic_error(
                      "Invalid memoryless_bandpass_filter configuration "
                      "tolerance > reference_value");
            }
        }

        if (m_low_cutoff > m_high_cutoff)
            throw std::logic_error(
              "Invalid memoryless_bandpass_filter configuration"
              " (low_cutoff > high_cutoff)");
    }

private:
    virtual void process_input(T input, std::optional<T> &output) {
        if (input >= m_low_cutoff and input <= m_high_cutoff) {
            output = input;
            return;
        }
    }

    T m_low_cutoff;
    T m_high_cutoff;
};

/*
 * weighted moving average */
template<typename input_t, typename output_t>
class wma : public PipelineStage<input_t, output_t> {
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

    virtual void process_input(input_t input,
                               std::optional<output_t> &result) override {
        size_t window_width = m_weights.size();
        m_buffer.push_back(input);

        if (m_buffer.size() > window_width) {
            m_buffer.pop_front();
        }

        if (m_buffer.size() == window_width) {
            output_t avg = 0;
            for (size_t i = 0; i < m_buffer.size(); ++i) {
                avg += m_buffer[i] * m_weights[i];
            }
            result = avg;
        }
    }

private:
    std::deque<input_t> m_buffer;
    std::vector<float> m_weights;
};

/*
 * Simple moving average */
template<typename input_t, typename output_t>
class sma : public PipelineStage<input_t, output_t> {
public:
    sma(size_t window_width)
        : PipelineStage<input_t, output_t>()
        , m_fastpath(false)
        , m_window_width(window_width)
        , m_weight(1.0 / window_width)
        , m_sma(0.f) {
        if (window_width == 0) {
            throw std::logic_error("Invalid window of 0 width");
        }
    }

    virtual void process_input(input_t input,
                               std::optional<output_t> &result) override {
        if (!m_fastpath) {
            if (m_buffer.size() < m_window_width) {
                m_buffer.push_back(input);
            }

            /* once we have filled the buffer and calculated the first
             * SMA, then we only need 3 pieces of information to keep the
             * SMA running: the sma and the latest and newest elements. */
            if (m_buffer.size() == m_window_width) {
                std::for_each(
                  m_buffer.begin(), m_buffer.end(), [this](input_t elem) {
                      m_sma += m_weight * elem;
                  });
                result = m_sma;
                m_fastpath = true;
            }
            return;
        }

        // else fastpath
        assert(m_fastpath == true);
        m_buffer.push_back(input);
        input_t oldest = m_buffer.front();
        m_buffer.pop_front();
        m_sma = (m_weight * input) + m_sma - (m_weight * oldest);
        result = m_sma;
    }

private:
    bool m_fastpath;
    size_t m_window_width;
    float m_weight;
    std::deque<input_t> m_buffer;
    output_t m_sma;
};

template<typename T>
using tolerance_filter = memoryless_bandpass_filter<T>;

/*
 * Counter; this is based on the idea of an electronic flip-flop counter
 * circuit, often used to divide an input clock to a lower frequency
 *(frequency division).
 * For example a frequency 1/4 of the input clock frequency can be obtained
 * by using a counter with a period of 4. That is, for every 4 input
 * pulses the counter outputs 1 pulse. The number of distinct states in
 * the counter is termed the 'modulus'. After MODULUS states, the counter
 * recycles back (i.e. wraps around) to its initial state.
 *
 * NOTE: in the context of this class:
 * + the process_input function of the counter simply advances the counter
 * to its next state. The value passed in is ignored and the function only
 * takes a value in order to respect the pipeline stage signature.
 * The output value is a constant 1, emitted on every counter wraparound.
 *
 * The output is not configurable -- it is always size_t, meaning it does not
 * work with just any pipeline stages. The input is configurable since the user
 * may want to count a certain number of events. So any input goes. For example,
 * to implement logic of the following sort: do something for every x events
 * seen; e.g. for doing rate limiting, logging a warning on noticing a mib
 * counter increasing too much etc.
 */
template<typename input_t>
class counter : public PipelineStage<std::size_t, std::size_t> {
public:
    counter(size_t modulus)
        : PipelineStage<size_t, size_t>()
        , m_modulus(modulus)
        , m_current_value(0)
        , m_monotonic_output(1) {
        if (m_modulus <= 0) {
            throw std::logic_error("Invalid counter modulus (0)");
        }
    }

    virtual void process_input(std::size_t,
                               std::optional<std::size_t> &result) override {
        m_current_value = (m_current_value + 1 % m_modulus);
        if (m_current_value != 0) return;
        result = m_monotonic_output;
    }

private:
    size_t m_monotonic_output;
    std::size_t m_modulus;
    std::size_t m_current_value;
};

} /* namespace tarp */
