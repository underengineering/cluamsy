// dropping packet module
#include "common.h"
#include "iup.h"
#include <Windows.h>
#include <stdbool.h>
#include <stdlib.h>
#define NAME "drop"

static Ihandle *inboundCheckbox, *outboundCheckbox, *chanceInput;

static volatile short dropEnabled = 0, dropInbound = 1, dropOutbound = 1,
                      chance = 1000; // [0-10000]

static Ihandle *dropSetupUI() {
    Ihandle *dropControlsBox =
        IupHbox(inboundCheckbox = IupToggle("Inbound", NULL),
                outboundCheckbox = IupToggle("Outbound", NULL),
                IupLabel("Chance(%):"), chanceInput = IupText(NULL), NULL);

    IupSetAttribute(chanceInput, "VISIBLECOLUMNS", "4");
    IupSetAttribute(chanceInput, "VALUE", "10.0");
    IupSetCallback(chanceInput, "VALUECHANGED_CB", uiSyncChance);
    IupSetAttribute(chanceInput, SYNCED_VALUE, (char *)&chance);
    IupSetCallback(inboundCheckbox, "ACTION", (Icallback)uiSyncToggle);
    IupSetAttribute(inboundCheckbox, SYNCED_VALUE, (char *)&dropInbound);
    IupSetCallback(outboundCheckbox, "ACTION", (Icallback)uiSyncToggle);
    IupSetAttribute(outboundCheckbox, SYNCED_VALUE, (char *)&dropOutbound);

    // enable by default to avoid confusing
    IupSetAttribute(inboundCheckbox, "VALUE", "ON");
    IupSetAttribute(outboundCheckbox, "VALUE", "ON");

    if (parameterized) {
        setFromParameter(inboundCheckbox, "VALUE", NAME "-inbound");
        setFromParameter(outboundCheckbox, "VALUE", NAME "-outbound");
        setFromParameter(chanceInput, "VALUE", NAME "-chance");
    }

    return dropControlsBox;
}

static void dropStartUp() { LOG("drop enabled"); }

static void dropCloseDown(PacketNode *head, PacketNode *tail) {
    UNREFERENCED_PARAMETER(head);
    UNREFERENCED_PARAMETER(tail);
    LOG("drop disabled");
}

static short dropProcess(PacketNode *head, PacketNode *tail) {
    int dropped = 0;
    while (head->next != tail) {
        PacketNode *pac = head->next;
        // chance in range of [0, 10000]
        if (checkDirection(pac->addr.Outbound, dropInbound, dropOutbound) &&
            calcChance(chance)) {
            LOG("dropped with chance %.1f%%, direction %s", chance / 100.0,
                pac->addr.Outbound ? "OUTBOUND" : "INBOUND");
            freeNode(popNode(pac));
            ++dropped;
        } else {
            head = head->next;
        }
    }

    return dropped > 0;
}

static int drop_enable(lua_State *L) {
    int type = lua_gettop(L) > 0 ? lua_type(L, -1) : LUA_TNIL;
    switch (type) {
    case LUA_TBOOLEAN:
        bool enabled = lua_toboolean(L, -1);
        dropEnabled = enabled;
        break;
    case LUA_TNIL:
        lua_pushboolean(L, dropEnabled);
        return 1;
    default:
        char message[256];
        int message_length = snprintf(
            message, sizeof(message),
            "Invalid argument #1 to drop_enable: '%s'", lua_typename(L, type));
        lua_pushlstring(L, message, message_length);
        lua_error(L);
        break;
    }

    return 0;
}

static void push_lua_functions(lua_State *L) {
    lua_pushcfunction(L, drop_enable);
    lua_setfield(L, -2, "enable");
}

Module dropModule = {"Drop", NAME, (short *)&dropEnabled, dropSetupUI,
                     dropStartUp, dropCloseDown, dropProcess,
                     // runtime fields
                     0, 0, NULL, push_lua_functions};
