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
    this->key_config  = key_config;
    this->pad_config  = pad_config;
    this->analog      = analog;
    this->axis        = axis;
    this->wheel_zone  = analog_settings[0];
    this->wheel_dead  = analog_settings[1];

    gamepad = SDL_NumJoysticks() > pad_id;

    if (gamepad)
    {
        stick = SDL_JoystickOpen(pad_id);
    }

    reset_axis_config();
    wheel = a_wheel = CENTRE;
}

void Input::close()
{
    if (gamepad && stick != NULL)
        SDL_JoystickClose(stick);
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
    if (key == key_config[0])
        keys[UP] = is_pressed;

    else if (key == key_config[1])
        keys[DOWN] = is_pressed;

    else if (key == key_config[2])
        keys[LEFT] = is_pressed;

    else if (key == key_config[3])
        keys[RIGHT] = is_pressed;

    if (key == key_config[4])
        keys[ACCEL] = is_pressed;

    if (key == key_config[5])
        keys[BRAKE] = is_pressed;

    if (key == key_config[6])
        keys[GEAR1] = is_pressed;

    if (key == key_config[7])
        keys[GEAR2] = is_pressed;

    if (key == key_config[8])
        keys[START] = is_pressed;

    if (key == key_config[9])
        keys[COIN] = is_pressed;

    if (key == key_config[10])
        keys[MENU] = is_pressed;

    if (key == key_config[11])
        keys[VIEWPOINT] = is_pressed;

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

void Input::handle_joy_axis(SDL_JoyAxisEvent* evt)
{
    int16_t value = evt->value;

    // Digital Controls
    if (!analog)
    {
        // X-Axis
        if (evt->axis == 0)
        {
            // Neural
            if ( (value > -DIGITAL_DEAD ) && (value < DIGITAL_DEAD ) )
            {
                keys[LEFT]  = false;
                keys[RIGHT] = false;
            }
            else if (value < 0)
            {
                keys[LEFT] = true;
            }
            else if (value > 0)
            {
                keys[RIGHT] = true;
            }
        }
        // Y-Axis
        else if (evt->axis == 1)
        {
            // Neural
            if ( (value > -DIGITAL_DEAD ) && (value < DIGITAL_DEAD ) )
            {
                keys[UP]  = false;
                keys[DOWN] = false;
            }
            else if (value < 0)
            {
                keys[UP] = true;
            }
            else if (value > 0)
            {
                keys[DOWN] = true;
            }
        }
    }
    // Analog Controls
    else
    {
        store_last_axis(evt->axis, value);

        // Steering
        // OutRun requires values between 0x48 and 0xb8.
        if (evt->axis == axis[0])
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
        else if (evt->axis == axis[1])
            a_accel = ((value + 0x8000) / 0x100);

        // Brake [Single Axis]       : Scale input to be in the range of 0 to 0xFF (rather than -32768 to 32768)
        else if (evt->axis == axis[2])
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

void Input::handle_joy_down(SDL_JoyButtonEvent* evt)
{
    // Latch joystick button presses for redefines
    joy_button = evt->button;
    handle_joy(evt->button, true);
}

void Input::handle_joy_up(SDL_JoyButtonEvent* evt)
{
    handle_joy(evt->button, false);
}

void Input::handle_joy(const uint8_t button, const bool is_pressed)
{	
    if (button == pad_config[0])
        keys[ACCEL] = is_pressed;

    if (button == pad_config[1])
        keys[BRAKE] = is_pressed;

    if (button == pad_config[2])
        keys[GEAR1] = is_pressed;

    if (button == pad_config[3])
        keys[GEAR2] = is_pressed;

    if (button == pad_config[4])
        keys[START] = is_pressed;

    if (button == pad_config[5])
        keys[COIN] = is_pressed;

    if (button == pad_config[6])
        keys[MENU] = is_pressed;

    if (button == pad_config[7])
        keys[VIEWPOINT] = is_pressed;
}
