#include <algorithm>
#include <cassert>
#include <chrono>
#include <imgui.h>
#include <optional>

#include "common.hpp"
#include "lag.hpp"
#include "packet.hpp"

bool LagModule::draw() {
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

    int lag_time = static_cast<int>(m_lag_time.count());
    ImGui::SetNextItemWidth(8.f * ImGui::GetFontSize());
    if (ImGui::InputInt("Delay", &lag_time)) {
        lag_time = std::max(lag_time, 0);
        m_lag_time = std::chrono::milliseconds(lag_time);
        dirty = true;
    }

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

void LagModule::enable() {
    LOG("Enabling");
    assert(m_lagged_packets.empty());
}

void LagModule::disable() {
    LOG("Disabling, flushing %zu packets", m_lagged_packets.size());

    // Send all lagged packets
    g_packets.splice(g_packets.cend(), m_lagged_packets);

    m_indicator = 0.f;
}

void LagModule::apply_config(const toml::table& config) {
    Module::apply_config(config);

    m_inbound = config["inbound"].value_or(true);
    m_outbound = config["outbound"].value_or(true);

    m_chance = std::clamp(config["chance"].value_or(100.f), 0.f, 100.f);
    m_lag_time = std::chrono::milliseconds(
        std::max(config["lag_time"].value_or(200), 0));
}

LagModule::Result LagModule::process() {
    const auto current_time_point = std::chrono::steady_clock::now();
    for (auto it = g_packets.cbegin(); it != g_packets.cend();) {
        const auto it_copy = it++;
        const auto& packet = *it_copy;
        if (check_direction(packet.addr.Outbound, m_inbound, m_outbound) &&
            check_chance(m_chance)) {
            m_lagged_packets.splice(m_lagged_packets.cend(), g_packets,
                                    it_copy);
        }
    }

    // Try sending overdue packets
    auto dirty = false;
    if (m_indicator > 0.f) {
        m_indicator = 0.f;
        dirty = true;
    }

    std::optional<std::chrono::milliseconds> schedule_after = std::nullopt;
    for (auto it = m_lagged_packets.cbegin(); it != m_lagged_packets.cend();) {
        const auto it_copy = it++;
        const auto& packet = *it_copy;

        if (current_time_point > packet.captured_at + m_lag_time) {
            g_packets.splice(g_packets.cend(), m_lagged_packets, it_copy);
            m_indicator = 1.f;
            dirty = true;
        } else {
            const auto will_send_after_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    packet.captured_at + m_lag_time - current_time_point);

            if (schedule_after) {
                schedule_after = std::chrono::milliseconds(std::min(
                    schedule_after->count(), will_send_after_ms.count()));
            } else {
                schedule_after = will_send_after_ms;
            }
        }
    }

    // If buffer is full just flush things out
    if (m_lagged_packets.size() > MAX_PACKETS) {
        LOG("Buffer full, flushing");
        g_packets.splice(g_packets.cend(), m_lagged_packets);
    }

    return {.schedule_after = schedule_after, .dirty = dirty};
}
