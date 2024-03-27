#include <tarp/signal.hxx>

using namespace tarp;

std::unique_ptr<signal_connection>
tarp::abstract_signal_interface::make_signal_connection(
  abstract_signal_interface &signal,
  unsigned id) {
    tarp::signal_connection *sc = new signal_connection(signal, id);
    return std::unique_ptr<tarp::signal_connection>(sc);
}
