#include <Base/Platform/DebugUi.hpp>

#include <cstdio>

#include <string>
#include <utility>
#include <vector>

#ifdef TELLER_DEBUG_UI
#include <Base/Platform/DebugUiRenderer.hpp>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#endif

namespace TellerEngine::Base::Platform {

#ifdef TELLER_DEBUG_UI

namespace {

// 呼び出し口が占める場所
DebugBounds StripOf(const ImGuiViewport &main, DebugEdge edge, float iconSize,
                    std::size_t count) {
    if (count == 0) {
        return {};
    }
    const float pad = ImGui::GetStyle().WindowPadding.x;
    const float gap = ImGui::GetStyle().ItemSpacing.y;
    const float thick = iconSize + pad * 2.0f;
    const auto many = static_cast<float>(count);
    const float along = many * iconSize + (many - 1.0f) * gap + pad * 2.0f;

    const bool vertical = edge == DebugEdge::Left || edge == DebugEdge::Right;
    const ImVec2 size = vertical ? ImVec2{thick, along} : ImVec2{along, thick};
    ImVec2 position = main.WorkPos;
    if (edge == DebugEdge::Right) {
        position.x += main.WorkSize.x - size.x;
    } else if (edge == DebugEdge::Bottom) {
        position.y += main.WorkSize.y - size.y;
    }
    return DebugBounds{position.x, position.y, size.x, size.y};
}

} // namespace

struct DebugUi::State {
    // 呼び出し口の1つ
    struct Tool {
        std::string name;
        SDL_Texture *icon = nullptr;
        bool open = false;
    };

    ImGuiContext *context = nullptr;
    SDL_Window *window = nullptr;
    std::size_t drawCount = 0;

    std::vector<Tool> tools;
    DebugEdge edge = DebugEdge::Left;
    float iconSize = 32.0f;
    DebugBounds bounds;
    DebugBounds dockBounds;
    bool dock = false;

    ~State() {
        if (context == nullptr) {
            return;
        }
        ImGui::SetCurrentContext(context);
        DebugUiRender::Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(context);
    }
};

Expected<DebugUi, Error> DebugUi::Create(SDL_Window *window, SDL_Renderer *renderer) {
    if (window == nullptr || renderer == nullptr) {
        return Unexpected<Error>(Error{ErrorCode::Unavailable, "窓か描画先が無い"});
    }

    DebugUi ui;
    ui.state_ = std::make_unique<State>();
    ui.state_->window = window;

    IMGUI_CHECKVERSION();
    ui.state_->context = ImGui::CreateContext();
    if (ui.state_->context == nullptr) {
        return Unexpected<Error>(Error{ErrorCode::Unavailable, "ImGuiを作れない"});
    }
    ImGui::SetCurrentContext(ui.state_->context);

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)) {
        return Unexpected<Error>(Error{ErrorCode::Unavailable, "SDL3の口を開けない"});
    }
    if (!DebugUiRender::Init(renderer)) {
        return Unexpected<Error>(Error{ErrorCode::Unavailable, "描画の口を開けない"});
    }

#ifndef __EMSCRIPTEN__
    ui.SetViewports(true);
#endif
    return ui;
}

bool DebugUi::Handle(const SDL_Event &event) {
    if (state_ == nullptr) {
        return false;
    }
    ImGui::SetCurrentContext(state_->context);
    return ImGui_ImplSDL3_ProcessEvent(&event);
}

