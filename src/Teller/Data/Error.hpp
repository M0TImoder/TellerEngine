#pragma once

// データ層の失敗を報告する
// メッセージは事実の報告に留める

#include <string>

namespace Teller::Data {

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

} // namespace Teller::Data
