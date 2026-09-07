#pragma once

// データ層の失敗を報告する

#include <string>

namespace TellerEngine::Base {

enum class ErrorCode {
    NotFound,
    OutOfRange,
    ReadFailed,
    Malformed,
    Unavailable,
};

struct Error {
    ErrorCode code = ErrorCode::NotFound;
    std::string context;
};

inline const char *ToString(ErrorCode code) {
    switch (code) {
    case ErrorCode::NotFound:
        return "NotFound";
    case ErrorCode::OutOfRange:
        return "OutOfRange";
    case ErrorCode::ReadFailed:
        return "ReadFailed";
    case ErrorCode::Malformed:
        return "Malformed";
    case ErrorCode::Unavailable:
        return "Unavailable";
    }
    return "Unknown";
}

} // namespace TellerEngine::Base