void DebugUi::NewFrame() {
    if (state_ == nullptr) {
        return;
    }
    ImGui::SetCurrentContext(state_->context);
    DebugUiRender::NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void DebugUi::Render(SDL_Renderer *renderer) {
    if (state_ == nullptr) {
        return;
    }
    ImGui::SetCurrentContext(state_->context);
    ImGui::Render();

    ImDrawData *data = ImGui::GetDrawData();
    state_->drawCount = data == nullptr ? 0 : static_cast<std::size_t>(data->TotalIdxCount) / 3;
    if (data != nullptr) {
        DebugUiRender::Draw(data, renderer);
    }

    if (Viewports()) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void DebugUi::SetViewports(bool enabled) {
    if (state_ == nullptr) {
        return;
    }
    ImGui::SetCurrentContext(state_->context);
    ImGuiIO &io = ImGui::GetIO();
    if (enabled) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        return;
    }

    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
}

bool DebugUi::Viewports() const {
    if (state_ == nullptr) {
        return false;
    }
    ImGui::SetCurrentContext(state_->context);
    return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
}

std::size_t DebugUi::WindowCount() const {
    if (state_ == nullptr) {
        return 0;
    }
    ImGui::SetCurrentContext(state_->context);
    std::size_t count = 0;
    for (const ImGuiViewport *viewport : ImGui::GetPlatformIO().Viewports) {
        if (viewport->PlatformHandle != nullptr) {
            count += 1;
        }
    }
    return count;
}

bool DebugUi::WantsMouse() const {
    if (state_ == nullptr) {
        return false;
    }
    ImGui::SetCurrentContext(state_->context);
    return ImGui::GetIO().WantCaptureMouse;
}

bool DebugUi::WantsKeyboard() const {
    if (state_ == nullptr) {
        return false;
    }
    ImGui::SetCurrentContext(state_->context);
    return ImGui::GetIO().WantCaptureKeyboard;
}

void DebugUi::SetDocking(bool enabled) {
    if (state_ == nullptr) {
        return;
    }
    ImGui::SetCurrentContext(state_->context);
    ImGuiIO &io = ImGui::GetIO();
    if (enabled) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
    }
}

bool DebugUi::Docking() const {
    if (state_ == nullptr) {
        return false;
    }
    ImGui::SetCurrentContext(state_->context);
    return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) != 0;
}

std::size_t DebugUi::DrawCount() const {
    return state_ == nullptr ? 0 : state_->drawCount;
}

std::size_t DebugUi::AddTool(std::string name) {
    if (state_ == nullptr) {
        return 0;
    }
    state_->tools.push_back(State::Tool{std::move(name), nullptr, false});
    return state_->tools.size() - 1;
}

void DebugUi::SetIcon(std::size_t tool, SDL_Texture *icon) {
    if (state_ == nullptr || tool >= state_->tools.size()) {
        return;
    }
    state_->tools[tool].icon = icon;
}

bool DebugUi::ToolOpen(std::size_t tool) const {
    if (state_ == nullptr || tool >= state_->tools.size()) {
        return false;
    }
    return state_->tools[tool].open;
}

void DebugUi::SetToolOpen(std::size_t tool, bool open) {
    if (state_ == nullptr || tool >= state_->tools.size()) {
        return;
    }
    state_->tools[tool].open = open;
}

std::size_t DebugUi::ToolCount() const {
    return state_ == nullptr ? 0 : state_->tools.size();
}

void DebugUi::SetEdge(DebugEdge edge) {
    if (state_ != nullptr) {
        state_->edge = edge;
    }
}

DebugEdge DebugUi::Edge() const {
    return state_ == nullptr ? DebugEdge::Left : state_->edge;
}

void DebugUi::SetIconSize(double size) {
    if (state_ != nullptr && size > 0.0) {
        state_->iconSize = static_cast<float>(size);
    }
}

double DebugUi::IconSize() const { return state_ == nullptr ? 0.0 : state_->iconSize; }

DebugBounds DebugUi::ToolBounds() const {
    return state_ == nullptr ? DebugBounds{} : state_->bounds;
}

void DebugUi::SetDock(bool enabled) {
    if (state_ != nullptr) {
        state_->dock = enabled;
    }
}

bool DebugUi::Dock() const { return state_ != nullptr && state_->dock; }

DebugBounds DebugUi::DockBounds() const {
    return state_ == nullptr ? DebugBounds{} : state_->dockBounds;
}

