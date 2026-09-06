#pragma once

#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>
#include <Extract/Strings.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Extract {

inline constexpr std::uint64_t CodeEntrySize = 20;
inline constexpr std::uint64_t VariableHeaderSize = 12;
inline constexpr std::uint64_t VariableEntrySize = 20;
inline constexpr std::uint64_t FunctionEntrySize = 12;

struct CodeEntry {
    std::string name;
    std::uint32_t length = 0;
    std::uint16_t localCount = 0;
    std::uint16_t argumentCount = 0;

    bool weirdLocalFlag = false;

    // バイトコード本体のファイル上の位置
    std::uint64_t bytecodeOffset = 0;
    std::uint32_t offset = 0;
};

struct CodeTable {
    std::vector<CodeEntry> entries;
};

inline Expected<CodeTable, TellerEngine::Base::Error>
ReadCode(const TellerEngine::Extract::Bytes &bytes, std::string_view name,
         const Chunk &chunk, const StringTable &strings) {
    const auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    const auto pointers = ReadPointerList(view, chunk, "CODE");
    if (!pointers) {
        return Unexpected<TellerEngine::Base::Error>(pointers.error());
    }

    CodeTable table;
    table.entries.reserve(pointers->size());

    for (const auto pointer : *pointers) {
        if (!InChunk(chunk, pointer, CodeEntrySize)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("CODE entry runs past the end of the chunk"));
        }
        const auto at = LocalOffset(chunk, pointer);

        CodeEntry entry;
        auto text = ResolveString(strings, ReadU32(view, at), "CODE name");
        if (!text) {
            return Unexpected<TellerEngine::Base::Error>(text.error());
        }
        entry.name = std::move(*text);
        entry.length = ReadU32(view, at + 4);
        entry.localCount = ReadU16(view, at + 8);
        const std::uint16_t arguments = ReadU16(view, at + 10);
        entry.weirdLocalFlag = (arguments & 0x8000) != 0;
        entry.argumentCount = static_cast<std::uint16_t>(arguments & 0x7fff);

        // 相対位置は、その欄そのものの位置からの差分
        const std::int32_t relative = ReadI32(view, at + 12);
        entry.bytecodeOffset = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(pointer) + 12 + relative);
        entry.offset = ReadU32(view, at + 16);

        if (!InChunk(chunk, entry.bytecodeOffset, entry.length)) {
            return Unexpected<TellerEngine::Base::Error>(Malformed(
                entry.name + " bytecode runs past the end of the chunk"));
        }
        table.entries.push_back(std::move(entry));
    }

    return table;
}

inline Expected<TellerEngine::Extract::ByteBuffer,
                TellerEngine::Base::Error>
ReadBytecode(const TellerEngine::Extract::Bytes &bytes,
             std::string_view name, const CodeEntry &entry) {
    return bytes.Read(name, entry.bytecodeOffset, entry.length);
}

struct Variable {
    std::string name;
    std::int32_t instanceType = 0;
    std::int32_t id = 0;
    std::uint32_t occurrences = 0;
    std::int32_t firstAddress = -1;
};

struct VariableTable {
    std::uint32_t declaredCount = 0;
    std::uint32_t instanceCount = 0;
    std::uint32_t maxLocalCount = 0;
    std::vector<Variable> variables;
};

