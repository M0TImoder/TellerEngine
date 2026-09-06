#pragma once

// データ層の失敗を報告する

#include <string>

namespace TellerEngine::Base {

enum class ErrorCode {
    NotFound,
    OutOfRange,
    ReadFailed,
    Malformed,
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
    }
    return "Unknown";
}

} // namespace TellerEngineEngine::Base::Data
