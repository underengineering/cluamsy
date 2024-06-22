// duplicate packet module
#include "common.h"
#include "iup.h"
#include <stdbool.h>
#include <stdlib.h>
#define NAME "duplicate"
#define COPIES_MIN "2"
#define COPIES_MAX "50"
#define COPIES_COUNT 2

static Ihandle *inboundCheckbox, *outboundCheckbox, *chanceInput, *countInput;

static volatile short dupEnabled = 0, dupInbound = 1, dupOutbound = 1,
                      chance = 1000, // [0-10000]
    count = COPIES_COUNT;            // how many copies to duplicate

static Ihandle *dupSetupUI() {
    Ihandle *dupControlsBox =
        IupHbox(IupLabel("Count:"), countInput = IupText(NULL),
                inboundCheckbox = IupToggle("Inbound", NULL),
                outboundCheckbox = IupToggle("Outbound", NULL),
                IupLabel("Chance(%):"), chanceInput = IupText(NULL), NULL);

    IupSetAttribute(chanceInput, "VISIBLECOLUMNS", "4");
    IupSetAttribute(chanceInput, "VALUE", "10.0");
    IupSetCallback(chanceInput, "VALUECHANGED_CB", uiSyncChance);
    IupSetAttribute(chanceInput, SYNCED_VALUE, (char *)&chance);
    IupSetCallback(inboundCheckbox, "ACTION", (Icallback)uiSyncToggle);
    IupSetAttribute(inboundCheckbox, SYNCED_VALUE, (char *)&dupInbound);
    IupSetCallback(outboundCheckbox, "ACTION", (Icallback)uiSyncToggle);
    IupSetAttribute(outboundCheckbox, SYNCED_VALUE, (char *)&dupOutbound);
    // sync count
    IupSetAttribute(countInput, "VISIBLECOLUMNS", "3");
    IupSetAttribute(countInput, "VALUE", STR(COPIES_COUNT));
    IupSetCallback(countInput, "VALUECHANGED_CB", (Icallback)uiSyncInteger);
    IupSetAttribute(countInput, SYNCED_VALUE, (char *)&count);
    IupSetAttribute(countInput, INTEGER_MAX, COPIES_MAX);
    IupSetAttribute(countInput, INTEGER_MIN, COPIES_MIN);

    // enable by default to avoid confusing
    IupSetAttribute(inboundCheckbox, "VALUE", "ON");
    IupSetAttribute(outboundCheckbox, "VALUE", "ON");

    if (parameterized) {
        setFromParameter(inboundCheckbox, "VALUE", NAME "-inbound");
        setFromParameter(outboundCheckbox, "VALUE", NAME "-outbound");
        setFromParameter(chanceInput, "VALUE", NAME "-chance");
        setFromParameter(countInput, "VALUE", NAME "-count");
    }

    return dupControlsBox;
}

static void dupStartup() { LOG("dup enabled"); }

static void dupCloseDown(PacketNode *head, PacketNode *tail) {
    UNREFERENCED_PARAMETER(head);
    UNREFERENCED_PARAMETER(tail);
    LOG("dup disabled");
}

static short dupProcess(PacketNode *head, PacketNode *tail) {
    short duped = FALSE;
    PacketNode *pac = head->next;
    while (pac != tail) {
        if (checkDirection(pac->addr.Outbound, dupInbound, dupOutbound) &&
            calcChance(chance)) {
            short copies = count - 1;
            LOG("duplicating w/ chance %.1f%%, cloned additionally %d packets",
                chance / 100.0, copies);
            while (copies--) {
                PacketNode *copy =
                    createNode(pac->packet, pac->packetLen, &(pac->addr));
                insertBefore(
                    copy, pac); // must insertBefore or next packet is still pac
            }
            duped = TRUE;
        }
        pac = pac->next;
    }
    return duped;
}

static int duplicate_enable(lua_State *L) {
    int type = lua_gettop(L) > 0 ? lua_type(L, -1) : LUA_TNIL;
    switch (type) {
    case LUA_TBOOLEAN:
        bool enabled = lua_toboolean(L, -1);
        dupEnabled = enabled;
        break;
    case LUA_TNIL:
        lua_pushboolean(L, dupEnabled);
        return 1;
    default:
        char message[256];
        int message_length =
            snprintf(message, sizeof(message),
                     "Invalid argument #1 to duplicate_enable: '%s'",
                     lua_typename(L, type));
        lua_pushlstring(L, message, message_length);
        lua_error(L);
        break;
    }

    return 0;
}

static void push_lua_functions(lua_State *L) {
    lua_pushcfunction(L, duplicate_enable);
    lua_setfield(L, -2, "enable");
}

Module dupModule = {"Duplicate", NAME, (short *)&dupEnabled, dupSetupUI,
                    dupStartup, dupCloseDown, dupProcess,
                    // runtime fields
                    0, 0, NULL, push_lua_functions};
