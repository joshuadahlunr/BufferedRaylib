#include "BufferedInput.hpp"

#include "raymath.h"

#include <set>
#include <string>
#include <string_view>
#include <map>
#include <array>
#include <concepts>

namespace raylib {

	ButtonAction Action::button(Button button, bool combo /*= false*/) {
		return ButtonAction::button(button, combo);
	}

	ButtonAction Action::key(KeyboardKey key, bool combo /*= false*/) { return ButtonAction::key(key, combo); }
	ButtonAction Action::mouse_button(MouseButton b, bool combo /*= false*/) {return ButtonAction::mouse_button(b, combo); }
	ButtonAction Action::pad(GamepadButton b, int gamepad /*= 0*/, bool combo /*= false*/) { return ButtonAction::pad(b, gamepad, combo); }
	ButtonAction Action::joy(GamepadButton b, int gamepad /*= 0*/, bool combo /*= false*/) { return ButtonAction::joy(b, gamepad, combo); }
	ButtonAction Action::gamepad_button(GamepadButton b, int gamepad /*= 0*/, bool combo /*= false*/) { return ButtonAction::gamepad_button(b, gamepad, combo); }

	ButtonAction Action::button_set(ButtonSet buttons /*= {}*/, bool combo /*= false*/) {
		return ButtonAction::set(buttons, combo);
	}

	AxisAction Action::gamepad_axis(GamepadAxis axis /*= GAMEPAD_AXIS_LEFT_X*/, int gamepad /*= 0*/) {
		return AxisAction::gamepad_axis(axis, gamepad);
	}

	AxisAction Action::mouse_wheel() { return AxisAction::mouse_wheel(); }

	VectorAction Action::mouse_wheel_vector() { return VectorAction::mouse_wheel(); }

	VectorAction Action::mouse_position() { return VectorAction::mouse_position(); }

	VectorAction Action::gamepad_axes(
		GamepadAxis horizontal /*= GAMEPAD_AXIS_LEFT_X*/,
		GamepadAxis vertical /*= GAMEPAD_AXIS_LEFT_Y*/,
		int gamepadHorizontal /*= 0*/,
		int gamepadVertical /*= -1*/
	) {
		return VectorAction::gamepad_axes(horizontal, vertical, gamepadHorizontal, gamepadVertical);
	}

	MultiButtonsAction Action::quad_buttons(ButtonSet up, ButtonSet down, ButtonSet left, ButtonSet right, bool normalized /*= true*/) {
		return MultiButtonsAction::quad(up, down, left, right, normalized);
	}
	MultiButtonsAction Action::wasd(ButtonSet up, ButtonSet left, ButtonSet down, ButtonSet right, bool normalized /*= true*/)
	{ return MultiButtonsAction::quad(up, down, left, right, normalized); }
	MultiButtonsAction Action::wasd(bool normalized /*= true*/)
	{ return MultiButtonsAction::quad({Button::key(KEY_W), Button::key(KEY_UP)}, {Button::key(KEY_S), Button::key(KEY_DOWN)}, {Button::key(KEY_A), Button::key(KEY_LEFT)}, {Button::key(KEY_D), Button::key(KEY_RIGHT)}, normalized); }

	bool Button::operator<(const Button& o) const {
		if (type != o.type) return type < o.type;
		if (type == Type::Gamepad && gamepad.id == o.gamepad.id)
			return gamepad.button < o.gamepad.button;
		return keyboard < o.keyboard; // They should all be castable to a keyboard key, thus comparing only it should be fine!
	}

	bool Button::IsPressed(const Button& button) {
		switch(button.type) {
		break; case Button::Type::Keyboard:
			return IsKeyDown(button.keyboard);
		break; case Button::Type::Mouse:
			return IsMouseButtonDown(button.mouse);
		break; case Button::Type::Gamepad: {
			return IsGamepadButtonDown(button.gamepad.id, button.gamepad.button);
		}
		break; default: assert(button.type != Button::Type::Invalid);
		}
		return false;
	}

	uint8_t Button::IsSetPressed(const std::set<Button>& buttons) {
		uint8_t state = 0;
		for(auto& button: buttons)
			state += IsPressed(button);
		return state;
	}

	ButtonAction& ButtonAction::set_callback(is::signals::signal<void(const std::string_view name, uint8_t state, bool wasPressed)>::slot_type callback) {
		this->callback.disconnect_all_slots();
		this->callback.connect(callback);
		return *this;
	}

