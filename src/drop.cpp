#include <imgui.h>

#include "common.hpp"
#include "drop.hpp"
#include "packet.hpp"

bool DropModule::draw() {
    bool dirty = false;

    ImGui::BeginGroup();
    ImGui::Text("%s", m_display_name);
    dirty |= ImGui::Checkbox("Enable", &m_enabled);
    ImGui::EndGroup();

    return dirty;
}

void DropModule::enable() {}
bool DropModule::process() {
    int dropped = 0;
    for (auto it = g_packets.begin(); it != g_packets.end();) {
        auto& packet = *it;
        // chance in range of [0, 10000]
        if (checkDirection(packet.addr.Outbound, dropInbound, dropOutbound) &&
            calcChance(chance)) {
            LOG("dropped with chance %.1f%%, direction %s", chance / 100.0,
                packet.addr.Outbound ? "OUTBOUND" : "INBOUND");
            it = g_packets.erase(it);
            ++dropped;
        } else {
            ++it;
        }
    }

    return dropped > 0;
}
void DropModule::disable() {}
