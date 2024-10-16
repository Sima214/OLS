#pragma once

#include "nlohmann/json.hpp"
#include "FunscriptAction.h"
#include "OFS_Reflection.h"
#include "OFS_Serialization.h"
#include "OFS_BinarySerialization.h"

#include <string>
#include <memory>
#include <chrono>
#include <tuple>

#include "OFS_Util.h"
#include "FunscriptSpline.h"

#include "OFS_Profiling.h"

#include "OFS_Event.h"

class FunscriptUndoSystem;
class Funscript;

class FunscriptActionsChangedEvent: public OFS_Event<FunscriptActionsChangedEvent> {
public:
    // FIXME: get rid of this raw pointer
    const Funscript* Script = nullptr;
    FunscriptActionsChangedEvent(const Funscript* changedScript) noexcept
    : Script(changedScript) {}
};

class FunscriptSelectionChangedEvent: public OFS_Event<FunscriptSelectionChangedEvent> {
public:
    // FIXME: get rid of this raw pointer
    const Funscript* Script = nullptr;
    FunscriptSelectionChangedEvent(const Funscript* changedScript) noexcept
    : Script(changedScript) {}
};

class FunscriptNameChangedEvent: public OFS_Event<FunscriptNameChangedEvent> {
public:
    // FIXME: get rid of this raw pointer
    const Funscript* Script = nullptr;
    std::string oldName;
    FunscriptNameChangedEvent(const Funscript* changedScript, const std::string& oldName) noexcept
    : Script(changedScript), oldName(oldName) {}
};

class FunscriptRemovedEvent: public OFS_Event<FunscriptRemovedEvent> {
public:
    std::string name;
    FunscriptRemovedEvent(const std::string& name) noexcept
    : name(name) {}
};

class Funscript {
public:
    static constexpr auto Extension = ".funscript";

    struct FunscriptData {
        FunscriptArray Actions;
        FunscriptArray Selection;
    };

    struct Metadata {
        std::string type = "basic";
        std::string title;
        std::string creator;
        std::string script_url;
        std::string video_url;
        std::vector<std::string> tags;
        std::vector<std::string> performers;
        std::string description;
        std::string license;
        std::string notes;
        int64_t duration = 0;
    };

    template<typename S>
    void serialize(S& s)
    {
        s.ext(*this, bitsery::ext::Growable{},
            [](S& s, Funscript& o) {
                s.container(o.data.Actions, std::numeric_limits<uint32_t>::max());
                s.text1b(o.currentPathRelative, o.currentPathRelative.max_size());
                s.text1b(o.title, o.title.max_size());
                s.boolValue(o.Enabled);
            });
    }

private:
    // FIXME: OFS should be able to retain metadata injected by other programs without overwriting it
    // nlohmann::json JsonOther;

    std::chrono::system_clock::time_point editTime;
    bool funscriptChanged = false; // used to fire only one event every frame a change occurs
    bool unsavedEdits = false; // used to track if the script has unsaved changes
    bool selectionChanged = false;
    FunscriptData data;

    void checkForInvalidatedActions() noexcept;

    FunscriptAction* getAction(FunscriptAction action) noexcept;

public:
    static FunscriptAction* getActionAtTime(FunscriptArray& actions, float time, float maxErrorTime) noexcept;

private:
    FunscriptAction* getNextActionAhead(float time) noexcept;

    FunscriptAction* getPreviousActionBehind(float time) noexcept;

    void moveAllActionsTime(float timeOffset);
    void moveActionsPosition(std::vector<FunscriptAction*> moving, int32_t posOffset);
    inline void sortSelection() noexcept { sortActions(data.Selection); }
    static void sortActions(FunscriptArray& actions) noexcept;
    inline void addAction(FunscriptArray& actions, FunscriptAction newAction) noexcept
    {
        actions.emplace(newAction);
        notifyActionsChanged(true);
    }
    inline void notifySelectionChanged() noexcept { selectionChanged = true; }

    static void loadMetadata(const nlohmann::json& metadataObj, Funscript::Metadata& outMetadata) noexcept;
    static void saveMetadata(nlohmann::json& outMetadataObj, const Funscript::Metadata& inMetadata) noexcept;

    void notifyActionsChanged(bool isEdit) noexcept;
    std::string currentPathRelative;
    std::string title;

public:
    Funscript() noexcept;
    ~Funscript() noexcept;

    static std::array<const char*, 9> AxisNames;

    bool Enabled = true;
    std::unique_ptr<FunscriptUndoSystem> undoSystem;

    void UpdateRelativePath(const std::string& path) noexcept;
    inline void ClearUnsavedEdits() noexcept { unsavedEdits = false; }
    inline const std::string& RelativePath() const noexcept { return currentPathRelative; }
    inline const std::string& Title() const noexcept { return title; }

