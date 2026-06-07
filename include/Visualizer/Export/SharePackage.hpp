#pragma once

#include "Visualizer/Export/OfflineExporter.hpp"

#include <string>

namespace viz {

bool writeSharePackage(const OfflineExportOptions& options,
                       OfflineExportResult& result,
                       std::string& error);

} // namespace viz