	void ButtonAction::Pump(std::string_view name) {
		uint8_t state = Button::IsSetPressed(buttons);
		if (state != last_state) {
			if(combo) {
				if(bool comboState = state == buttons.size(), lastComboState = last_state == buttons.size(); comboState != lastComboState)
					callback(name, comboState, lastComboState);
			} else callback(name, state, last_state);
			last_state = state;
		}
	}

	AxisAction& AxisAction::set_callback(is::signals::signal<void(const std::string_view name, float state, float delta)>::slot_type callback) {
		this->callback.disconnect_all_slots();
		this->callback.connect(callback);
		return *this;
	}

	void AxisAction::Pump(std::string_view name) {
		float state = last_state;
		switch(axis) {
		break; case Type::Gamepad: {
			float movement = GetGamepadAxisMovement(gamepad.id, gamepad.axis);
			if(movement != 0) state += movement;
		}
		break; case Type::MouseWheel: {
			float movement = GetMouseWheelMove();
			if(movement != 0) state += movement;
		}
		break; default: assert(axis != Type::Invalid);
		}
		if (state != last_state) {
			callback(name, state, state - last_state);
			last_state = state;
		}
	}

	VectorAction& VectorAction::set_callback(is::signals::signal<void(const std::string_view name, Vector2 state, Vector2 delta)>::slot_type callback) {
		this->callback.disconnect_all_slots();
		this->callback.connect(callback);
		return *this;
	}

	void VectorAction::Pump(std::string_view name) {
		Vector2 state = last_state;
		switch(vector) {
		break; case Type::MouseWheel:
			state = GetMouseWheelMoveV();
		break; case Type::MousePosition:
			state = GetMousePosition();
		// break; case Type::MouseDelta:
		//     state = GetMouseDelta();
		break; case Type::GamepadAxes: {
			state.x += GetGamepadAxisMovement(gamepad.horizontal.id, gamepad.horizontal.axis);
			state.y += GetGamepadAxisMovement(gamepad.vertical.id, gamepad.vertical.axis);
		}
		break; default: assert(vector != Type::Invalid);
		}
		if (!Vector2Equals(state, last_state)) {
			callback(name, state, Vector2Subtract(state, last_state));
			last_state = state;
		}
	}

	VectorAction VectorAction::gamepad_axes(GamepadAxis horizontal /*= GAMEPAD_AXIS_LEFT_X*/, GamepadAxis vertical /*= GAMEPAD_AXIS_LEFT_Y*/, int gamepadHorizontal /*= 0*/, int gamepadVertical /*= -1*/) {
		if(gamepadVertical < 0) gamepadVertical = gamepadHorizontal;

		VectorAction out = {Action::Type::Vector, VectorAction::Type::GamepadAxes};
		out.gamepad = {{gamepadHorizontal, horizontal}, {gamepadVertical, vertical}};
		return out;
	}

	MultiButtonsAction& MultiButtonsAction::set_callback(is::signals::signal<void(const std::string_view name, Vector2 state, Vector2 delta)>::slot_type callback) {
		this->callback.disconnect_all_slots();
		this->callback.connect(callback);
		return *this;
	}

	void MultiButtonsAction::Pump(std::string_view name) {
		Vector2 state = last_state;{
			std::array<uint8_t, 4> buttonState;
			for(uint8_t i = 0; i < 4; i++) {
				buttonState[i] = Button::IsSetPressed(quadButtons.directions[i]);
				if(quadButtons.normalize && buttonState[i] > 0)
					buttonState[i] = 1;
			}
			state.x = buttonState[ButtonData<4>::Direction::Left] - buttonState[ButtonData<4>::Direction::Right];
			state.y = buttonState[ButtonData<4>::Direction::Up] - buttonState[ButtonData<4>::Direction::Down];
		}
		if (!Vector2Equals(state, last_state)) {
			callback(name, state, Vector2Subtract(state, last_state));
			last_state = state;
		}
	}

	void BufferedInput::PumpMessages(bool whileUnfocused /*= false*/) {
		if(!whileUnfocused && !IsWindowFocused()) return;
		for(auto& [name, action]: actions) {
			switch(action->type){
			break; case Action::Type::Button:
				action->as<ButtonAction>().Pump(name);
			break; case Action::Type::Axis:
				action->as<AxisAction>().Pump(name);
			break; case Action::Type::Vector:
				action->as<VectorAction>().Pump(name);
			break; case Action::Type::MultiButton:
				action->as<MultiButtonsAction>().Pump(name);
			break; default: assert(action->type != Action::Type::Invalid);
			}
		}
	}
}