    inline void Rollback(FunscriptData&& data) noexcept
    {
        this->data = std::move(data);
        notifyActionsChanged(true);
    }
    inline void Rollback(const FunscriptData& data) noexcept
    {
        this->data = data;
        notifyActionsChanged(true);
    }
    void Update() noexcept;

    bool ParseFromCsv(const std::string& csvText) noexcept;
    bool Deserialize(const nlohmann::json& json, Funscript::Metadata* outMetadata, bool loadChapters) noexcept;
    inline nlohmann::json Serialize(const Funscript::Metadata& metadata, bool includeChapters) const noexcept
    {
        nlohmann::json json;
        Serialize(json, data, metadata, includeChapters);
        return json;
    }
    static void Serialize(nlohmann::json& json, const FunscriptData& funscriptData, const Funscript::Metadata& metadata, bool includeChapters) noexcept;

    inline const FunscriptData& Data() const noexcept { return data; }
    inline const auto& Selection() const noexcept { return data.Selection; }
    inline const auto& Actions() const noexcept { return data.Actions; }

    inline const FunscriptAction* GetAction(FunscriptAction action) noexcept { return getAction(action); }
    inline const FunscriptAction* GetActionAtTime(float time, float errorTime) noexcept { return getActionAtTime(data.Actions, time, errorTime); }
    inline const FunscriptAction* GetNextActionAhead(float time) noexcept { return getNextActionAhead(time); }
    inline const FunscriptAction* GetPreviousActionBehind(float time) noexcept { return getPreviousActionBehind(time); }
    inline const FunscriptAction* GetClosestAction(float time) noexcept { return getActionAtTime(data.Actions, time, std::numeric_limits<float>::max()); }

    float GetPositionAtTime(float time) const noexcept;

    /**
     * Calculate and return the current and target position
     * and the remaining time until the target position is reached.
     * 
     * NOTE: Playback speed compensation is not applied.
     */
    std::tuple<float, float, float> getInterpolatedAction(
        float time) const noexcept;

    inline void AddAction(FunscriptAction newAction) noexcept { addAction(data.Actions, newAction); }
    void AddMultipleActions(const FunscriptArray& actions) noexcept;

    bool EditAction(FunscriptAction oldAction, FunscriptAction newAction) noexcept;
    void AddEditAction(FunscriptAction action, float frameTime) noexcept;
    void RemoveAction(FunscriptAction action, bool checkInvalidSelection = true) noexcept;
    void RemoveActions(const FunscriptArray& actions) noexcept;

    std::vector<FunscriptAction> GetLastStroke(float time) noexcept;

    void SetActions(const FunscriptArray& override_with) noexcept;

    inline bool HasUnsavedEdits() const { return unsavedEdits; }
    inline const std::chrono::system_clock::time_point& EditTime() const { return editTime; }

    void RemoveActionsInInterval(float fromTime, float toTime) noexcept;

    // selection api
    void RangeExtendSelection(int32_t rangeExtend) noexcept;
    bool ToggleSelection(FunscriptAction action) noexcept;

    void SetSelected(FunscriptAction action, bool selected) noexcept;
    void SelectTopActions() noexcept;
    void SelectBottomActions() noexcept;
    void SelectMidActions() noexcept;
    void SelectTime(float fromTime, float toTime, bool clear = true) noexcept;
    FunscriptArray GetSelection(float fromTime, float toTime) noexcept;

    void SelectAction(FunscriptAction select) noexcept;
    void DeselectAction(FunscriptAction deselect) noexcept;
    void SelectAll() noexcept;
    void RemoveSelectedActions() noexcept;
    void MoveSelectionTime(float time_offset, float frameTime) noexcept;
    void MoveSelectionPosition(int32_t pos_offset) noexcept;
    inline bool HasSelection() const noexcept { return !data.Selection.empty(); }
    inline uint32_t SelectionSize() const noexcept { return data.Selection.size(); }
    inline void ClearSelection() noexcept { data.Selection.clear(); }
    inline const FunscriptAction* GetClosestActionSelection(float time) noexcept { return getActionAtTime(data.Selection, time, std::numeric_limits<float>::max()); }

    void SetSelection(const FunscriptArray& actions) noexcept;
    bool IsSelected(FunscriptAction action) noexcept;

    void EqualizeSelection() noexcept;
    void InvertSelection() noexcept;

    FunscriptSpline ScriptSpline;
    inline const float Spline(float time) noexcept
    {
        return ScriptSpline.Sample(data.Actions, time);
    }

    inline const float SplineClamped(float time) noexcept
    {
        return Util::Clamp<float>(Spline(time) * 100.f, 0.f, 100.f);
    }
};

REFL_TYPE(Funscript::Metadata)
REFL_FIELD(type)
REFL_FIELD(title)
REFL_FIELD(creator)
REFL_FIELD(script_url)
REFL_FIELD(video_url)
REFL_FIELD(tags)
REFL_FIELD(performers)
REFL_FIELD(description)
REFL_FIELD(license)
REFL_FIELD(notes)
REFL_FIELD(duration)
REFL_END
