#include "RamMonitor.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

uint64_t PagesToBytes(uint64_t pages, uint64_t pageSize) {
    if (pages > std::numeric_limits<uint64_t>::max() / pageSize)
        throw std::runtime_error("Process memory value exceeds the supported range.");
    return pages * pageSize;
}

uint64_t KilobytesToBytes(uint64_t kilobytes) {
    constexpr uint64_t BYTES_PER_KILOBYTE = 1024;
    if (kilobytes >
        std::numeric_limits<uint64_t>::max() / BYTES_PER_KILOBYTE) {
        throw std::runtime_error("System memory value exceeds the supported range.");
    }
    return kilobytes * BYTES_PER_KILOBYTE;
}

std::string FormatBytes(uint64_t bytes) {
    static constexpr const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0;
        ++unit;
    }

    std::ostringstream result;
    result << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value
           << ' ' << units[unit];
    return result.str();
}

std::string FormatPercent(uint64_t used, uint64_t total) {
    if (total == 0)
        return "0.0%";
    const double percent =
        std::clamp(static_cast<double>(used) * 100.0 / total, 0.0, 100.0);
    std::ostringstream result;
    result << std::fixed << std::setprecision(1) << percent << '%';
    return result.str();
}

} // namespace

RamSample ProcMemoryReader::ReadProcess(pid_t processId) {
    if (processId <= 0)
        throw std::invalid_argument("Process ID must be positive.");

    std::ifstream memInfo("/proc/meminfo");
    if (!memInfo)
        throw std::runtime_error("Unable to read /proc/meminfo.");

    uint64_t totalKilobytes = 0;
    uint64_t availableKilobytes = 0;
    bool hasTotal = false;
    bool hasAvailable = false;
    std::string line;
    while (std::getline(memInfo, line)) {
        std::istringstream fields(line);
        std::string key;
        uint64_t value = 0;
        std::string unit;
        if (!(fields >> key >> value))
            continue;

        if (key == "MemTotal:" || key == "MemAvailable:") {
            if (!(fields >> unit) || unit != "kB")
                throw std::runtime_error("Unexpected units in /proc/meminfo.");
            if (key == "MemTotal:") {
                totalKilobytes = value;
                hasTotal = true;
            } else {
                availableKilobytes = value;
                hasAvailable = true;
            }
        }
    }
    if (!hasTotal || !hasAvailable || totalKilobytes == 0 ||
        availableKilobytes > totalKilobytes)
        throw std::runtime_error(
            "/proc/meminfo has invalid MemTotal or MemAvailable values.");

    const std::string statmPath =
        "/proc/" + std::to_string(processId) + "/statm";
    std::ifstream statm(statmPath);
    if (!statm)
        throw std::runtime_error("Unable to read " + statmPath + ".");

    uint64_t sizePages = 0;
    uint64_t residentPages = 0;
    uint64_t sharedPages = 0;
    uint64_t textPages = 0;
    uint64_t libraryPages = 0;
    uint64_t dataPages = 0;
    uint64_t dirtyPages = 0;
    if (!(statm >> sizePages >> residentPages >> sharedPages >> textPages >>
          libraryPages >> dataPages >> dirtyPages)) {
        throw std::runtime_error("Unable to parse " + statmPath + ".");
    }

    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0)
        throw std::runtime_error("Unable to determine the system page size.");

    RamSample sample;
    sample.systemTotalBytes = KilobytesToBytes(totalKilobytes);
    sample.systemAvailableBytes = KilobytesToBytes(availableKilobytes);
    sample.processVirtualBytes =
        PagesToBytes(sizePages, static_cast<uint64_t>(pageSize));
    sample.processResidentBytes =
        PagesToBytes(residentPages, static_cast<uint64_t>(pageSize));
    sample.processSharedBytes =
        PagesToBytes(sharedPages, static_cast<uint64_t>(pageSize));
    sample.processDataBytes =
        PagesToBytes(dataPages, static_cast<uint64_t>(pageSize));
    return sample;
}

