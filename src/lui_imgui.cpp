// Dear ImGui text fields for lui.  Built as C++ and called from lui_host.c
// through a C interface.  Each field is a borderless ImGui window pinned to
// its rectangle; selection, clipboard, cursor movement and undo are ImGui's.
#include "imgui.h"
#include <cstring>
#include <cstdio>

extern "C" {

void lui_imgui_setup(void);     // rlImGuiSetup + no imgui.ini
void lui_imgui_begin(void);     // rlImGuiBegin
void lui_imgui_end(void);       // rlImGuiEnd

// Draws one field.  `buf` holds the current text on entry and the edited text
// on exit (capacity `cap`).  Returns 1 if Enter was pressed, 2 if the text
// changed, 0 otherwise (bit flags, so both can be set).
int lui_imgui_field(const char *id, float x, float y, float w, float h,
                    char *buf, int cap, const char *hint, float fontSize) {
    char win[96], label[96];
    std::snprintf(win, sizeof win, "lui_field_%s", id);
    std::snprintf(label, sizeof label, "##%s", id);

    float savedScale = ImGui::GetStyle().FontScaleMain;
    ImGui::GetStyle().FontScaleMain = fontSize / 13.0f;

    char before[4096];
    std::snprintf(before, sizeof before, "%s", buf);

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, (h - fontSize) / 2 - 3));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::Begin(win, nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
    ImGui::SetNextItemWidth(w - 12);
    bool entered = ImGui::InputTextWithHint(label, hint, buf, cap,
                                            ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::End();
    ImGui::GetStyle().FontScaleMain = savedScale;
    ImGui::PopStyleVar(3);

    int out = entered ? 1 : 0;
    if (std::strcmp(before, buf) != 0) out |= 2;
    return out;
}

}  // extern "C"

// Setup, frame and teardown live here so lui_host.c needs no ImGui headers.
#include "rlImGui.h"
extern "C" {
void lui_imgui_setup(void) {
    rlImGuiSetup(true);
    ImGui::GetIO().IniFilename = nullptr;   // no imgui.ini next to the app
}
void lui_imgui_begin(void) { rlImGuiBegin(); }
void lui_imgui_end(void) { rlImGuiEnd(); }
}
