/***************************************************************************
    SDL Based Input Handling.

    Populates keys array with user input.
    If porting to a non-SDL platform, you would need to replace this class.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <iostream>
#include <cstring>
#include <cstdlib> // abs
#include "sdl2/input.hpp"

Input input;

Input::Input(void)
{
}

Input::~Input(void)
{
}

void Input::init(int pad_id, int* key_config, int* pad_config, int analog, int* axis, int* analog_settings)
{
    this->pad_id      = pad_id;
    this->key_config  = key_config;
    this->pad_config  = pad_config;
    this->analog      = analog;
    this->axis        = axis;
    this->wheel_zone  = analog_settings[0];
    this->wheel_dead  = analog_settings[1];

    open_joy();
}

void Input::open_joy()
{
    gamepad = SDL_NumJoysticks() > pad_id;

    if (gamepad)
    {
        bool isController = SDL_IsGameController(pad_id);

        // If this is a recognized Game Controller, let's pull some useful default information
        if (controller == NULL && isController)
        {
            std::cout << "open controller\n";
            controller = SDL_GameControllerOpen(pad_id);

            bind_axis(controller, SDL_CONTROLLER_AXIS_LEFTX, 0);                // Analog: Default Steering Axis
            bind_axis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 1);         // Analog: Default Accelerate Axis
            bind_axis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 2);          // Analog: Default Brake Axis
            bind_button(controller, SDL_CONTROLLER_BUTTON_A, 0);                // Digital Controls. Map 'A' to Accelerate
            bind_button(controller, SDL_CONTROLLER_BUTTON_B, 1);                // Digital Controls. Map 'B' to Brake
            bind_button(controller, SDL_CONTROLLER_BUTTON_X, 2);                // Digital Controls. Map 'X' to Gear
            bind_button(controller, SDL_CONTROLLER_BUTTON_START, 4);            // Digital Controls. Map 'START'
            bind_button(controller, SDL_CONTROLLER_BUTTON_Y, 5);                // Digital Controls. Map 'Y' to Coin
            bind_button(controller, SDL_CONTROLLER_BUTTON_BACK, 6);             // Digital Controls. Map 'MENU' to BACK
            bind_button(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 7);     // Digital Controls. Map 'VIEW' to Left Shoulder Button
            bind_button(controller, SDL_CONTROLLER_BUTTON_DPAD_UP, 8);          // Digital Controls. Map D-Pad
            bind_button(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN, 9);
            bind_button(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT, 10);
            bind_button(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 11);
        }
        
        if (stick == NULL && !isController)
        {
            std::cout << "open joy\n";
            stick = SDL_JoystickOpen(pad_id);
        }
    }

    reset_axis_config();
    wheel = a_wheel = CENTRE;
}

void Input::bind_axis(SDL_GameController* controller, SDL_GameControllerAxis ax, int offset)
{
    if (axis[offset] == -1)
        axis[offset] = ax;
}

void Input::bind_button(SDL_GameController* controller, SDL_GameControllerButton button, int offset)
{
    if (pad_config[offset] == -1)
        pad_config[offset] = button;
}

void Input::close_joy()
{
    if (controller != NULL)
    {
        std::cout << "close controller\n";
        SDL_GameControllerClose(controller);
        controller = NULL;
    }

    if (stick != NULL)
    {
        std::cout << "close joy\n";
        SDL_JoystickClose(stick);
        stick = NULL;
    }

    gamepad = false;
}

// Detect whether a key press change has occurred
bool Input::has_pressed(presses p)
{
    return keys[p] && !keys_old[p];
}

// Detect whether key is still pressed
bool Input::is_pressed(presses p)
{
    return keys[p];
}

// Detect whether pressed and clear the press
bool Input::is_pressed_clear(presses p)
{
    bool pressed = keys[p];
    keys[p] = false;
    return pressed;
}

// Denote that a frame has been done by copying key presses into previous array
void Input::frame_done()
{
    memcpy(&keys_old, &keys, sizeof(keys));
}

void Input::handle_key_down(SDL_Keysym* keysym)
{
    key_press = keysym->sym;
    handle_key(key_press, true);
}

void Input::handle_key_up(SDL_Keysym* keysym)
{
    handle_key(keysym->sym, false);
}
void Input::handle_key(const int key, const bool is_pressed)
{
    // Redefinable Key Input
    if (key == key_config[0])  keys[UP] = is_pressed;
    if (key == key_config[1])  keys[DOWN] = is_pressed;
    if (key == key_config[2])  keys[LEFT] = is_pressed;
    if (key == key_config[3])  keys[RIGHT] = is_pressed;
    if (key == key_config[4])  keys[ACCEL] = is_pressed;
    if (key == key_config[5])  keys[BRAKE] = is_pressed;
    if (key == key_config[6])  keys[GEAR1] = is_pressed;
    if (key == key_config[7])  keys[GEAR2] = is_pressed;
    if (key == key_config[8])  keys[START] = is_pressed;
    if (key == key_config[9])  keys[COIN] = is_pressed;
    if (key == key_config[10]) keys[MENU] = is_pressed;
    if (key == key_config[11]) keys[VIEWPOINT] = is_pressed;

    // Function keys are not redefinable
    switch (key)
    {
        case SDLK_F1:
            keys[PAUSE] = is_pressed;
            break;

        case SDLK_F2:
            keys[STEP] = is_pressed;
            break;

        case SDLK_F3:
            keys[TIMER] = is_pressed;
            break;

        case SDLK_F5:
            keys[MENU] = is_pressed;
            break;
    }
}

void Input::handle_controller_axis(SDL_ControllerAxisEvent* evt)
{
    if (controller == NULL) return;

    handle_axis(evt->axis, evt->value);
}

void Input::handle_joy_axis(SDL_JoyAxisEvent* evt)
{
    if (stick == NULL) return;

    handle_axis(evt->axis, evt->value);
}

void Input::handle_axis(const uint8_t evtAxis, const int16_t evtValue)
{
    // Analog Controls
    if (analog)
    {
        int16_t value = evtValue;
        store_last_axis(evtAxis, value);

        // Steering
        // OutRun requires values between 0x48 and 0xb8.
        if (evtAxis == axis[0])
        {
            wheel = ((value + 0x8000) / 0x100); // Back up this value for cab diagnostics only

            int percentage_adjust = ((wheel_zone) << 8) / 100;         
            int adjusted = value + ((value * percentage_adjust) >> 8);
            
            // Make 0 hard left, and 0x80 centre value.
            adjusted = ((adjusted + (1 << 15)) >> 9);
            adjusted += 0x40; // Centre

            if (adjusted < 0x40)
                adjusted = 0x40;
            else if (adjusted > 0xC0)
                adjusted = 0xC0;

            // Remove Dead Zone
            if (wheel_dead)
            {
                if (std::abs(CENTRE - adjusted) <= wheel_dead)
                    adjusted = CENTRE;
            }

            //std::cout << "wheel zone : " << wheel_zone << " : " << std::hex << " : " << (int) adjusted << std::endl;
            a_wheel = adjusted;
        }
        // Accelerator [Single Axis] : Scale input to be in the range of 0 to 0xFF (rather than -32768 to 32768)
        else if (evtAxis == axis[1])
            a_accel = ((value + 0x8000) / 0x100);

        // Brake [Single Axis]       : Scale input to be in the range of 0 to 0xFF (rather than -32768 to 32768)
        else if (evtAxis == axis[2])
            a_brake = ((value + 0x8000) / 0x100);
    }
}

// ------------------------------------------------------------------------------------------------
// Store the last analog axis to be pressed and depressed beyond the cap value for config purposes
// ------------------------------------------------------------------------------------------------
void Input::store_last_axis(const uint8_t axis, const int16_t value)
{
    const static int CAP = 10000;

    if (std::abs(value) > CAP)
        axis_last = axis;

    if (axis == axis_last)
    {
        if (value > CAP && axis_counter == 0)   axis_counter = 1;       // Increment beyond cap
        if (value < -CAP && axis_counter == 1)  axis_counter = 2;       // Decrement below cap
        if (axis_counter == 2)                  axis_config = axis;     // Store the axis
    }
}

int Input::get_axis_config()
{
    if (axis_counter == 2)
    {
        //std::cout << "axis: " << axis_config << " counter: " << axis_counter << std::endl;
        int value = axis_config;
        reset_axis_config();
        return value;
    }
    return -1;
}

void Input::reset_axis_config()
{
    axis_config = -1;
    axis_last = -1;
    axis_counter = 0;
}

void Input::handle_controller_down(SDL_ControllerButtonEvent* evt)
{
    if (controller == NULL) return;

    // Latch joystick button presses for redefines
    joy_button = evt->button;
    handle_joy(evt->button, true);
}

void Input::handle_controller_up(SDL_ControllerButtonEvent* evt)
{
    if (controller == NULL) return;

    handle_joy(evt->button, false);
}

void Input::handle_joy_down(SDL_JoyButtonEvent* evt)
{
    if (stick == NULL) return;

    // Latch joystick button presses for redefines
    joy_button = evt->button;
    handle_joy(evt->button, true);
}

void Input::handle_joy_up(SDL_JoyButtonEvent* evt)
{
    if (stick == NULL) return;

    handle_joy(evt->button, false);
}

void Input::handle_joy(const uint8_t button, const bool is_pressed)
{	
    if (button == pad_config[0])   keys[ACCEL]     = is_pressed;
    if (button == pad_config[1])   keys[BRAKE]     = is_pressed;
    if (button == pad_config[2])   keys[GEAR1]     = is_pressed;
    if (button == pad_config[3])   keys[GEAR2]     = is_pressed;
    if (button == pad_config[4])   keys[START]     = is_pressed;
    if (button == pad_config[5])   keys[COIN]      = is_pressed;
    if (button == pad_config[6])   keys[MENU]      = is_pressed;
    if (button == pad_config[7])   keys[VIEWPOINT] = is_pressed;
    if (button == pad_config[8])   keys[UP]        = is_pressed;
    if (button == pad_config[9])   keys[DOWN]      = is_pressed;
    if (button == pad_config[10])  keys[LEFT]      = is_pressed;
    if (button == pad_config[11])  keys[RIGHT]     = is_pressed;
}

void Input::handle_joy_hat(SDL_JoyHatEvent* evt)
{
    if (stick == NULL) return;

    keys[UP] = evt->value == SDL_HAT_UP;
    keys[DOWN] = evt->value == SDL_HAT_DOWN;
    keys[LEFT] = evt->value == SDL_HAT_LEFT;
    keys[RIGHT] = evt->value == SDL_HAT_RIGHT;
}
