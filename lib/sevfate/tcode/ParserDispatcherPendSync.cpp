#include "ParserDispatcher.hpp"

namespace tcode {

void ParserDispatcher::wait_pending_response() {
    while (_pending_response) {
        _pending_response.wait(true);
    }
}

void ParserDispatcher::_notify_pending_response() {
    _pending_response.notify_all();
}

}  // namespace tcode
