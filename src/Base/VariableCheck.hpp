#pragma once

#include <Base/Variables.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace TellerEngine::Base {

template <typename T> constexpr bool HasUniqueVariableNames() {
    constexpr auto descriptors = DescribeVariables<T>();
    for (std::size_t i = 0; i < descriptors.size(); ++i) {
        for (std::size_t j = i + 1; j < descriptors.size(); ++j) {
            if (descriptors[i].name == descriptors[j].name ||
                descriptors[i].name == descriptors[j].original ||
                descriptors[i].original == descriptors[j].name ||
                descriptors[i].original == descriptors[j].original) {
                return false;
            }
        }
    }
    return true;
}

template <typename T> constexpr bool VariablesFitInObject() {
    return RegisteredBytes<T>() <= sizeof(T);
}

struct VariableGap {
    std::size_t offset = 0;
    std::size_t size = 0;
};

struct VariableCoverage {
    static constexpr std::size_t kMaxGaps = 8;

    std::size_t objectSize = 0;
    std::size_t registeredBytes = 0;
    std::array<VariableGap, kMaxGaps> gaps{};
    std::size_t gapCount = 0;
    std::size_t overlapCount = 0;
    bool truncated = false;

    constexpr bool Ok() const { return gapCount == 0 && overlapCount == 0; }
};

template <typename T> VariableCoverage CheckVariableCoverage(const T &object) {
    struct Placement {
        std::size_t offset = 0;
        std::size_t size = 0;
    };

    VariableCoverage coverage;
    coverage.objectSize = sizeof(T);
    coverage.registeredBytes = RegisteredBytes<T>();

    std::array<Placement, VariableCount<T>()> placements{};
    std::size_t index = 0;
    const auto *origin = reinterpret_cast<const std::byte *>(&object);
    ForEachVariable(object, [&](const VariableDescriptor &descriptor, ConstVariableRef ref) {
        const auto *address = static_cast<const std::byte *>(ref.data);
        placements[index] = Placement{static_cast<std::size_t>(address - origin),
                                      descriptor.size};
        index += 1;
    });

    std::sort(placements.begin(), placements.end(),
              [](const Placement &lhs, const Placement &rhs) {
                  return lhs.offset < rhs.offset;
              });

    const std::size_t padding = alignof(T);

    const auto note = [&](std::size_t offset, std::size_t size) {
        if (size < padding) {
            return;
        }
        if (coverage.gapCount >= VariableCoverage::kMaxGaps) {
            coverage.truncated = true;
            return;
        }
        coverage.gaps[coverage.gapCount] = VariableGap{offset, size};
        coverage.gapCount += 1;
    };

    std::size_t cursor = std::is_polymorphic_v<T> ? sizeof(void *) : 0;
    for (const Placement &placement : placements) {
        if (placement.offset > cursor) {
            note(cursor, placement.offset - cursor);
        } else if (placement.offset < cursor) {
            coverage.overlapCount += 1;
        }
        cursor = std::max(cursor, placement.offset + placement.size);
    }
    if (cursor < sizeof(T)) {
        note(cursor, sizeof(T) - cursor);
    }

    return coverage;
}

} // namespace TellerEngine::Base