std::vector<ProcessInfo> ProcMemoryReader::ListActiveProcesses() {
    std::error_code error;
    std::filesystem::directory_iterator current(
        "/proc", std::filesystem::directory_options::skip_permission_denied,
        error);
    if (error)
        throw std::runtime_error("Unable to enumerate /proc: " + error.message());

    std::vector<ProcessInfo> result;
    const std::filesystem::directory_iterator end;
    while (current != end) {
        const std::string entryName = current->path().filename().string();
        current.increment(error);
        if (error)
            throw std::runtime_error("Error while enumerating /proc: " +
                                     error.message());

        pid_t candidateId = 0;
        const auto parsed =
            std::from_chars(entryName.data(), entryName.data() + entryName.size(),
                            candidateId);
        if (parsed.ec != std::errc{} ||
            parsed.ptr != entryName.data() + entryName.size() ||
            candidateId <= 0) {
            continue;
        }

        const std::string basePath = "/proc/" + entryName;
        std::ifstream statFile(basePath + "/stat");
        std::string statLine;
        if (!statFile || !std::getline(statFile, statLine))
            continue;
        const size_t commEnd = statLine.rfind(')');
        if (commEnd == std::string::npos || commEnd + 2 >= statLine.size())
            continue;
        std::istringstream statFields(statLine.substr(commEnd + 2));
        char state = '\0';
        if (!(statFields >> state) || state == 'Z' || state == 'X')
            continue;

        std::ifstream statmFile(basePath + "/statm");
        uint64_t virtualPages = 0;
        uint64_t residentPages = 0;
        if (!statmFile || !(statmFile >> virtualPages >> residentPages))
            continue;

        std::ifstream commFile(basePath + "/comm");
        std::string name;
        if (!commFile || !std::getline(commFile, name) || name.empty())
            name = "unknown";
        result.push_back({candidateId, std::move(name)});
    }

    std::sort(result.begin(), result.end(),
              [](const ProcessInfo &left, const ProcessInfo &right) {
                  std::string leftName = left.name;
                  std::string rightName = right.name;
                  std::transform(leftName.begin(), leftName.end(), leftName.begin(),
                                 [](unsigned char ch) {
                                     return static_cast<char>(std::tolower(ch));
                                 });
                  std::transform(rightName.begin(), rightName.end(),
                                 rightName.begin(), [](unsigned char ch) {
                                     return static_cast<char>(std::tolower(ch));
                                 });
                  if (leftName != rightName)
                      return leftName < rightName;
                  return left.processId < right.processId;
              });
    return result;
}

Color RamMonitor::GetColor(ColorRole role) const {
    if (lightTheme) {
        switch (role) {
        case ColorRole::Background:
            return {0.94f, 0.955f, 0.975f, 1.0f};
        case ColorRole::Surface:
            return {1.0f, 1.0f, 1.0f, 1.0f};
        case ColorRole::Inset:
            return {0.973f, 0.980f, 0.990f, 1.0f};
        case ColorRole::Border:
            return {0.84f, 0.88f, 0.92f, 1.0f};
        case ColorRole::PrimaryText:
            return {0.12f, 0.16f, 0.22f, 1.0f};
        case ColorRole::SecondaryText:
            return {0.30f, 0.36f, 0.44f, 1.0f};
        case ColorRole::MutedText:
            return {0.47f, 0.53f, 0.60f, 1.0f};
        case ColorRole::Accent:
            return {0.15f, 0.42f, 0.75f, 1.0f};
        case ColorRole::AccentTint:
            return {0.86f, 0.92f, 0.98f, 1.0f};
        case ColorRole::Track:
            return {0.87f, 0.90f, 0.93f, 1.0f};
        case ColorRole::Grid:
            return {0.91f, 0.93f, 0.95f, 1.0f};
        case ColorRole::Live:
            return {0.13f, 0.52f, 0.36f, 1.0f};
        }
    }

    switch (role) {
    case ColorRole::Background:
        return {0.035f, 0.047f, 0.065f, 1.0f};
    case ColorRole::Surface:
        return {0.055f, 0.071f, 0.094f, 1.0f};
    case ColorRole::Inset:
        return {0.042f, 0.055f, 0.074f, 1.0f};
    case ColorRole::Border:
        return {0.12f, 0.15f, 0.20f, 1.0f};
    case ColorRole::PrimaryText:
        return {0.89f, 0.92f, 0.96f, 1.0f};
    case ColorRole::SecondaryText:
        return {0.64f, 0.70f, 0.78f, 1.0f};
    case ColorRole::MutedText:
        return {0.40f, 0.47f, 0.56f, 1.0f};
    case ColorRole::Accent:
        return {0.30f, 0.61f, 0.91f, 1.0f};
    case ColorRole::AccentTint:
        return {0.13f, 0.23f, 0.35f, 1.0f};
    case ColorRole::Track:
        return {0.12f, 0.15f, 0.19f, 1.0f};
    case ColorRole::Grid:
        return {0.10f, 0.13f, 0.17f, 1.0f};
    case ColorRole::Live:
        return {0.34f, 0.72f, 0.56f, 1.0f};
    }
    throw std::logic_error("Unknown RAM monitor color role.");
}

