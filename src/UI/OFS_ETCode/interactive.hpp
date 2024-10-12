#ifndef SATR_VK_SEVFATE_INTERACTIVE_HPP
#define SATR_VK_SEVFATE_INTERACTIVE_HPP
/**
 * @file
 * @brief
 */

#include <OFS_StateHandle.h>
#include <OFS_VideoplayerEvents.h>
#include <OFS_EventSystem.h>
#include <UI/OFS_ETCode/axis_control.hpp>
#include <UI/OFS_ETCode/state.hpp>
#include <tcode/ParserDispatcher.hpp>

#include <deque>
#include <string>
#include <vector>
#include <chrono>
#include <glm/glm.hpp>

class eTCodeInteractive {
public:
    using cmd_idx_prop_name_key_t = std::pair<tcode::common::CommandIndex, std::string>;
    using plot_history_t = std::vector<std::deque<float>>;

    static constexpr size_t PLOT_HISTORY_MAX_BUFFER_SIZE = 3 * 250;

    using text_input_t = std::variant<std::monostate, std::string, uint32_t, int32_t, uint64_t, int64_t, float, double>;

    static constexpr const char* WindowId = "###ETCODE";

protected:
    uint32_t stateHandle = 0xFFFF'FFFF;
    OFS_EventQueue::Handle _play_pause_change_handle;

    std::string _conn_path{};
    tcode::ConnectionConfig _conn_cfg{};
    tcode::ParserDispatcher _state{};
    bool _connection_active = false;
    bool _enable_suggested_property_intervals = true;
    bool _enable_packet_tracing = false;

    std::map<cmd_idx_prop_name_key_t, plot_history_t, std::less<>> _plot_history{};
    std::map<cmd_idx_prop_name_key_t, text_input_t, std::less<>> _text_input_tmp{};

    std::vector<sevfate::AxisControlElement> _axis_control_state;
    std::chrono::steady_clock::time_point _handle_axes_last_time;

    /** Internal configuration */
    static constexpr uint32_t AXIS_DEFAULT_DIGIT_COUNT = 3;
    static constexpr uint32_t AXIS_OUTPUT_MIN = 0;
    static constexpr uint32_t AXIS_OUTPUT_MAX =
        tcode::make_nines<uint32_t, AXIS_DEFAULT_DIGIT_COUNT>();
    static constexpr uint32_t AXIS_OUTPUT_DEFAULT = (AXIS_OUTPUT_MAX + 1) / 2;

    static constexpr uint32_t AXIS_IMPULSE_INTERVAL = 1;
    static constexpr uint32_t AXIS_IMPULSE_SPEED = 1000; // 10 units/sec.

    static constexpr glm::vec4 BG_COLOR = { 0.0, 0.0, 0.0, 1.0 };
    static constexpr double FP64_ZERO = 0.0;

public:
    eTCodeInteractive() noexcept;
    ~eTCodeInteractive();

    void render_ui(bool* open);

private:
    void _build_connection_tab();
    void _build_info_tab(tcode::Registry& reg);
    void _build_endpoint_tab(tcode::Registry& reg, tcode::common::CommandIndex cmd_idx, tcode::CommandEndpoint& ep);

    void _build_axis_control(tcode::common::CommandIndex cmd_idx, tcode::CommandEndpoint& ep);

    void _build_property_textbox(tcode::common::CommandIndex cmd_idx,
        const std::string& prop_name,
        tcode::PropertyMetadata& prop_meta);

    // void _on_plot_property_update(tcode::ParserDispatcher&, tcode::common::CommandIndex cmd_idx, std::string_view prop_name, tcode::PropertyMetadata& prop_meta);
    // void _build_property_plot(tcode::common::CommandIndex cmd_idx, const std::string& prop_name, tcode::PropertyMetadata& prop_meta);

    void _build_property_default(tcode::common::CommandIndex cmd_idx,
        const std::string& prop_name,
        tcode::PropertyMetadata& prop_meta);
    void _build_property(tcode::common::CommandIndex cmd_idx, const std::string& prop_name, tcode::PropertyMetadata& prop_meta);

    void _handle_axes();
    void _handle_axes_on_pause();
    void _handle_axes_on_play();
    size_t _handle_axes_get_time_delta();
    void _handle_io();
    /** Performs a manual state reset */
    void _disconnect();

    // void _register_plot_history_callbacks(tcode::Registry& reg);
    void _connection_setup();

    void _on_video_playpause_change(const PlayPauseChangeEvent* ev) noexcept;
    void _save_state();
};

#endif /*SATR_VK_SEVFATE_INTERACTIVE_HPP*/
