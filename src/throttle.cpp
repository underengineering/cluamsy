#include <algorithm>
#include <cassert>
#include <chrono>
#include <imgui.h>

#include "common.hpp"
#include "throttle.hpp"

bool ThrottleModule::draw() {
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

    int timeframe = static_cast<int>(m_timeframe_ms.count());
    ImGui::SetNextItemWidth(8.f * ImGui::GetFontSize());
    if (ImGui::InputInt("Timeframe", &timeframe)) {
        timeframe = std::max(timeframe, 0);
        m_timeframe_ms = std::chrono::milliseconds(timeframe);
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

void ThrottleModule::enable() {
    LOG("Enabling");
    assert(m_throttle_list.empty() && !m_throttling);
}

void ThrottleModule::disable() {
    LOG("Disabling");

    flush();
}

void ThrottleModule::apply_config(const toml::table& config) {
    Module::apply_config(config);

    m_inbound = config["inbound"].value_or(true);
    m_outbound = config["outbound"].value_or(true);

    m_chance = std::clamp(config["chance"].value_or(100.f), 0.f, 100.f);
    m_timeframe_ms = std::chrono::milliseconds(
        std::max(config["timeframe"].value_or(200), 0));
    m_drop_throttled = config["drop_throttled"].value_or(false);
}

void ThrottleModule::flush() {
    LOG("Sending all %zu packets", m_throttle_list.size());
    if (m_drop_throttled)
        m_throttle_list.clear();
    else
        g_packets.splice(g_packets.cend(), m_throttle_list);

    m_throttling = false;
    m_indicator = 0.f;
}

ThrottleModule::Result ThrottleModule::process() {
    auto dirty = false;
    if (!m_throttling && check_chance(m_chance)) {
        LOG("Start new throttling w/ chance %.1f, time frame: %lld", m_chance,
            m_timeframe_ms.count());
        m_throttling = true;
        m_start_point = std::chrono::steady_clock::now();
        m_indicator = 1.f;
        dirty = true;
    }

    if (m_throttling) {
        // Already throttling, keep filling up
        const auto current_time_point = std::chrono::steady_clock::now();
        for (auto it = g_packets.cbegin();
             it != g_packets.cend() && m_throttle_list.size() < MAX_PACKETS;) {
            const auto it_copy = it++;
            const auto& packet = *it_copy;
            if (check_direction(packet.addr.Outbound, m_inbound, m_outbound)) {
                m_throttle_list.splice(m_throttle_list.cend(), g_packets,
                                       it_copy);
            }
        }

        // send all when throttled enough, including in current step
        const auto delta_time = current_time_point - m_start_point;
        if (m_throttle_list.size() >= MAX_PACKETS ||
            delta_time > m_timeframe_ms) {
            flush();
            return {.schedule_after = std::nullopt};
        } else {
            const auto delta_time_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    delta_time);
            return {.schedule_after = m_timeframe_ms - delta_time_ms};
        }
    }

    return {.dirty = dirty};
}