int RamMonitor::AddShape(int parentId, float x, float y, float width,
                         float height, ColorRole role, float radius,
                         bool outlined) {
    const int id = scene->AddRectangle(
        parentId, {x, y, width, height}, GetColor(role), radius,
        outlined ? 1.0f : 0.0f, GetColor(ColorRole::Border));
    colorBindings.push_back({id, role, false});
    if (outlined)
        colorBindings.push_back({id, ColorRole::Border, true});
    return id;
}

int RamMonitor::AddText(int parentId, float x, float y, float fontSize,
                        const std::string &text, ColorRole role) {
    const int id =
        scene->AddText(parentId, x, y, fontSize, text, GetColor(role));
    textBindings.push_back({id, role});
    return id;
}

void RamMonitor::SetText(int elementId, const std::string &text,
                         float fontSize) {
    scene->EditElementText(elementId, text, fontSize);
}

void RamMonitor::ApplyTheme() {
    if (!scene)
        throw std::logic_error("Cannot change the theme before building the UI.");

    for (const ColorBinding &binding : colorBindings) {
        if (binding.border) {
            scene->EditElementBorderColor(binding.elementId,
                                          GetColor(binding.role), false);
        } else {
            scene->EditElementColor(binding.elementId, GetColor(binding.role),
                                    false);
        }
    }
    for (const TextBinding &binding : textBindings) {
        scene->EditElementColor(binding.elementId, GetColor(binding.role), false);
    }
    if (statusDotId != I_UNSET) {
        scene->EditElementColor(
            statusDotId,
            GetColor(lastReadError.empty() ? ColorRole::Live
                                          : ColorRole::MutedText),
            false);
    }
}

