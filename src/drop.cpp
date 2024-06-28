#include <algorithm>
#include <imgui.h>

#include "common.hpp"
#include "drop.hpp"
#include "packet.hpp"

bool DropModule::draw() {
    bool dirty = false;

    ImGui::BeginGroup();

    ImGui::Text(m_indicator > 0.f ? "[*]" : "[ ]");
    if (m_indicator > 0.f) {
        m_indicator -= ImGui::GetIO().DeltaTime;
        if (m_indicator <= 0.f)
            dirty = true;
    }

    ImGui::SameLine();

    ImGui::Text("%s", m_display_name);

    ImGui::SameLine();

    dirty |= ImGui::Checkbox("Enable", &m_enabled);

    ImGui::SameLine();

    dirty |= ImGui::Checkbox("Inbound", &m_drop_inbound);

    ImGui::SameLine();

    dirty |= ImGui::Checkbox("Outbound", &m_drop_outbound);

    ImGui::SameLine();

    if (ImGui::InputFloat("Chance", &m_chance)) {
        m_chance = std::clamp(m_chance, 0.f, 100.f);
        dirty = true;
    }

    ImGui::EndGroup();

    return dirty;
}

void DropModule::enable() {}
void DropModule::process() {
    int dropped = 0;
    for (auto it = g_packets.begin(); it != g_packets.end();) {
        auto& packet = *it;
        if (checkDirection(packet.addr.Outbound, m_drop_inbound,
                           m_drop_outbound) &&
            calcChance(m_chance)) {
            LOG("Dropped with chance %.1f%%, direction %s", m_chance,
                packet.addr.Outbound ? "OUTBOUND" : "INBOUND");
            it = g_packets.erase(it);
            ++dropped;
        } else {
            ++it;
        }
    }

    if (dropped > 0) {
        m_indicator = 0.01f;
        m_dirty = true;
    }
}
void DropModule::disable() {}
