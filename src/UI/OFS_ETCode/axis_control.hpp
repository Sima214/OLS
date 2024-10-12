#ifndef SATR_VK_SEVFATE_AXIS_CONTROL_HPP
#define SATR_VK_SEVFATE_AXIS_CONTROL_HPP
/**
 * @file
 * @brief
 */

#include "tcode/Messages.hpp"
#include <Funscript/Funscript.h>
#include <tcode/ParserDispatcherRegistry.hpp>
#include <tcode/Utils.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>

namespace sevfate {

    using axis_manual_ctlst_t = uint32_t;

    class AxisPatternElement {
    public:
        enum class Type : uint8_t { NoAction = 0,
            Normal = 1,
            Interval = 2,
            Speed = 3 };
        static constexpr std::array TYPE_METADATA_TABLE = { "Nop", "Normal", "Interval", "Speed" };

        static constexpr uint16_t TIME_STEP = 1;
        static constexpr uint16_t TIME_STEP_FAST = 100;

        static constexpr uint32_t TARGET_DIGIT_COUNT = 3;
        static constexpr uint32_t TARGET_MIN = 0;
        static constexpr uint32_t TARGET_MAX = tcode::make_nines<uint32_t, TARGET_DIGIT_COUNT>();
        static constexpr uint32_t TARGET_DEFAULT = (TARGET_MAX + 1) / 2;

        struct build_ui_state_t {
            const uint16_t current_time;
            const bool first;
            const bool last;

            bool remove = false;
            bool append = false;
            bool move_up = false;
            bool move_down = false;

            bool update_type = false;
            bool update_duration = false;
            bool update_target = false;

            Type updated_type = Type::NoAction;
            uint16_t updated_duration = UINT16_MAX;
            uint16_t updated_target = UINT16_MAX;
        };

    protected:
        uint16_t _start_time = 0;
        uint16_t _duration = 1;
        Type _type = Type::NoAction;
        uint16_t _target = TARGET_DEFAULT;

    public:
        constexpr AxisPatternElement() noexcept = default;
        constexpr AxisPatternElement(uint16_t start_time) noexcept : _start_time(start_time) {}
        constexpr AxisPatternElement(const AxisPatternElement&) noexcept = default;
        constexpr AxisPatternElement& operator=(const AxisPatternElement&) noexcept = default;

        constexpr bool operator<(const AxisPatternElement& rhs) const noexcept
        {
            return this->_start_time < rhs._start_time;
        }
        constexpr bool operator==(const AxisPatternElement& rhs) const noexcept
        {
            return this->_start_time == rhs._start_time;
        }

        constexpr bool operator<(const uint16_t& rhs) const noexcept
        {
            return this->_start_time < rhs;
        }
        constexpr bool operator==(const uint16_t& rhs) const noexcept
        {
            return this->_start_time == rhs;
        }

        constexpr auto get_start_time() const
        {
            return _start_time;
        }
        constexpr auto get_duration() const
        {
            return _duration;
        }
        /**
         * End time is the tick time at which the change to the next pattern occurs.
         */
        constexpr auto get_end_time() const
        {
            return _start_time + _duration;
        }
        constexpr auto get_type() const
        {
            return _type;
        }
        constexpr auto get_target() const
        {
            return _target;
        }

        void apply(tcode::CommandEndpoint& ep, uint16_t previous_target) const;
        void build_ui(size_t i, tcode::CommandEndpoint& ep, build_ui_state_t& state, uint32_t duration_allowance) const;

        friend class AxisPatternList;
    };

    class AxisPatternList {
    public:
        static constexpr uint32_t TIME_LIMIT = 60 * 1000;

    protected:
        /* Time fields are in milliseconds (max time: 1 minute). */
        uint16_t _current_time = 0;
        uint16_t _current_pattern_idx = 0;
        bool _active = false;
        std::vector<AxisPatternElement> _patterns = std::vector<AxisPatternElement>(1);

