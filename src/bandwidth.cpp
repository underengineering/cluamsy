#include <algorithm>
#include <imgui.h>

#include "bandwidth.hpp"
#include "common.hpp"
#include "packet.hpp"

bool BandwidthModule::draw() {
    bool dirty = false;
    ImGui::PushID(m_short_name);
    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_Text, {m_indicator, 0.f, 0.f, 1.f});
    ImGui::Bullet();
    ImGui::PopStyleColor();

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
    if (ImGui::InputInt("Limit", &m_limit)) {
        m_limit = std::max(0, m_limit);
        dirty = true;
    }

    ImGui::EndGroup();
    ImGui::PopID();

    return dirty;
}

void BandwidthModule::enable() { LOG("Enabling"); }
void BandwidthModule::disable() {
    m_indicator = 0.f;
    m_rate_stats.reset();
}

void BandwidthModule::apply_config(const toml::table& config) {
    Module::apply_config(config);

    m_inbound = config["inbound"].value_or(true);
    m_outbound = config["outbound"].value_or(true);

    m_limit = std::max(config["limit"].value_or(10), 0);
}

BandwidthModule::Result BandwidthModule::process() {
    const auto current_time_point = std::chrono::steady_clock::now();

    // allow 0 limit which should drop all
    if (m_limit < 0)
        return {};

    const auto limit = m_limit * 1024;

    const auto total_packets = g_packets.size();
    size_t dropped = 0;

    m_rate_stats.set_max_tokens(limit);
    m_rate_stats.replenish(current_time_point, limit);
    for (auto it = g_packets.begin(); it != g_packets.end();) {
        const auto packet = *it;
        if (check_direction(packet.addr.Outbound, m_inbound, m_outbound)) {
            const auto size = packet.packet.size();
            if (!m_rate_stats.update(current_time_point, size)) {
                LOG("Dropped with bandwidth %dKiB/s, direction %s",
                    (int)m_limit,
                    packet.addr.Outbound ? "OUTBOUND" : "INBOUND");
                it = g_packets.erase(it);
                dropped++;
            } else {
                ++it;
            }
        }
    }

    const auto indicator =
        static_cast<float>(dropped) / static_cast<float>(total_packets);
    if (!almost_equal(indicator, m_indicator)) {
        m_indicator = indicator;
        return {.dirty = true};
    }

    return {};
}