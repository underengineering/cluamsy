#include <algorithm>
#include <imgui.h>

#include "common.hpp"
#include "duplicate.hpp"
#include "packet.hpp"

bool DuplicateModule::draw() {
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

    ImGui::SameLine();

    ImGui::SetNextItemWidth(6.f * ImGui::GetFontSize());
    if (ImGui::InputInt("Count", &m_count)) {
        m_count = std::clamp(m_count, 1, 100);
        dirty = true;
    }

    ImGui::EndGroup();
    ImGui::PopID();

    return dirty;
}

void DuplicateModule::enable() { LOG("Enabling"); }
void DuplicateModule::disable() {
    LOG("Disabling");

    m_indicator = 0.f;
}

void DuplicateModule::apply_config(const toml::table& config) {
    Module::apply_config(config);

    m_inbound = config["inbound"].value_or(true);
    m_outbound = config["outbound"].value_or(true);

    m_chance = std::clamp(config["chance"].value_or(100.f), 0.f, 100.f);
    m_count = std::max(config["count"].value_or(100), 0);
}

DuplicateModule::Result DuplicateModule::process() {
    const auto total_packets = g_packets.size();
    auto duplicated = 0;
    for (auto it = g_packets.cbegin(); it != g_packets.cend();) {
        const auto& packet = *it;
        if (check_direction(packet.addr.Outbound, m_inbound, m_outbound) &&
            check_chance(m_chance)) {
            LOG("Duplicated with chance %.1f%%, direction %s", m_chance,
                packet.addr.Outbound ? "OUTBOUND" : "INBOUND");
            for (auto i = 0; i < m_count; i++)
                it = g_packets.insert(it, packet);
            std::advance(it, m_count);
            ++duplicated;
        } else {
            ++it;
        }
    }

    const auto indicator =
        static_cast<float>(duplicated) / static_cast<float>(total_packets);
    if (!almost_equal(indicator, m_indicator)) {
        m_indicator = indicator;
        return {.dirty = true};
    }

    return {};
}