    public:
        constexpr AxisPatternList() noexcept = default;
        AxisPatternList(const AxisPatternList&) = delete;
        AxisPatternList operator=(const AxisPatternList&) = delete;
        AxisPatternList(AxisPatternList&& o) noexcept = default;
        AxisPatternList& operator=(AxisPatternList&& o) noexcept = default;

        /** Getters */
        bool is_active() const
        {
            return _active;
        }

        auto get_current_time() const
        {
            return _current_time;
        }
        auto get_total_time() const
        {
            return _patterns.back().get_end_time();
        }
        size_t get_pattern_count() const
        {
            return _patterns.size();
        }
        const AxisPatternElement& get_pattern(size_t i) const
        {
            return _patterns.at(i);
        }
        const AxisPatternElement& operator[](size_t i) const
        {
            return _patterns.at(i);
        }

        size_t find_pattern_index(uint16_t time) const;
        const AxisPatternElement* find_pattern(uint16_t time) const;

        /** Pattern Modifications - @return pointer to the object that was directly modified. */
        const AxisPatternElement* set_pattern_duration(size_t i, uint16_t new_duration);
        bool set_pattern_start_time(size_t i, uint16_t new_start_time);
        const AxisPatternElement* set_pattern_type(size_t i, AxisPatternElement::Type new_type);
        const AxisPatternElement* set_pattern_target(size_t i, uint16_t new_target);

        /** List Modify */
        const AxisPatternElement* new_pattern(size_t i = -1ULL);
        bool swap_patterns(size_t a, size_t b);
        bool del_pattern(size_t i);

        /** Tick/State update */
        void apply(tcode::CommandEndpoint& ep, int tick_delta);
        void build_ui(tcode::CommandEndpoint& ep);

    protected:
        /** Performs insertion sort on patterns array while also fixing duration fields. */
        void _sort();
    };

    class AxisScriptLink {
    public:
        typedef tcode::fractional<uint32_t> normal_cmd_t;
        typedef std::pair<tcode::fractional<uint32_t>, tcode::request::IntervalData> interval_cmd_t;
        typedef std::pair<tcode::fractional<uint32_t>, tcode::request::SpeedData> speed_cmd_t;

    protected:
        static constexpr uint32_t TARGET_DIGIT_COUNT = 3;
        static constexpr uint32_t TARGET_MIN = 0;
        static constexpr uint32_t TARGET_MAX = tcode::make_nines<uint32_t, TARGET_DIGIT_COUNT>();
        static constexpr uint32_t TARGET_DEFAULT = (TARGET_MAX + 1) / 2;
        static constexpr int32_t MAX_UPDATE_PERIOD_MS = 333;

        std::weak_ptr<Funscript> _script = {};
        bool _invert = false;
        bool _paused_update_state = false;
        int32_t _ms_until_next_update = 0;
        std::variant<std::monostate, normal_cmd_t, interval_cmd_t, speed_cmd_t> _last_command;

    public:
        constexpr AxisScriptLink() noexcept = default;
        AxisScriptLink(const AxisScriptLink&) noexcept = default;
        AxisScriptLink& operator=(const AxisScriptLink&) noexcept = default;

        void apply(tcode::CommandEndpoint& ep, size_t delta_ms);
        void build_ui(tcode::CommandEndpoint& ep);

        const auto& get_last_command() const
        {
            return _last_command;
        }

    private:
        bool _send_normal_cmd(tcode::CommandEndpoint& ep, tcode::fractional<uint32_t> v);
        bool _send_interval_cmd(tcode::CommandEndpoint& ep, tcode::fractional<uint32_t> v, uint32_t interval);
        bool _send_speed_cmd(tcode::CommandEndpoint& ep, tcode::fractional<uint32_t> v, uint32_t speed);
        bool _send_stop_cmd(tcode::CommandEndpoint& ep);
    };

    enum class AxisControlState : uint8_t { Unknown = 0,
        Manual = 1,
        Pattern = 2,
        Script = 3 };