void RamMonitor::Build(UIScene &uiScene,
                       const std::array<float, 2> &resolution,
                       pid_t monitoredProcessId) {
    scene = &uiScene;
    processId = monitoredProcessId;
    sample = ProcMemoryReader::ReadProcess(processId);

    const float width = resolution[0];
    const float height = resolution[1];
    const float margin = 64.0f;
    const float contentWidth = width - margin * 2.0f;
    const float cardGap = 18.0f;
    const float halfCardWidth = (contentWidth - cardGap) * 0.5f;

    const int rootId = scene->AddRectangle(
        I_UNSET, {0.0f, 0.0f, width, height}, GetColor(ColorRole::Background));
    scene->SetRoot(rootId);
    colorBindings.push_back({rootId, ColorRole::Background, false});

    AddShape(rootId, margin, 42.0f, 4.0f, 36.0f, ColorRole::Accent, 2.0f);
    AddText(rootId, margin + 16.0f, 38.0f, 25.0f, "RAM MONITOR",
            ColorRole::PrimaryText);
    AddText(rootId, margin + 17.0f, 72.0f, 13.0f,
            "SYSTEM AND PROCESS MEMORY", ColorRole::MutedText);
    statusDotId = AddShape(rootId, width - margin - 340.0f, 52.0f, 8.0f, 8.0f,
                           ColorRole::Live, 4.0f);
    statusTextId = AddText(rootId, width - margin - 322.0f, 47.0f, 14.0f,
                           "LIVE  /proc/" + std::to_string(processId) + "/statm",
                           ColorRole::SecondaryText);
    AddText(rootId, margin + 17.0f, 94.0f, 12.0f, "T  SWITCH THEME",
            ColorRole::MutedText);

    const float cardsY = 126.0f;
    const float cardsHeight = 210.0f;
    const int systemCard =
        AddShape(rootId, margin, cardsY, halfCardWidth, cardsHeight,
                 ColorRole::Surface, 12.0f, true);
    const int processCard =
        AddShape(rootId, margin + halfCardWidth + cardGap, cardsY,
                 halfCardWidth, cardsHeight, ColorRole::Surface, 12.0f, true);

    AddText(systemCard, 24.0f, 20.0f, 13.0f, "SYSTEM MEMORY",
            ColorRole::SecondaryText);
    systemUsedTextId =
        AddText(systemCard, 24.0f, 52.0f, 34.0f, "0.0 GB", ColorRole::PrimaryText);
    systemTotalTextId =
        AddText(systemCard, 24.0f, 100.0f, 15.0f, "of 0.0 GB total",
                ColorRole::MutedText);
    systemPercentTextId =
        AddText(systemCard, halfCardWidth - 125.0f, 62.0f, 22.0f, "0.0%",
                ColorRole::Accent);
    systemProgressWidth = halfCardWidth - 48.0f;
    AddShape(systemCard, 24.0f, 145.0f, systemProgressWidth, 8.0f,
             ColorRole::Track, 4.0f);
    systemProgressId =
        AddShape(systemCard, 24.0f, 145.0f, 1.0f, 8.0f, ColorRole::Accent, 4.0f);
    systemAvailableTextId =
        AddText(systemCard, 24.0f, 170.0f, 14.0f, "0.0 GB available",
                ColorRole::SecondaryText);

    AddText(processCard, 24.0f, 20.0f, 13.0f, "TARGET PROCESS",
            ColorRole::SecondaryText);
    processResidentTextId =
        AddText(processCard, 24.0f, 52.0f, 34.0f, "0.0 MB", ColorRole::PrimaryText);
    processIdTextId =
        AddText(processCard, 24.0f, 101.0f, 15.0f, "RESIDENT SET  |  PID 0",
                ColorRole::MutedText);
    AddShape(processCard, 24.0f, 139.0f, halfCardWidth - 48.0f, 1.0f,
             ColorRole::Grid, 0.0f);
    AddText(processCard, 24.0f, 156.0f, 13.0f,
            "RSS IS AN APPROXIMATE RESIDENT SET SIZE", ColorRole::MutedText);

    const float chartY = 360.0f;
    const float chartCardHeight = 254.0f;
    const int chartCard =
        AddShape(rootId, margin, chartY, contentWidth, chartCardHeight,
                 ColorRole::Surface, 12.0f, true);
    AddText(chartCard, 24.0f, 18.0f, 14.0f, "PROCESS RSS HISTORY",
            ColorRole::SecondaryText);
    AddText(chartCard, 24.0f, 41.0f, 12.0f,
            "ONE SAMPLE PER SECOND  |  LAST 60 SECONDS", ColorRole::MutedText);

    chartX = 24.0f;
    chartHeight = 126.0f;
    chartBaseline = 201.0f;
    chartBarGap = 3.0f;
    const float plotWidth = contentWidth - 48.0f;
    chartBarWidth =
        (plotWidth - chartBarGap * static_cast<float>(HISTORY_LENGTH - 1)) /
        static_cast<float>(HISTORY_LENGTH);
    for (int grid = 1; grid <= 2; ++grid) {
        const float y = chartBaseline - chartHeight * grid / 3.0f;
        AddShape(chartCard, chartX, y, plotWidth, 1.0f, ColorRole::Grid, 0.0f);
    }

    rssHistory.assign(HISTORY_LENGTH, 0);
    historyBars.reserve(HISTORY_LENGTH);
    for (size_t i = 0; i < HISTORY_LENGTH; ++i) {
        const float x = chartX + static_cast<float>(i) *
                                     (chartBarWidth + chartBarGap);
        historyBars.push_back(
            AddShape(chartCard, x, chartBaseline - 2.0f, chartBarWidth, 2.0f,
                     ColorRole::Accent, 2.0f));
    }
    AddText(chartCard, chartX, 215.0f, 12.0f, "60S AGO", ColorRole::MutedText);
    AddText(chartCard, contentWidth - 76.0f, 215.0f, 12.0f, "NOW",
            ColorRole::MutedText);

    const float metricY = 638.0f;
    const float metricHeight = 104.0f;
    const float metricGap = 18.0f;
    const float metricWidth = (contentWidth - 2.0f * metricGap) / 3.0f;
    const int virtualCard =
        AddShape(rootId, margin, metricY, metricWidth, metricHeight,
                 ColorRole::Surface, 10.0f, true);
    const int sharedCard =
        AddShape(rootId, margin + metricWidth + metricGap, metricY, metricWidth,
                 metricHeight, ColorRole::Surface, 10.0f, true);
    const int dataCard =
        AddShape(rootId, margin + 2.0f * (metricWidth + metricGap), metricY,
                 metricWidth, metricHeight, ColorRole::Surface, 10.0f, true);
    AddText(virtualCard, 20.0f, 17.0f, 12.0f, "VIRTUAL SIZE",
            ColorRole::MutedText);
    processVirtualTextId =
        AddText(virtualCard, 20.0f, 43.0f, 22.0f, "0.0 MB",
                ColorRole::PrimaryText);
    AddText(sharedCard, 20.0f, 17.0f, 12.0f, "SHARED PAGES",
            ColorRole::MutedText);
    processSharedTextId =
        AddText(sharedCard, 20.0f, 43.0f, 22.0f, "0.0 MB",
                ColorRole::PrimaryText);
    AddText(dataCard, 20.0f, 17.0f, 12.0f, "DATA + STACK",
            ColorRole::MutedText);
    processDataTextId =
        AddText(dataCard, 20.0f, 43.0f, 22.0f, "0.0 MB",
                ColorRole::PrimaryText);

    processPickerX = width - margin - 340.0f;
    processPickerY = 84.0f;
    processPickerWidth = 340.0f;
    processPickerHeight = 32.0f;
    processRowHeight = 32.0f;
    processPickerId =
        AddShape(rootId, processPickerX, processPickerY, processPickerWidth,
                 processPickerHeight, ColorRole::Inset, 7.0f, true);
    processPickerTextId =
        AddText(rootId, processPickerX + 12.0f, processPickerY + 8.0f, 13.0f,
                "PROCESS  |  PID " + std::to_string(processId),
                ColorRole::PrimaryText);

    const float menuY = processPickerY + processPickerHeight + 4.0f;
    const float menuHeaderHeight = 34.0f;
    const float menuHeight =
        menuHeaderHeight + processRowHeight * PROCESS_MENU_ROW_COUNT;
    processMenuId =
        AddShape(rootId, processPickerX, menuY, processPickerWidth, menuHeight,
                 ColorRole::Surface, 8.0f, true);
    processMenuHeaderId =
        AddText(processMenuId, 12.0f, 8.0f, 12.0f, "ACTIVE PROCESSES",
                ColorRole::MutedText);
    processMenuCountTextId =
        AddText(processMenuId, processPickerWidth - 68.0f, 8.0f, 12.0f, "0",
                ColorRole::MutedText);
    for (size_t i = 0; i < PROCESS_MENU_ROW_COUNT; ++i) {
        const float rowY = menuHeaderHeight + processRowHeight * i;
        const int rowBackground =
            AddShape(processMenuId, 6.0f, rowY, processPickerWidth - 12.0f,
                     processRowHeight, ColorRole::Inset, 5.0f);
        const int rowText =
            AddText(processMenuId, 16.0f, rowY + 8.0f, 13.0f, "",
                    ColorRole::SecondaryText);
        processRows.push_back({rowBackground, rowText, 0, 0});
    }

    RefreshProcessList();
    UpdateProcessMenuRows();
    SetProcessMenuOpen(false);

    ApplySample(sample);
    UpdateHistoryChart();
}

