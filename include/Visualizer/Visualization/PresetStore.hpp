#pragma once

#include "Visualizer/Visualization/VisualizerEngine.hpp"

#include <filesystem>
#include <optional>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace viz {

struct VisualPreset {
    std::string name = "Untitled";
    VisualSettings settings{};
};

struct PresetLibraryEntry {
    std::string name = "Untitled";
    std::filesystem::path path;
};

[[nodiscard]] std::optional<VisualMode> parseVisualMode(std::string_view value);
[[nodiscard]] std::optional<Palette> parsePalette(std::string_view value);
[[nodiscard]] std::optional<MotionStyle> parseMotionStyle(std::string_view value);
[[nodiscard]] const std::vector<VisualPreset>& curatedPresets();
[[nodiscard]] std::optional<std::size_t> findCuratedPresetIndex(std::string_view value);
[[nodiscard]] std::optional<VisualPreset> findCuratedPreset(std::string_view value);
[[nodiscard]] std::string sanitizePresetFileStem(std::string_view name);
[[nodiscard]] std::filesystem::path defaultUserPresetDirectory();
[[nodiscard]] std::vector<PresetLibraryEntry> scanUserPresetLibrary(const std::filesystem::path& directory);
[[nodiscard]] std::optional<PresetLibraryEntry> findUserPreset(const std::filesystem::path& directory,
                                                               std::string_view value);

bool savePreset(const std::filesystem::path& path,
                const VisualPreset& preset,
                std::string& error);

[[nodiscard]] std::optional<VisualPreset> loadPreset(const std::filesystem::path& path,
                                                     std::string& error);

bool saveUserPreset(const std::filesystem::path& directory,
                    const VisualPreset& preset,
                    std::filesystem::path& savedPath,
                    std::string& error);

[[nodiscard]] std::optional<VisualPreset> loadUserPresetEntry(const PresetLibraryEntry& entry,
                                                              std::string& error);

} // namespace viz
