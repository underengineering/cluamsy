// lagging packets
#include "common.h"
#include "iup.h"
#include <stdbool.h>
#define NAME "lag"
#define LAG_MIN "0"
#define LAG_MAX "15000"
#define KEEP_AT_MOST 2000
// send FLUSH_WHEN_FULL packets when buffer is full
#define FLUSH_WHEN_FULL 800
#define LAG_DEFAULT 50

// don't need a chance
static Ihandle *inboundCheckbox, *outboundCheckbox, *timeInput;

static volatile short lagEnabled = 0, lagInbound = 1, lagOutbound = 1,
                      lagTime = LAG_DEFAULT; // default for 50ms

static PacketNode lagHeadNode = {0}, lagTailNode = {0};
static PacketNode *bufHead = &lagHeadNode, *bufTail = &lagTailNode;
static int bufSize = 0;

static INLINE_FUNCTION short isBufEmpty() {
    short ret = bufHead->next == bufTail;
    if (ret)
        assert(bufSize == 0);
    return ret;
}

static Ihandle *lagSetupUI() {
    Ihandle *lagControlsBox =
        IupHbox(inboundCheckbox = IupToggle("Inbound", NULL),
                outboundCheckbox = IupToggle("Outbound", NULL),
                IupLabel("Delay(ms):"), timeInput = IupText(NULL), NULL);

    IupSetAttribute(timeInput, "VISIBLECOLUMNS", "4");
    IupSetAttribute(timeInput, "VALUE", STR(LAG_DEFAULT));
    IupSetCallback(timeInput, "VALUECHANGED_CB", uiSyncInteger);
    IupSetAttribute(timeInput, SYNCED_VALUE, (char *)&lagTime);
    IupSetAttribute(timeInput, INTEGER_MAX, LAG_MAX);
    IupSetAttribute(timeInput, INTEGER_MIN, LAG_MIN);
    IupSetCallback(inboundCheckbox, "ACTION", (Icallback)uiSyncToggle);
    IupSetAttribute(inboundCheckbox, SYNCED_VALUE, (char *)&lagInbound);
    IupSetCallback(outboundCheckbox, "ACTION", (Icallback)uiSyncToggle);
    IupSetAttribute(outboundCheckbox, SYNCED_VALUE, (char *)&lagOutbound);

    // enable by default to avoid confusing
    IupSetAttribute(inboundCheckbox, "VALUE", "ON");
    IupSetAttribute(outboundCheckbox, "VALUE", "ON");

    if (parameterized) {
        setFromParameter(inboundCheckbox, "VALUE", NAME "-inbound");
        setFromParameter(outboundCheckbox, "VALUE", NAME "-outbound");
        setFromParameter(timeInput, "VALUE", NAME "-time");
    }

    return lagControlsBox;
}

static void lagStartUp() {
    if (bufHead->next == NULL && bufTail->next == NULL) {
        bufHead->next = bufTail;
        bufTail->prev = bufHead;
        bufSize = 0;
    } else {
        assert(isBufEmpty());
    }
    startTimePeriod();
}

static void lagCloseDown(PacketNode *head, PacketNode *tail) {
    PacketNode *oldLast = tail->prev;
    UNREFERENCED_PARAMETER(head);
    // flush all buffered packets
    LOG("Closing down lag, flushing %d packets", bufSize);
    while (!isBufEmpty()) {
        insertAfter(popNode(bufTail->prev), oldLast);
        --bufSize;
    }
    endTimePeriod();
}

static short lagProcess(PacketNode *head, PacketNode *tail) {
    DWORD currentTime = timeGetTime();
    PacketNode *pac = tail->prev;
    // pick up all packets and fill in the current time
    while (bufSize < KEEP_AT_MOST && pac != head) {
        if (checkDirection(pac->addr.Outbound, lagInbound, lagOutbound)) {
            insertAfter(popNode(pac), bufHead)->timestamp = timeGetTime();
            ++bufSize;
            pac = tail->prev;
        } else {
            pac = pac->prev;
        }
    }

    // try sending overdue packets from buffer tail
    while (!isBufEmpty()) {
        pac = bufTail->prev;
        if (currentTime > pac->timestamp + lagTime) {
            insertAfter(popNode(bufTail->prev),
                        head); // sending queue is already empty by now
            --bufSize;
            LOG("Send lagged packets.");
        } else {
            LOG("Sent some lagged packets, still have %d in buf", bufSize);
            break;
        }
    }

    // if buffer is full just flush things out
    if (bufSize >= KEEP_AT_MOST) {
        int flushCnt = FLUSH_WHEN_FULL;
        while (flushCnt-- > 0) {
            insertAfter(popNode(bufTail->prev), head);
            --bufSize;
        }
    }

    return bufSize > 0;
}

static int lag_enable(lua_State *L) {
    int type = lua_gettop(L) > 0 ? lua_type(L, -1) : LUA_TNIL;
    switch (type) {
    case LUA_TBOOLEAN:
        bool enabled = lua_toboolean(L, -1);
        lagEnabled = enabled;
        break;
    case LUA_TNIL:
        lua_pushboolean(L, lagEnabled);
        return 1;
    default:
        char message[256];
        int message_length = snprintf(message, sizeof(message),
                                      "Invalid argument #1 to lag_enable: '%s'",
                                      lua_typename(L, type));
        lua_pushlstring(L, message, message_length);
        lua_error(L);
        break;
    }

    return 0;
}

static void push_lua_functions(lua_State *L) {
    lua_pushcfunction(L, lag_enable);
    lua_setfield(L, -2, "enable");
}

Module lagModule = {"Lag", NAME, (short *)&lagEnabled, lagSetupUI, lagStartUp,
                    lagCloseDown, lagProcess,
                    // runtime fields
                    0, 0, NULL, push_lua_functions};
