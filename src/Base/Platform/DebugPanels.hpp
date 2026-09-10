#pragma once

#include <Base/Context.hpp>
#include <Base/Draw.hpp>
#include <Base/GameObject.hpp>
#include <Base/Instances.hpp>
#include <Base/Variables.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Base::Platform {

// 同じ種別のインスタンスをまとめたもの
struct HierarchyGroup {
    std::string type;
    std::vector<InstanceId> instances;
};

// 実装が付ける綴りから、読める名前を作る
std::string TypeNameOf(const GameObject &object);

// 種別ごとにまとめる
// 種別は名前の順、インスタンスは生成順
std::vector<HierarchyGroup> GroupInstances(Instances &instances);

// 選んでいるものを持つ
struct DebugSelection {
    InstanceId instance = InstanceId::None;
};

// Room→種別→インスタンスの並びを出す
// 親子関係ではなく、見せ方だけのまとまり
void DrawHierarchy(const char *title, bool &open, Context &context, std::string_view room,
                   DebugSelection &selection);

// 選んだインスタンスの変数を表から並べる
void DrawInspector(const char *title, bool &open, Context &context,
                   DebugSelection &selection);

// 破棄の手続きを通して消す
// 選んでいたものなら選択も外す
void DestroyInstance(Context &context, DebugSelection &selection, InstanceId id);

const char *NameOf(DrawKind kind);
const char *NameOf(BlendMode blend);

// 積み荷を1つずつ並べる
// 選んでいるインスタンスが積んだものだけに絞れる
void DrawCommandList(const char *title, bool &open, const DrawList &list,
                     const DebugSelection &selection, bool &onlySelected);

// 種類ごとに読み書きできるか
bool Editable(VariableKind kind);

} // namespace TellerEngine::Base::Platform