void DebugUi::DrawDock() {
    if (state_ == nullptr || !state_->dock) {
        return;
    }
    ImGui::SetCurrentContext(state_->context);

    const ImGuiViewport *main = ImGui::GetMainViewport();
    ImVec2 position = main->WorkPos;
    ImVec2 size = main->WorkSize;

    // 呼び出し口のぶんを空ける
    const DebugBounds strip =
        StripOf(*main, state_->edge, state_->iconSize, state_->tools.size());
    const auto thick = static_cast<float>(state_->edge == DebugEdge::Left ||
                                                  state_->edge == DebugEdge::Right
                                              ? strip.width
                                              : strip.height);
    switch (state_->edge) {
    case DebugEdge::Left:
        position.x += thick;
        size.x -= thick;
        break;
    case DebugEdge::Right:
        size.x -= thick;
        break;
    case DebugEdge::Top:
        position.y += thick;
        size.y -= thick;
        break;
    case DebugEdge::Bottom:
        size.y -= thick;
        break;
    }

    state_->dockBounds = DebugBounds{position.x, position.y, size.x, size.y};

    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(main->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##dock", nullptr, flags);
    ImGui::PopStyleVar(3);

    // 真ん中は素通しにして、下のゲームを隠さない
    ImGui::DockSpace(ImGui::GetID("##dockspace"), ImVec2{0.0f, 0.0f},
                     ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
}

void DebugUi::DrawTools() {
    if (state_ == nullptr || state_->tools.empty()) {
        return;
    }
    ImGui::SetCurrentContext(state_->context);

    const ImGuiViewport *main = ImGui::GetMainViewport();
    const bool vertical = state_->edge == DebugEdge::Left || state_->edge == DebugEdge::Right;
    state_->bounds =
        StripOf(*main, state_->edge, state_->iconSize, state_->tools.size());

    const ImVec2 position{static_cast<float>(state_->bounds.x),
                          static_cast<float>(state_->bounds.y)};
    const ImVec2 size{static_cast<float>(state_->bounds.width),
                      static_cast<float>(state_->bounds.height)};

    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(main->ID);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    if (ImGui::Begin("##tools", nullptr, flags)) {
        const ImVec2 square{state_->iconSize, state_->iconSize};
        for (std::size_t i = 0; i < state_->tools.size(); ++i) {
            State::Tool &tool = state_->tools[i];
            ImGui::PushID(static_cast<int>(i));

            bool hit = false;
            if (tool.icon != nullptr) {
                hit = ImGui::ImageButton("##icon",
                                         static_cast<ImTextureID>(
                                             reinterpret_cast<std::intptr_t>(tool.icon)),
                                         square);
            } else {
                // 差し替えるまでの仮の絵
                char label[8] = {};
                std::snprintf(label, sizeof(label), "%zu", i + 1);
                hit = ImGui::Button(label, square);
            }

            if (hit) {
                tool.open = !tool.open;
            }
            if (ImGui::IsItemHovered() && !tool.name.empty()) {
                ImGui::SetTooltip("%s", tool.name.c_str());
            }
            if (!vertical && i + 1 < state_->tools.size()) {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
    }
    ImGui::End();
}

#else

struct DebugUi::State {};

Expected<DebugUi, Error> DebugUi::Create(SDL_Window *, SDL_Renderer *) { return DebugUi{}; }

bool DebugUi::Handle(const SDL_Event &) { return false; }
void DebugUi::NewFrame() {}
void DebugUi::Render(SDL_Renderer *) {}
bool DebugUi::WantsMouse() const { return false; }
bool DebugUi::WantsKeyboard() const { return false; }
void DebugUi::SetDocking(bool) {}
bool DebugUi::Docking() const { return false; }
void DebugUi::SetViewports(bool) {}
bool DebugUi::Viewports() const { return false; }
std::size_t DebugUi::WindowCount() const { return 0; }
std::size_t DebugUi::DrawCount() const { return 0; }
std::size_t DebugUi::AddTool(std::string) { return 0; }
void DebugUi::SetIcon(std::size_t, SDL_Texture *) {}
bool DebugUi::ToolOpen(std::size_t) const { return false; }
void DebugUi::SetToolOpen(std::size_t, bool) {}
std::size_t DebugUi::ToolCount() const { return 0; }
void DebugUi::SetEdge(DebugEdge) {}
DebugEdge DebugUi::Edge() const { return DebugEdge::Left; }
void DebugUi::SetIconSize(double) {}
double DebugUi::IconSize() const { return 0.0; }
void DebugUi::DrawTools() {}
DebugBounds DebugUi::ToolBounds() const { return {}; }
void DebugUi::SetDock(bool) {}
bool DebugUi::Dock() const { return false; }
void DebugUi::DrawDock() {}
DebugBounds DebugUi::DockBounds() const { return {}; }

#endif

DebugUi::DebugUi(DebugUi &&other) noexcept : state_(std::move(other.state_)) {}

DebugUi &DebugUi::operator=(DebugUi &&other) noexcept {
    if (this != &other) {
        state_ = std::move(other.state_);
    }
    return *this;
}

DebugUi::~DebugUi() = default;

} // namespace TellerEngine::Base::Platform
