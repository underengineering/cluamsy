local ffi = require("ffi")
ffi.cdef[[
typedef struct {
  uint16_t wButtons;
  uint8_t  bLeftTrigger;
  uint8_t  bRightTrigger;
  int16_t  sThumbLX;
  int16_t  sThumbLY;
  int16_t  sThumbRX;
  int16_t  sThumbRY;
} XINPUT_GAMEPAD;

typedef struct {
  uint32_t       dwPacketNumber;
  XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE;

uint32_t XInputGetState(
  uint32_t      dwUserIndex,
  XINPUT_STATE* pState
);
]]

local xinput = ffi.load("xinput1_4")

local XINPUT_GAMEPAD_DPAD_UP = 0x0001
local XINPUT_GAMEPAD_DPAD_DOWN = 0x0002
local XINPUT_GAMEPAD_DPAD_LEFT = 0x0004
local XINPUT_GAMEPAD_DPAD_RIGHT = 0x0008
local XINPUT_GAMEPAD_START = 0x0010
local XINPUT_GAMEPAD_BACK = 0x0020
local XINPUT_GAMEPAD_LEFT_THUMB = 0x0040
local XINPUT_GAMEPAD_RIGHT_THUMB = 0x0080
local XINPUT_GAMEPAD_LEFT_SHOULDER = 0x0100
local XINPUT_GAMEPAD_RIGHT_SHOULDER = 0x0200
local XINPUT_GAMEPAD_A = 0x1000
local XINPUT_GAMEPAD_B = 0x2000
local XINPUT_GAMEPAD_X = 0x4000
local XINPUT_GAMEPAD_Y = 0x8000

local state = ffi.new("XINPUT_STATE")

local last_packet_number = -1
local next_throttle = 0
local throttling = false

cluamsy.Timer.create(40, function()
    local current_time = os.clock()
    if current_time < next_throttle then return end

    xinput.XInputGetState(0, state)

    local current_packet_number = state.dwPacketNumber
    if current_packet_number > last_packet_number then
        last_packet_number = current_packet_number

        local in_attack = state.Gamepad.bLeftTrigger > 200
        local heavy_attack = state.Gamepad.bRightTrigger > 200

        if not throttling and in_attack and heavy_attack then
            cluamsy.modules.Throttle:enabled(true)
            throttling = true

            cluamsy.Timer.create(400, function()
                cluamsy.modules.Throttle:enabled(false)
                throttling = false
                return 0
            end)
        end
    end
end)