    class AxisControlElement {
    protected:
        tcode::common::CommandIndex _axis_idx{};
        /** For use when integrating with interactive applications. */
        bool _stop_on_pause = false;
        AxisControlState _ctl_state = AxisControlState::Unknown;
        axis_manual_ctlst_t _ctl_manual{};
        AxisPatternList _ctl_pattern{};
        AxisScriptLink _ctl_script{};

    public:
        constexpr AxisControlElement() noexcept = default;
        constexpr AxisControlElement(tcode::common::CommandIndex axis_idx) noexcept : AxisControlElement()
        {
            _axis_idx = axis_idx;
        };
        AxisControlElement(const AxisControlElement&) = delete;
        AxisControlElement& operator=(const AxisControlElement&) = delete;
        AxisControlElement(AxisControlElement&& o) noexcept : _axis_idx(o._axis_idx), _stop_on_pause(o._stop_on_pause),
                                                              _ctl_state(std::exchange(o._ctl_state, AxisControlState::Unknown)),
                                                              _ctl_manual(std::move(o._ctl_manual)), _ctl_pattern(std::move(o._ctl_pattern)) {}
        AxisControlElement& operator=(AxisControlElement&& o) noexcept
        {
            _axis_idx = o._axis_idx;
            _stop_on_pause = o._stop_on_pause;
            _ctl_state = std::exchange(o._ctl_state, AxisControlState::Unknown);
            _ctl_manual = std::move(o._ctl_manual);
            _ctl_pattern = std::move(o._ctl_pattern);
            return *this;
        }
        ~AxisControlElement()
        {
            // Invalidate
            _ctl_state = AxisControlState::Unknown;
        }

        /* Getters/Setters */
        tcode::common::CommandIndex get_axis_idx() const
        {
            return _axis_idx;
        }
        bool get_stop_on_pause() const
        {
            return _stop_on_pause;
        }
        bool& get_stop_on_pause_mut()
        {
            return _stop_on_pause;
        }
        void set_stop_on_pause(bool v)
        {
            _stop_on_pause = v;
        }
        AxisControlState get_ctl_state() const
        {
            return _ctl_state;
        }
        axis_manual_ctlst_t get_ctl_manual() const
        {
            return _ctl_manual;
        }
        AxisPatternList& get_ctl_pattern()
        {
            return _ctl_pattern;
        }
        AxisScriptLink& get_ctl_script()
        {
            return _ctl_script;
        }

        /* Operators for use by ordered containers */
        constexpr bool operator<(const AxisControlElement& rhs) const
        {
            return this->operator<(rhs._axis_idx);
        }
        constexpr bool operator<(const tcode::common::CommandIndex& rhs_axis_idx) const
        {
            return this->_axis_idx < rhs_axis_idx;
        }
        constexpr bool operator==(const AxisControlElement& rhs) const
        {
            return this->operator==(rhs._axis_idx);
        }
        constexpr bool operator==(const tcode::common::CommandIndex& rhs_axis_idx) const
        {
            return this->_axis_idx == rhs_axis_idx;
        }

        /** @name Selectors
         * @brief sets the appropriate _ctl_state enum and returns the corresponding ctl object.
         */
        ///@{
        axis_manual_ctlst_t& select_ctl_manual(axis_manual_ctlst_t def)
        {
            bool changed = _change_ctl_state(AxisControlState::Manual);
            if (changed) {
                _ctl_manual = def;
            }
            return _ctl_manual;
        }
        AxisPatternList& select_ctl_pattern()
        {
            _change_ctl_state(AxisControlState::Pattern);
            return _ctl_pattern;
        }
        AxisScriptLink& select_ctl_script()
        {
            _change_ctl_state(AxisControlState::Script);
            return _ctl_script;
        }
        ///@}

    protected:
        /**
         * @returns true if ctl state has changed.
         */
        bool _change_ctl_state(AxisControlState new_ctl_state);
    };

} // namespace sevfate

#endif /*SATR_VK_SEVFATE_AXIS_CONTROL_HPP*/