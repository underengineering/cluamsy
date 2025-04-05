#include <algorithm>
#include <bit>
#include <imgui.h>
#include <windivert.h>

#include "common.hpp"
#include "packet.hpp"
#include "tamper.hpp"

bool TamperModule::draw() {
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
    if (ImGui::InputInt("Max bit flips", &m_max_bit_flips)) {
        m_max_bit_flips = std::max(m_max_bit_flips, 1);
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

void TamperModule::enable() { LOG("Enabling"); }

void TamperModule::disable() {
    LOG("Disabling");
    m_indicator = 0.f;
}

void TamperModule::apply_config(const toml::table& config) {
    Module::apply_config(config);

    m_inbound = config["inbound"].value_or(true);
    m_outbound = config["outbound"].value_or(true);

    m_chance = std::clamp(config["chance"].value_or(100.f), 0.f, 100.f);
    m_max_bit_flips = std::max(config["max_bit_flips"].value_or(1), 1);
}

TamperModule::Result TamperModule::process() {
    const auto total_packets = g_packets.size();
    auto tampered = 0;
    for (auto it = g_packets.begin(); it != g_packets.end();) {
        auto& packet = *it;
        if (!check_direction(packet.addr.Outbound, m_inbound, m_outbound) ||
            !check_chance(m_chance))
            continue;

        auto& packet_data = packet.packet;

        char* data = nullptr;
        UINT data_size = 0;
        if (WinDivertHelperParsePacket(
                packet_data.data(), packet_data.size(), nullptr, nullptr,
                nullptr, nullptr, nullptr, nullptr, nullptr,
                std::bit_cast<PVOID*>(&data), &data_size, nullptr, nullptr) &&
            data != nullptr && data_size != 0) {
            for (auto i = 0; i < m_max_bit_flips; i++) {
                const size_t idx = rand() % data_size;
                const uint8_t bit = 1 << (rand() % 8);

                // NOLINTBEGIN(cppcoreguidelines-narrowing-conversions)
                data[idx] ^= bit;
                // NOLINTEND(cppcoreguidelines-narrowing-conversions)
            }

            WinDivertHelperCalcChecksums(packet_data.data(), packet_data.size(),
                                         nullptr, 0);
            tampered++;
        }
    }

    const auto indicator =
        static_cast<float>(tampered) / static_cast<float>(total_packets);
    if (!almost_equal(indicator, m_indicator)) {
        m_indicator = indicator;
        return {.dirty = true};
    }

    return {};
}