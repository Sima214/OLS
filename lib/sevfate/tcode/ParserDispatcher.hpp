#ifndef SEVFATE_TCODE_PARSERDISPATCHER_HPP
#define SEVFATE_TCODE_PARSERDISPATCHER_HPP

#define ASIO_NO_DYNAMIC_BUFFER_V1

#include <asio/buffer.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/serial_port.hpp>
#include <tcode/Messages.hpp>
#include <tcode/Parser.hpp>
#include <tcode/ParserDispatcherRegistry.hpp>
#include <utils/Class.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <variant>

namespace tcode {

class ConnectionConfig {
   public:
    enum class FlowControl : uint8_t { NotSet, None, Software, Hardware };

    enum class Parity : uint8_t { NotSet, None, Odd, Even };

    enum class StopBits : uint8_t { NotSet, One, OnePointFive, Two };

   private:
    /* Serial port section */
    bool _serial_port_enabled = true;
    FlowControl _serial_port_flow_control = FlowControl::NotSet;
    Parity _serial_port_parity = Parity::NotSet;
    StopBits _serial_port_stop_bits = StopBits::NotSet;
    uint32_t _serial_port_data_size = 0;
    uint32_t _serial_port_baud_rate = 0;

   public:
    constexpr ConnectionConfig() = default;

    DEFINE_AUTO_PROPERTY(serial_port_enabled);
    DEFINE_AUTO_VALIDATED_PROPERTY(serial_port_flow_control, FlowControl::NotSet);
    DEFINE_AUTO_VALIDATED_PROPERTY(serial_port_parity, Parity::NotSet);
    DEFINE_AUTO_VALIDATED_PROPERTY(serial_port_stop_bits, StopBits::NotSet);
    DEFINE_AUTO_VALIDATED_PROPERTY(serial_port_data_size, 0);
    DEFINE_AUTO_VALIDATED_PROPERTY(serial_port_baud_rate, 0);

   private:
    void configure(asio::serial_port&) const;
    friend class ParserDispatcher;
};

class ParserDispatcher : public meta::INonCopyable {
   public:
    using response_received_callback_t = std::function<bool(ParserDispatcher& pd)>;
    using response_end_callback_t = std::function<bool(ParserDispatcher& pd)>;
    using response_error_callback_t = std::function<bool(ParserDispatcher& pd)>;

    using request_success_callback_t = std::function<bool(ParserDispatcher& pd)>;
    using request_error_callback_t =
         std::function<bool(ParserDispatcher& pd, const response::Error& error)>;

   protected:
    std::atomic<bool> _connecting = false;
    std::atomic<bool> _building_request = false;
    std::atomic<bool> _pending_response = false;

    asio::io_context _conn_ctx{1};
    std::variant<void*, asio::serial_port> _conn_hnd{nullptr};
    std::thread _conn_thr;

    /** Do not accept responses larger than 1Mb. */
    static constexpr size_t INPUT_BUFFER_MAX_SIZE = 1 * 1024 * 1024;
    std::vector<char> _input_buffer;
    asio::dynamic_vector_buffer<char, std::vector<char>::allocator_type> _input_adapter{
         _input_buffer, INPUT_BUFFER_MAX_SIZE};

    size_t _request_code_count = 0;
    size_t _request_sequence_length = 0;
    size_t _output_buffer_usage = 0;
    char _output_buffer[5 * 64];

    parser::yyParser _parser_state;

    std::ofstream _trace_file;
    std::chrono::time_point<std::chrono::steady_clock> _trace_file_time_start;

    /** Public interface state. */
    response_received_callback_t _cb_on_response_received;
    response_end_callback_t _cb_on_response_end;
    response_error_callback_t _cb_on_response_error;
    request_success_callback_t _cb_on_request_success;
    request_error_callback_t _cb_on_request_error;
    std::recursive_mutex _callback_mutex;

    Registry _registry;
    std::mutex _registry_mutex;

   public:
    MMCC_TCODE_API_EXPORT ParserDispatcher();
    MMCC_TCODE_API_EXPORT ~ParserDispatcher();

    /* Public interface. */
    MMCC_TCODE_API_EXPORT void reset();
    MMCC_TCODE_API_EXPORT void set_packet_tracing(bool enabled);

    MMCC_TCODE_API_EXPORT std::pair<std::unique_lock<std::mutex>, Registry&> acquire_registry();

