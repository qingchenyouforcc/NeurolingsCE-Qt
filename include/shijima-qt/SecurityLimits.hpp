#pragma once

#include <cstddef>
#include <cstdint>

namespace SecurityLimits {

inline constexpr std::size_t kIpcMessageMaxBytes = 1024 * 1024;
inline constexpr std::size_t kHttpJsonBodyMaxBytes = 1024 * 1024;

inline constexpr std::uint64_t kMascotPackageMaxBytes = 100ULL * 1024ULL * 1024ULL;
inline constexpr std::uint64_t kMascotExtractedMaxBytes = 100ULL * 1024ULL * 1024ULL;
inline constexpr std::uint64_t kMascotSingleFileMaxBytes = 16ULL * 1024ULL * 1024ULL;
inline constexpr std::uint64_t kMascotAudioFileMaxBytes = 16ULL * 1024ULL * 1024ULL;
inline constexpr std::uint64_t kMascotImageMaxPixels = 4096ULL * 4096ULL;
inline constexpr std::size_t kMascotZipEntryMaxCount = 4096;

}