void RamMonitor::ApplySample(const RamSample &newSample) {
    sample = newSample;
    const uint64_t systemUsedBytes =
        sample.systemTotalBytes - sample.systemAvailableBytes;

    SetText(systemUsedTextId, FormatBytes(systemUsedBytes), 34.0f);
    SetText(systemTotalTextId,
            "of " + FormatBytes(sample.systemTotalBytes) + " total", 15.0f);
    SetText(systemAvailableTextId,
            FormatBytes(sample.systemAvailableBytes) + " available", 14.0f);
    SetText(systemPercentTextId,
            FormatPercent(systemUsedBytes, sample.systemTotalBytes), 22.0f);
    SetText(processResidentTextId, FormatBytes(sample.processResidentBytes), 34.0f);
    SetText(processIdTextId,
            "RESIDENT SET  |  PID " + std::to_string(processId), 15.0f);
    const auto selectedProcess =
        std::find_if(processes.begin(), processes.end(),
                     [this](const ProcessInfo &process) {
                         return process.processId == processId;
                     });
    SetText(processPickerTextId,
            selectedProcess == processes.end()
                ? "PID " + std::to_string(processId) + "   v"
                : FormatProcessLabel(*selectedProcess) + "   v",
            13.0f);
    SetText(processVirtualTextId, FormatBytes(sample.processVirtualBytes), 22.0f);
    SetText(processSharedTextId, FormatBytes(sample.processSharedBytes), 22.0f);
    SetText(processDataTextId, FormatBytes(sample.processDataBytes), 22.0f);

    const float progress =
        sample.systemTotalBytes == 0
            ? 0.0f
            : std::clamp(static_cast<float>(
                             static_cast<double>(systemUsedBytes) /
                             static_cast<double>(sample.systemTotalBytes)),
                         0.0f, 1.0f);
    scene->EditElementShape(
        systemProgressId,
        {scene->GetLocalX(systemProgressId), scene->GetLocalY(systemProgressId),
         std::max(1.0f, systemProgressWidth * progress), 8.0f},
        false);

    if (!rssHistory.empty()) {
        rssHistory.erase(rssHistory.begin());
        rssHistory.push_back(sample.processResidentBytes);
    }
    UpdateHistoryChart();
}

