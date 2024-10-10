#include "ParserDispatcher.hpp"

#include <utils/Filenames.hpp>
#include <utils/Trace.hpp>
#include <utils/Z85.hpp>

#include <deque>
#include <ios>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace tcode {

ParserDispatcher::ParserDispatcher() = default;
ParserDispatcher::~ParserDispatcher() {
    if (is_connected() || is_connecting()) [[unlikely]] {
        utils::trace("Destroying an active ParserDispatcher!");
        disconnect();
    }
    if (_conn_thr.get_id() != std::thread::id()) {
        _conn_thr.join();
    }
}

std::pair<std::unique_lock<std::mutex>, Registry&> ParserDispatcher::acquire_registry() {
    return {std::unique_lock(_registry_mutex), _registry};
}

void ParserDispatcher::reset() {
    if (is_connected() || is_connecting()) [[unlikely]] {
        utils::trace("Resetting an active ParserDispatcher!");
        disconnect();
    }

    _conn_ctx.restart();
    _conn_hnd = nullptr;

    if (_conn_thr.get_id() != std::thread::id()) {
        _conn_thr.join();
    }
    _conn_thr = std::thread();

    _building_request = false;
    _pending_response = false;

    _input_buffer.clear();
    /**
     * Regarding _input_adapter:
     * With the Dynamic Buffer V2 interface, the object
     * holds only a reference to the vector and the max size.
     * Because of the above, we do not need to re-construct v2 buffers,
     * unless memory corruption has occured (which should have already lead to abort).
     */

    _request_code_count = 0;
    _request_sequence_length = 0;
    _output_buffer_usage = 0;
    std::memset(_output_buffer, 0, sizeof(_output_buffer));

    _parser_state.reset();

    _registry = Registry();
}

void ParserDispatcher::set_packet_tracing(bool enabled) {
    if (_trace_file.is_open() != enabled) {
        std::lock_guard _g(_callback_mutex);
        if (enabled) {
            std::string fn = "etcode_" + utils::make_formatted_time_for_filename() + ".trace";
            _trace_file.open(fn, std::ios_base::out | std::ios_base::app);
            _trace_file_time_start = std::chrono::steady_clock::now();
        }
        else {
            _trace_file.close();
        }
    }
}

void ParserDispatcher::_send_raw_data(const void* data, size_t n) {
    // Bypass staging buffer on large inputs.
    if (n >= sizeof(_output_buffer)) [[unlikely]] {
        if (_output_buffer_usage != 0) {
            _flush_output();
        }
        _send_data(data, n);
    }
    // Append to buffer until it fills up.
    const size_t output_buffer_space = _get_output_buffer_space();
    const size_t pass_1_n = std::min(output_buffer_space, n);
    std::memcpy(&_output_buffer[_output_buffer_usage], data, pass_1_n);
    size_t pass_2_n = n - pass_1_n;
    _output_buffer_usage += pass_1_n;
    if (_output_buffer_usage >= sizeof(_output_buffer)) {
        _flush_output();
    }
    // Append any remaining data.
    if (pass_2_n > 0) {
        assert(pass_2_n <= _get_output_buffer_space());
        std::memcpy(&_output_buffer[_output_buffer_usage], ((uint8_t*) data) + pass_1_n,
                    pass_2_n);
        _output_buffer_usage += pass_2_n;
        if (_output_buffer_usage >= sizeof(_output_buffer)) {
            _flush_output();
        }
    }
}
void ParserDispatcher::_send_raw_byte(char ch) {
    _output_buffer[_output_buffer_usage++] = ch;
    if (_output_buffer_usage >= sizeof(_output_buffer)) {
        _flush_output();
    }
}
void ParserDispatcher::_send_z85_data(const void* data_raw, size_t n_raw, uint8_t null_symbol) {
    const uint32_t* data = (const uint32_t*) data_raw;
    while (n_raw >= 4) {
        // Ensure there is output buffer enough space.
        size_t output_buffer_space = _get_output_buffer_space();
        if (output_buffer_space < 5) {
            _flush_output();
            output_buffer_space = sizeof(_output_buffer);
        }
        // Encode data.
        size_t batch_tokens = std::min(output_buffer_space / 5, n_raw / 4);
        ::codec::encode((::codec::z85_pack_t*) &_output_buffer[_output_buffer_usage],
                        batch_tokens, data, batch_tokens);
        _output_buffer_usage += batch_tokens * 5;
        n_raw -= batch_tokens * 4;
        data += batch_tokens;
    }
    // Append partial, padded with null symbols.
    if (n_raw > 0) {
        // Ensure there is output buffer enough space.
        size_t output_buffer_space = _get_output_buffer_space();
        if (output_buffer_space < 5) {
            _flush_output();
            output_buffer_space = sizeof(_output_buffer);
        }
        uint32_t end_data;
        std::memset(&end_data, null_symbol, 4);
        std::memcpy(&end_data, data, n_raw);
        ::codec::encode((::codec::z85_pack_t*) &_output_buffer[_output_buffer_usage], 1,
                        &end_data, 1);
        _output_buffer_usage += 5;
    }
    if (_output_buffer_usage >= sizeof(_output_buffer)) {
        _flush_output();
    }
}

void ParserDispatcher::_flush_output() {
    assert(_output_buffer_usage <= sizeof(_output_buffer));

    if (_trace_file.is_open()) [[unlikely]] {
        std::lock_guard _g(_callback_mutex);
        if (_trace_file.is_open()) {
            auto const current_timestamp = std::chrono::steady_clock::now();
            auto const time_offset = current_timestamp - _trace_file_time_start;
            auto const timestamp =
                 std::chrono::duration_cast<std::chrono::microseconds>(time_offset);
            _trace_file << timestamp.count() << ">>>";
            std::string_view data_str(_output_buffer, _output_buffer_usage);
            if (!data_str.empty() && data_str.back() == '\n') {
                data_str = data_str.substr(0, data_str.size() - 1);
            }
            _trace_file << data_str << std::endl;
        }
    }

    _send_data(_output_buffer, _output_buffer_usage);
    _output_buffer_usage = 0;
}

}  // namespace tcode
