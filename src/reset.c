// Reset injection packet module
#include "common.h"
#include "iup.h"
#include <stdbool.h>
#include <stdlib.h>
#define NAME "reset"

static const unsigned int TCP_MIN_SIZE =
    sizeof(WINDIVERT_IPHDR) + sizeof(WINDIVERT_TCPHDR);

static Ihandle *inboundCheckbox, *outboundCheckbox, *chanceInput, *rstButton;

static volatile short resetEnabled = 0, resetInbound = 1, resetOutbound = 1,
                      chance = 0, // [0-10000]
    setNextCount = 0;

static int resetSetRSTNextButtonCb(Ihandle *ih) {
    UNREFERENCED_PARAMETER(ih);

    if (!(*resetModule.enabledFlag)) {
        return IUP_DEFAULT;
    }

    InterlockedIncrement16(&setNextCount);

    return IUP_DEFAULT;
}

static Ihandle *resetSetupUI() {
    Ihandle *dupControlsBox =
        IupHbox(rstButton = IupButton("RST next packet", NULL),
                inboundCheckbox = IupToggle("Inbound", NULL),
                outboundCheckbox = IupToggle("Outbound", NULL),
                IupLabel("Chance(%):"), chanceInput = IupText(NULL), NULL);

    IupSetAttribute(chanceInput, "VISIBLECOLUMNS", "4");
    IupSetAttribute(chanceInput, "VALUE", "0");
    IupSetCallback(chanceInput, "VALUECHANGED_CB", uiSyncChance);
    IupSetAttribute(chanceInput, SYNCED_VALUE, (char *)&chance);
    IupSetCallback(inboundCheckbox, "ACTION", (Icallback)uiSyncToggle);
    IupSetAttribute(inboundCheckbox, SYNCED_VALUE, (char *)&resetInbound);
    IupSetCallback(outboundCheckbox, "ACTION", (Icallback)uiSyncToggle);
    IupSetAttribute(outboundCheckbox, SYNCED_VALUE, (char *)&resetOutbound);
    IupSetCallback(rstButton, "ACTION", resetSetRSTNextButtonCb);
    IupSetAttribute(rstButton, "PADDING", "4x");

    // enable by default to avoid confusing
    IupSetAttribute(inboundCheckbox, "VALUE", "ON");
    IupSetAttribute(outboundCheckbox, "VALUE", "ON");

    if (parameterized) {
        setFromParameter(inboundCheckbox, "VALUE", NAME "-inbound");
        setFromParameter(outboundCheckbox, "VALUE", NAME "-outbound");
        setFromParameter(chanceInput, "VALUE", NAME "-chance");
    }

    return dupControlsBox;
}

static void resetStartup() {
    LOG("reset enabled");
    InterlockedExchange16(&setNextCount, 0);
}

static void resetCloseDown(PacketNode *head, PacketNode *tail) {
    UNREFERENCED_PARAMETER(head);
    UNREFERENCED_PARAMETER(tail);
    LOG("reset disabled");
    InterlockedExchange16(&setNextCount, 0);
}

static short resetProcess(PacketNode *head, PacketNode *tail) {
    short reset = FALSE;
    PacketNode *pac = head->next;
    while (pac != tail) {
        if (checkDirection(pac->addr.Outbound, resetInbound, resetOutbound) &&
            pac->packetLen > TCP_MIN_SIZE &&
            (setNextCount || calcChance(chance))) {
            PWINDIVERT_TCPHDR pTcpHdr;
            WinDivertHelperParsePacket(pac->packet, pac->packetLen, NULL, NULL,
                                       NULL, NULL, NULL, &pTcpHdr, NULL, NULL,
                                       NULL, NULL, NULL);

            if (pTcpHdr != NULL) {
                LOG("injecting reset w/ chance %.1f%%", chance / 100.0);
                pTcpHdr->Rst = 1;
                WinDivertHelperCalcChecksums(pac->packet, pac->packetLen, NULL,
                                             0);

                reset = TRUE;
                if (setNextCount > 0) {
                    InterlockedDecrement16(&setNextCount);
                }
            }
        }

        pac = pac->next;
    }
    return reset;
}

static int reset_enable(lua_State *L) {
    int type = lua_gettop(L) > 0 ? lua_type(L, -1) : LUA_TNIL;
    switch (type) {
    case LUA_TBOOLEAN:
        bool enabled = lua_toboolean(L, -1);
        resetEnabled = enabled;
        break;
    case LUA_TNIL:
        lua_pushboolean(L, resetEnabled);
        return 1;
    default:
        char message[256];
        int message_length = snprintf(
            message, sizeof(message),
            "Invalid argument #1 to reset_enable: '%s'", lua_typename(L, type));
        lua_pushlstring(L, message, message_length);
        lua_error(L);
        break;
    }

    return 0;
}

static void push_lua_functions(lua_State *L) {
    lua_pushcfunction(L, reset_enable);
    lua_setfield(L, -2, "enable");
}

Module resetModule = {"Set TCP RST", NAME, (short *)&resetEnabled, resetSetupUI,
                      resetStartup, resetCloseDown, resetProcess,
                      // runtime fields
                      0, 0, NULL, push_lua_functions};