void RamMonitor::UpdateHistoryChart() {
    uint64_t peak = 0;
    for (uint64_t value : rssHistory)
        peak = std::max(peak, value);
    const double scale = std::max(1.0, static_cast<double>(peak) * 1.15);

    for (size_t i = 0; i < historyBars.size(); ++i) {
        const double ratio = static_cast<double>(rssHistory[i]) / scale;
        const float barHeight =
            rssHistory[i] == 0
                ? 2.0f
                : std::max(3.0f, static_cast<float>(ratio * chartHeight));
        const float x = chartX + static_cast<float>(i) *
                                     (chartBarWidth + chartBarGap);
        scene->EditElementShape(
            historyBars[i],
            {x, chartBaseline - barHeight, chartBarWidth, barHeight}, false);
    }
}

void RamMonitor::SetReadError(const std::string &message) {
    if (lastReadError != message) {
        std::cerr << "RAM monitor data refresh failed: " << message << '\n';
        lastReadError = message;
    }
    if (statusTextId != I_UNSET)
        SetText(statusTextId, "STATM READ ERROR", 14.0f);
    scene->EditElementColor(statusDotId, GetColor(ColorRole::MutedText), false);
}

void RamMonitor::RefreshProcessList() {
    processes = ProcMemoryReader::ListActiveProcesses();
    const size_t maxOffset =
        processes.size() > PROCESS_MENU_ROW_COUNT
            ? processes.size() - PROCESS_MENU_ROW_COUNT
            : 0;
    processScrollOffset = std::min(processScrollOffset, maxOffset);
}

std::string RamMonitor::FormatProcessLabel(const ProcessInfo &process) const {
    constexpr size_t MAX_NAME_LENGTH = 25;
    std::string name = process.name;
    if (name.size() > MAX_NAME_LENGTH)
        name = name.substr(0, MAX_NAME_LENGTH - 3) + "...";
    return "PID " + std::to_string(process.processId) + "  |  " + name;
}

void RamMonitor::UpdateProcessMenuRows() {
    if (processMenuCountTextId != I_UNSET) {
        SetText(processMenuCountTextId, std::to_string(processes.size()), 12.0f);
    }

    for (size_t rowIndex = 0; rowIndex < processRows.size(); ++rowIndex) {
        ProcessRow &row = processRows[rowIndex];
        const size_t processIndex = processScrollOffset + rowIndex;
        const bool hasProcess = processIndex < processes.size();
        scene->EditElementVisibility(row.backgroundId,
                                     !processMenuOpen || !hasProcess, false);
        scene->EditElementVisibility(row.textId,
                                     !processMenuOpen || !hasProcess, false);
        if (!hasProcess) {
            row.processId = 0;
            continue;
        }

        const ProcessInfo &process = processes[processIndex];
        row.processId = process.processId;
        row.processIndex = processIndex;
        SetText(row.textId, FormatProcessLabel(process), 13.0f);
        const bool selected = process.processId == processId;
        const ColorRole backgroundRole =
            selected ? ColorRole::AccentTint : ColorRole::Inset;
        const ColorRole textRole =
            selected ? ColorRole::PrimaryText : ColorRole::SecondaryText;
        scene->EditElementColor(row.backgroundId, GetColor(backgroundRole), false);
        scene->EditElementColor(row.textId, GetColor(textRole), false);
        for (ColorBinding &binding : colorBindings) {
            if (binding.elementId == row.backgroundId)
                binding.role = backgroundRole;
        }
        for (TextBinding &binding : textBindings) {
            if (binding.elementId == row.textId)
                binding.role = textRole;
        }
    }
}

