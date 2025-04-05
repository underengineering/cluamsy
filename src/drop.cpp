#include <algorithm>
#include <imgui.h>

#include "common.hpp"
#include "drop.hpp"
#include "packet.hpp"

bool DropModule::draw() {
    bool dirty = false;

    ImGui::PushID(m_short_name);
    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_Text, {m_indicator, 0.f, 0.f, 1.f});
    ImGui::Bullet();
    ImGui::PopStyleColor();

    if (m_indicator > 0.f) {
        m_indicator -= ImGui::GetIO().DeltaTime * 10.f;
        dirty = true;
    }

    ImGui::SameLine();

    ImGui::Text("%s", m_display_name);

    ImGui::SameLine();

    dirty |= ImGui::Checkbox("Enable", &m_enabled);

    ImGui::SameLine();

    dirty |= ImGui::Checkbox("Inbound", &m_inbound);

    ImGui::SameLine();

    dirty |= ImGui::Checkbox("Outbound", &m_outbound);

    ImGui::SameLine();

    ImGui::SetNextItemWidth(8.f * ImGui::GetFontSize());
    if (ImGui::InputFloat("Chance", &m_chance)) {
        m_chance = std::clamp(m_chance, 0.f, 100.f);
        dirty = true;
    }

    ImGui::EndGroup();
    ImGui::PopID();

    return dirty;
}

void DropModule::enable() { LOG("Enabling"); }
void DropModule::disable() {
    LOG("Disabling");

    m_indicator = 0.f;
}

void DropModule::apply_config(const toml::table& config) {
    Module::apply_config(config);

    m_inbound = config["inbound"].value_or(true);
    m_outbound = config["outbound"].value_or(true);

    m_chance = std::clamp(config["chance"].value_or(100.f), 0.f, 100.f);
}

DropModule::Result DropModule::process() {
    int dropped = 0;
    for (auto it = g_packets.begin(); it != g_packets.end();) {
        auto& packet = *it;
        if (check_direction(packet.addr.Outbound, m_inbound, m_outbound) &&
            check_chance(m_chance)) {
            LOG("Dropped with chance %.1f%%, direction %s", m_chance,
                packet.addr.Outbound ? "OUTBOUND" : "INBOUND");
            it = g_packets.erase(it);
            ++dropped;
        } else {
            ++it;
        }
    }

    if (dropped > 0) {
        m_indicator = 1.f;
        return {.dirty = true};
    }

    return {};
}
