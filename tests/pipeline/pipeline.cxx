#include <initializer_list>
#include <memory>

#include <tarp/cohort.h>
#include <tarp/floats.h>
#include <tarp/log.h>
#include <tarp/pipeline.hxx>

using namespace std;
using namespace tarp;

enum transformType { SMA = 0, WMA };

/*
 * basic tests for for the Cxx Process api.
 *
 * Note the tests here are not comprehensive. They don't test everything
 * and won't catch all possible bugs. Rather the point of them is to have
 * a base to build on and expand if bugs are found and to run the available
 * sanitizers on the test binary.
 */

template<typename IN, typename OUT>
OUT test_ma(enum transformType transform,
            size_t window_width,
            std::initializer_list<float> weights, /* not used for sma */
            std::initializer_list<IN> inputs,
            vector<OUT> &expected_outputs) {
    vector<OUT> actual_outputs;
    std::shared_ptr<tarp::PipelineStage<IN, OUT>> ma;

    switch (transform) {
    case SMA: ma = make_shared<tarp::sma<IN, OUT>>(window_width); break;
    case WMA:
        ma = make_shared<tarp::wma<IN, OUT>>(window_width, weights);
        break;
    };

    ma->output.connect_detached([&](OUT val) {
        actual_outputs.push_back(val);
    });

    for (auto &i : inputs) {
        ma->process(i);
    }

    fprintf(stderr, "EXPECTED outputs: ");
    for (auto &elem : expected_outputs) {
        fprintf(stderr, "%f ", elem);
    }
    fprintf(stderr, "\n");

    fprintf(stderr, "ACTUAL outputs:   ");
    for (auto &elem : actual_outputs) {
        fprintf(stderr, "%f ", elem);
    }
    fprintf(stderr, "\n");

    if (actual_outputs.size() != expected_outputs.size()) {
        error(
          "number of actual outputs (%zu) != number of expected outputs (%zu)",
          actual_outputs.size(),
          expected_outputs.size());
        return TEST_FAIL;
    }

    for (size_t i = 0; i < actual_outputs.size(); ++i) {
        if (dbcmp(actual_outputs[i], expected_outputs[i], 0.01)) {
            error("FAIL: actual(%f) != expected(%f)",
                  actual_outputs[i],
                  expected_outputs[i]);
            return TEST_FAIL;
        }
    }

    return TEST_PASS;
}

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);

    prepare_test_variables();

    /*==========================
     * Test 1:
     * Test that the SMA (to a 0.01 tolerance) class produces the expected
     * outputs for the given inputs, using a window width of 4.
     */
    printf("Validating tarp::sma\n");
    std::initializer_list<float> inputs = {
      1.0, 3.1, 1235.222, 333.2222, 0.0031, 41.4, 47.1, 88.0};
    std::vector<float> outputs = {
      393.136, 392.8867, 402.4617, 105.4312, 44.12577};
    passed = run(test_ma<float, float>,
                 TEST_PASS,
                 SMA,
                 4,
                 std::initializer_list<float> {},
                 inputs,
                 outputs);
    update_test_counter(passed, test_sma);

    /*==========================
     * Test 2:
     * Test that the WMA (to a 0.01 tolerance) class produces the expected
     * outputs for the given inputs, using a window width of 5 with the last
     * 3 weights filled in implicitly.
     */
    printf("Validating tarp::wma (1)\n");
    outputs = {45.0028, 16.9278, 28.922, 19.6734};
    std::initializer_list<float> weights {0.3, 0.4};
    passed = run(test_ma<float, float>,
                 TEST_PASS,
                 WMA,
                 5,
                 weights,
                 initializer_list<float> {
                   123.f, 11.8, 33.02, 0.008, 0.8, 0.99, 188.34, 4.18},
                 outputs);
    update_test_counter(passed, test_wma);

    /*==========================
     * Test 3:
     * Same as test 2, but explicitly specify all the weights;
     */
    printf("Validating tarp::wma (2)\n");
    outputs = {45.0028, 16.9278, 28.922, 19.6734};
    passed = run(test_ma<float, float>,
                 TEST_PASS,
                 WMA,
                 5,
                 initializer_list<float> {0.3, 0.4, 0.1, 0.1, 0.1},
                 initializer_list<float> {
                   123.f, 11.8, 33.02, 0.008, 0.8, 0.99, 188.34, 4.18},
                 outputs);
    update_test_counter(passed, test_wma);

    report_test_summary();
}