    MMCC_TCODE_API_EXPORT response_received_callback_t
    register_on_response_received_callback(response_received_callback_t&& cb);
    MMCC_TCODE_API_EXPORT response_end_callback_t
    register_on_response_end_callback(response_end_callback_t&& cb);
    MMCC_TCODE_API_EXPORT response_error_callback_t
    register_on_response_error_callback(response_error_callback_t&& cb);

    MMCC_TCODE_API_EXPORT request_success_callback_t
    register_on_request_success_callback(request_success_callback_t&& cb);
    MMCC_TCODE_API_EXPORT request_error_callback_t
    register_on_request_error_callback(request_error_callback_t&& cb);

    /* Connection handling. */
    MMCC_TCODE_API_EXPORT bool is_connecting() const {
        return _connecting;
    }
    MMCC_TCODE_API_EXPORT bool is_connected() const;

    MMCC_TCODE_API_EXPORT void connect(const std::string& path,
                                       const ConnectionConfig& cfg = {});
    MMCC_TCODE_API_EXPORT void disconnect();

    MMCC_TCODE_API_EXPORT void start_event_loop();
    MMCC_TCODE_API_EXPORT void start_detached_event_loop(
         std::function<void(ParserDispatcher&)> prestart_callback = {},
         std::function<void(ParserDispatcher&)> stopped_callback = {});
    MMCC_TCODE_API_EXPORT bool poll_events();

    /* Host to device factory-style interface. */
    MMCC_TCODE_API_EXPORT bool is_response_pending() {
        return _pending_response;
    }

    MMCC_TCODE_API_EXPORT void begin_request();
    MMCC_TCODE_API_EXPORT void end_request();
    MMCC_TCODE_API_EXPORT void wait_pending_response();
    MMCC_TCODE_API_EXPORT void send_request(request::AxisUpdateData cmd_idx);
    MMCC_TCODE_API_EXPORT void send_request(request::AxisUpdateData cmd_idx,
                                            request::IntervalData interval);
    MMCC_TCODE_API_EXPORT void send_request(request::AxisUpdateData cmd_idx,
                                            request::SpeedData speed);
    MMCC_TCODE_API_EXPORT void send_request(request::CommandIndex cmd_idx);
    MMCC_TCODE_API_EXPORT void send_request(request::CommandIndex cmd_idx,
                                            request::PropertyData prop);
    MMCC_TCODE_API_EXPORT void send_request(request::CommandIndex cmd_idx,
                                            request::PropertyData prop,
                                            request::IntervalData interval);
    MMCC_TCODE_API_EXPORT void send_request(request::CommandIndex cmd_idx,
                                            request::PropertyData prop, request::Z85Data data,
                                            uint8_t null_symbol = '\0');
    MMCC_TCODE_API_EXPORT void send_stop_request(request::CommandIndex axis);
    MMCC_TCODE_API_EXPORT void send_stop_request();

    /**
     * The following are utility function with the ability
     * to send multiple requests/codes.
     * No current response must be pending.
     * Calls to begin_request are not necessary.
     * If the return value is true, then the user must call
     * end_request to finalize the request sequence.
     */
    MMCC_TCODE_API_EXPORT bool send_registry_pending_requests();

   private:
    void _on_new_request();
    size_t _get_output_buffer_space() const {
        return sizeof(_output_buffer) - _output_buffer_usage;
    }

    void _send_raw_data(const void* data, size_t n);
    void _send_raw_byte(char ch);
    /** Encode data in z85 and send. */
    void _send_z85_data(const void* data, size_t n, uint8_t null_symbol = '\0');
    void _flush_output();
    void _send_data(const void* data, size_t n);

    /** I/O internal callback handlers. */
    void _conn_thr_main(std::function<void(ParserDispatcher&)> prestart_callback,
                        std::function<void(ParserDispatcher&)> stopped_callback);
    void _schedule_response();
    void _handle_response(asio::error_code ec, size_t bytes_read);

    void _notify_pending_response();

    /* Tokenizer/Parser interface. */
    bool _tokenize(const char* const data, const size_t len);

   public:
    bool _on_response(response::CommandIndex, response::Z85Data);
    bool _on_response(response::CommandIndex, response::PropertyData, response::Z85Data);
    bool _on_response(response::ErrorCode);
    bool _on_response(response::ErrorCode, response::Z85Data);

    /* Called from tokenizer/parser. */
    void _on_response_tokenizer_error(size_t stream_idx);
    void _on_response_syntax_error(size_t stream_idx, parser::yyParser::State err_typ);
    bool _on_response_end();
};

}  // namespace tcode

#endif /*SEVFATE_TCODE_PARSERDISPATCHER_HPP*/
