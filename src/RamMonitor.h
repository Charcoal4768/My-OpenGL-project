#ifndef RAM_MONITOR_H
#define RAM_MONITOR_H

#include <UI/UIElements.h>

#include <array>
#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>

struct RamSample {
    uint64_t systemTotalBytes = 0;
    uint64_t systemAvailableBytes = 0;
    uint64_t processVirtualBytes = 0;
    uint64_t processResidentBytes = 0;
    uint64_t processSharedBytes = 0;
    uint64_t processDataBytes = 0;
};

struct ProcessInfo {
    pid_t processId = 0;
    std::string name;
};

class ProcMemoryReader {
  public:
    static RamSample ReadProcess(pid_t processId);
    static std::vector<ProcessInfo> ListActiveProcesses();
};

class RamMonitor {
  private:
    enum class ColorRole {
        Background,
        Surface,
        Inset,
        Border,
        PrimaryText,
        SecondaryText,
        MutedText,
        Accent,
        AccentTint,
        Track,
        Grid,
        Live,
    };

    struct ColorBinding {
        int elementId;
        ColorRole role;
        bool border;
    };

    struct TextBinding {
        int elementId;
        ColorRole role;
    };

    struct ProcessRow {
        int backgroundId;
        int textId;
        pid_t processId = 0;
        size_t processIndex = 0;
    };

    UIScene *scene = nullptr;
    bool lightTheme = false;
    pid_t processId = 0;
    double nextSampleTime = 0.0;
    double nextProcessScanTime = 0.0;
    RamSample sample;
    std::string lastReadError;
    std::vector<ProcessInfo> processes;
    size_t processScrollOffset = 0;
    bool processMenuOpen = false;
    std::vector<uint64_t> rssHistory;
    std::vector<int> historyBars;
    std::vector<ColorBinding> colorBindings;
    std::vector<TextBinding> textBindings;

    int statusDotId = I_UNSET;
    int processPickerId = I_UNSET;
    int processPickerTextId = I_UNSET;
    int processMenuId = I_UNSET;
    int processMenuHeaderId = I_UNSET;
    int processMenuCountTextId = I_UNSET;
    std::vector<ProcessRow> processRows;
    float processPickerX = 0.0f;
    float processPickerY = 0.0f;
    float processPickerWidth = 0.0f;
    float processPickerHeight = 0.0f;
    float processRowHeight = 0.0f;
    int systemUsedTextId = I_UNSET;
    int systemTotalTextId = I_UNSET;
    int systemAvailableTextId = I_UNSET;
    int systemPercentTextId = I_UNSET;
    int systemProgressId = I_UNSET;
    float systemProgressWidth = 0.0f;
    int processResidentTextId = I_UNSET;
    int processIdTextId = I_UNSET;
    int processVirtualTextId = I_UNSET;
    int processSharedTextId = I_UNSET;
    int processDataTextId = I_UNSET;
    int statusTextId = I_UNSET;
    float chartX = 0.0f;
    float chartBaseline = 0.0f;
    float chartHeight = 0.0f;
    float chartBarWidth = 0.0f;
    float chartBarGap = 0.0f;

    static constexpr size_t HISTORY_LENGTH = 60;
    static constexpr size_t PROCESS_MENU_ROW_COUNT = 6;
    static constexpr double PROCESS_SCAN_INTERVAL = 2.0;

    Color GetColor(ColorRole role) const;
    int AddShape(int parentId, float x, float y, float width, float height,
                 ColorRole role, float radius = 8.0f, bool outlined = false);
    int AddText(int parentId, float x, float y, float fontSize,
                const std::string &text, ColorRole role);
    void SetText(int elementId, const std::string &text, float fontSize);
    void ApplyTheme();
    void ApplySample(const RamSample &newSample);
    void UpdateHistoryChart();
    void SetReadError(const std::string &message);
    void RefreshProcessList();
    void UpdateProcessMenuRows();
    void SetProcessMenuOpen(bool open);
    std::string FormatProcessLabel(const ProcessInfo &process) const;
    bool Contains(int elementId, double x, double y) const;
    void SelectProcess(pid_t selectedProcessId);

  public:
    void Build(UIScene &uiScene, const std::array<float, 2> &resolution,
               pid_t monitoredProcessId);
    void Update(double currentTime);
    void ToggleTheme();
    void HandleMouseClick(double x, double y);
    void HandleScroll(double offset);
};

#endif