inline Expected<VariableTable, TellerEngine::Base::Error>
ReadVariables(const TellerEngine::Extract::Bytes &bytes,
              std::string_view name, const Chunk &chunk,
              const StringTable &strings) {
    const auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    if (view.size() < VariableHeaderSize) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed("VARI is smaller than its header"));
    }
    if ((view.size() - VariableHeaderSize) % VariableEntrySize != 0) {
        return Unexpected<TellerEngine::Base::Error>(Malformed(
            "VARI has " + std::to_string(view.size() - VariableHeaderSize) +
            " bytes of entries which is not a multiple of 20"));
    }

    VariableTable table;
    table.declaredCount = ReadU32(view, 0);
    table.instanceCount = ReadU32(view, 4);
    table.maxLocalCount = ReadU32(view, 8);

    const std::size_t count =
        (view.size() - VariableHeaderSize) / VariableEntrySize;
    table.variables.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t at = VariableHeaderSize + index * VariableEntrySize;

        Variable variable;
        auto text = ResolveString(strings, ReadU32(view, at), "VARI name");
        if (!text) {
            return Unexpected<TellerEngine::Base::Error>(text.error());
        }
        variable.name = std::move(*text);
        variable.instanceType = ReadI32(view, at + 4);
        variable.id = ReadI32(view, at + 8);
        variable.occurrences = ReadU32(view, at + 12);
        variable.firstAddress = ReadI32(view, at + 16);
        table.variables.push_back(std::move(variable));
    }

    return table;
}

struct Function {
    std::string name;
    std::uint32_t occurrences = 0;
    std::uint32_t firstAddress = 0;
};

struct LocalScope {
    std::string code;
    std::vector<std::string> locals;
};

struct FunctionTable {
    std::vector<Function> functions;
    std::vector<LocalScope> scopes;
};

inline Expected<FunctionTable, TellerEngine::Base::Error>
ReadFunctions(const TellerEngine::Extract::Bytes &bytes,
              std::string_view name, const Chunk &chunk,
              const StringTable &strings) {
    const auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    if (view.size() < 4) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed("FUNC is smaller than its count field"));
    }

    FunctionTable table;
    const std::uint64_t count = ReadU32(view, 0);
    if (4 + count * FunctionEntrySize > view.size()) {
        return Unexpected<TellerEngine::Base::Error>(
            Malformed("FUNC declares " + std::to_string(count) +
                      " functions but is only " + std::to_string(view.size()) +
                      " bytes"));
    }

    table.functions.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0; index < count; ++index) {
        const auto at = static_cast<std::size_t>(4 + index * FunctionEntrySize);

        Function function;
        auto text = ResolveString(strings, ReadU32(view, at), "FUNC name");
        if (!text) {
            return Unexpected<TellerEngine::Base::Error>(text.error());
        }
        function.name = std::move(*text);
        function.occurrences = ReadU32(view, at + 4);
        function.firstAddress = ReadU32(view, at + 8);
        table.functions.push_back(std::move(function));
    }

    std::size_t cursor =
        static_cast<std::size_t>(4 + count * FunctionEntrySize);
    if (cursor + 4 > view.size()) {
        return table;
    }

    const std::uint64_t scopeCount = ReadU32(view, cursor);
    cursor += 4;
    table.scopes.reserve(static_cast<std::size_t>(scopeCount));

    for (std::uint64_t index = 0; index < scopeCount; ++index) {
        if (cursor + 8 > view.size()) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("FUNC local scope " + std::to_string(index) +
                          " runs past the end"));
        }
        const std::uint64_t localCount = ReadU32(view, cursor);

        LocalScope scope;
        auto text = ResolveString(strings, ReadU32(view, cursor + 4),
                                  "FUNC scope name");
        if (!text) {
            return Unexpected<TellerEngine::Base::Error>(text.error());
        }
        scope.code = std::move(*text);
        cursor += 8;

        if (cursor + localCount * 8 > view.size()) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed(scope.code + " has locals which run past the end"));
        }

        scope.locals.reserve(static_cast<std::size_t>(localCount));
        for (std::uint64_t local = 0; local < localCount; ++local) {
            auto localName = ResolveString(strings, ReadU32(view, cursor + 4),
                                           "FUNC local name");
            if (!localName) {
                return Unexpected<TellerEngine::Base::Error>(
                    localName.error());
            }
            scope.locals.push_back(std::move(*localName));
            cursor += 8;
        }
        table.scopes.push_back(std::move(scope));
    }

    return table;
}

} // namespace TellerEngine::Extract
