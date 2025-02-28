#include <tarp/cancellation_token.hxx>

namespace tarp {

cancellation_token::cancellation_token(const cancellation_token &rhs) {
    *this = rhs;
}

cancellation_token::cancellation_token(cancellation_token &&rhs) {
    *this = std::move(rhs);
}

cancellation_token &cancellation_token::operator=(cancellation_token &&rhs) {
    m_observer_id = rhs.m_observer_id;
    rhs.m_observer_id.reset();
    m_token = std::move(rhs.m_token);
    return *this;
}

// We only copy the token, but NOT the observer (if any) id associated with it.
// This is since in the dtor we remove the observer. If we were to copy it, then
// if we had multiple copies, any one copy, when it got destructed (went out
// of scope) would remove the observer for all other copies as well!
// Each copy can have its own observer by not copying that and the semantics is
// much clearer.
cancellation_token &
cancellation_token::operator=(const cancellation_token &rhs) {
    m_token = rhs.m_token;
    return *this;
}

}  // namespace tarp