void RamMonitor::SetProcessMenuOpen(bool open) {
    processMenuOpen = open;
    scene->EditElementVisibility(processMenuId, !open, false);
    scene->EditElementVisibility(processMenuHeaderId, !open, false);
    scene->EditElementVisibility(processMenuCountTextId, !open, false);
    UpdateProcessMenuRows();
    if (open) {
        nextProcessScanTime = 0.0;
    }
}

bool RamMonitor::Contains(int elementId, double x, double y) const {
    if (elementId < 0)
        return false;
    const float left = scene->GetAbsoluteX(elementId);
    const float top = scene->GetAbsoluteY(elementId);
    const float right = left + scene->GetWidth(elementId);
    const float bottom = top + scene->GetHeight(elementId);
    return x >= left && x <= right && y >= top && y <= bottom;
}

void RamMonitor::SelectProcess(pid_t selectedProcessId) {
    try {
        const RamSample selectedSample =
            ProcMemoryReader::ReadProcess(selectedProcessId);
        processId = selectedProcessId;
        rssHistory.assign(HISTORY_LENGTH, 0);
        rssHistory.back() = selectedSample.processResidentBytes;
        ApplySample(selectedSample);
        lastReadError.clear();
        SetText(statusTextId,
                "LIVE  /proc/" + std::to_string(processId) + "/statm", 14.0f);
        scene->EditElementColor(statusDotId, GetColor(ColorRole::Live), false);
        UpdateProcessMenuRows();
        SetProcessMenuOpen(false);
    } catch (const std::exception &error) {
        SetReadError(error.what());
        RefreshProcessList();
        UpdateProcessMenuRows();
    }
}

void RamMonitor::HandleMouseClick(double x, double y) {
    if (!scene)
        return;

    if (processMenuOpen) {
        for (const ProcessRow &row : processRows) {
            if (row.processId != 0 && Contains(row.backgroundId, x, y)) {
                SelectProcess(row.processId);
                return;
            }
        }
    }

    if (Contains(processPickerId, x, y)) {
        SetProcessMenuOpen(!processMenuOpen);
        return;
    }

    if (processMenuOpen && !Contains(processMenuId, x, y))
        SetProcessMenuOpen(false);
}

void RamMonitor::HandleScroll(double offset) {
    if (!processMenuOpen || offset == 0.0 || processes.empty())
        return;

    const size_t maxOffset =
        processes.size() > PROCESS_MENU_ROW_COUNT
            ? processes.size() - PROCESS_MENU_ROW_COUNT
            : 0;
    if (offset > 0.0) {
        processScrollOffset =
            processScrollOffset > 0 ? processScrollOffset - 1 : 0;
    } else {
        processScrollOffset = std::min(processScrollOffset + 1, maxOffset);
    }
    UpdateProcessMenuRows();
}

void RamMonitor::Update(double currentTime) {
    if (!scene)
        return;

    if (currentTime >= nextSampleTime) {
        nextSampleTime = currentTime + 1.0;
        try {
            ApplySample(ProcMemoryReader::ReadProcess(processId));
            lastReadError.clear();
            SetText(statusTextId,
                    "LIVE  /proc/" + std::to_string(processId) + "/statm",
                    14.0f);
            scene->EditElementColor(statusDotId, GetColor(ColorRole::Live),
                                    false);
        } catch (const std::exception &error) {
            SetReadError(error.what());
        }
    }

    if (processMenuOpen && currentTime >= nextProcessScanTime) {
        nextProcessScanTime = currentTime + PROCESS_SCAN_INTERVAL;
        try {
            RefreshProcessList();
            UpdateProcessMenuRows();
        } catch (const std::exception &error) {
            SetReadError(error.what());
        }
    }
}

void RamMonitor::ToggleTheme() {
    if (!scene)
        throw std::logic_error("Cannot change the theme before building the UI.");
    lightTheme = !lightTheme;
    ApplyTheme();
}
