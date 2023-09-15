/**
 * @file BufferedInput.hpp
 * @brief Contains definitions for buffered input handling and actions based off raylib
 */

#include "raylib.h"
#include "FastSignals/libfastsignals/include/signal.h"

#include <set>
#include <string>
#include <string_view>
#include <map>
#include <array>
#include <concepts>

namespace raylib {

	template<typename F>
	struct Delegate {};

	template<typename Ret, typename... Args>
	struct Delegate<Ret(Args...)>: public is::signals::signal<Ret(Args...)> {
		using callback_type = typename is::signals::signal<Ret(Args...)>::slot_type;

		Delegate& operator+=(callback_type callback) { this->connect(callback); return *this; }

		void set(callback_type callback) {
			this->disconnect_all_slots();
			this->connect(callback);
		}
		Delegate& operator=(callback_type callback) { set(callback); return *this; }
	};

	struct Button {
		enum class Type {
			Invalid = 0,
			Keyboard,
			Mouse,
			Gamepad
		} type;

		union {
			KeyboardKey keyboard;
			MouseButton mouse;
			struct {
				int id;
				GamepadButton button;
			} gamepad;
		};

		bool operator<(const Button& o) const;

		static Button key(KeyboardKey key) { return { Type::Keyboard, key}; }
		static Button btn(MouseButton button) { return { Type::Mouse, {.mouse = button}}; }
		static Button mouse_button(MouseButton button) { return btn(button); }
		static Button pad(GamepadButton button, int gamepad = 0) { return { Type::Keyboard, {.gamepad = {gamepad, button}}}; }
		static Button joy(GamepadButton button, int gamepad = 0) { return pad(button, gamepad); }
		static Button gamepad_button(GamepadButton button, int gamepad = 0) { return pad(button, gamepad); }

		static bool IsPressed(const Button& button);

		static uint8_t IsSetPressed(const std::set<Button>& buttons);
	};

	using ButtonSet = std::set<Button>;

	struct Action {
		enum class Type {
			Invalid = 0,
			Button,
			Axis,
			Vector,
			MultiButton
		} type;

		template<typename T>
		T& as() {
			static_assert(std::is_base_of_v<Action, T>, "as can only be used to cast to action types!");
			return *reinterpret_cast<T*>(this);
		}

		struct Gamepad {
			int id;
			GamepadAxis axis;
		};

		template<size_t N>
		struct MultiButtonData {
			enum Direction {
				Up, Down, Left, Right,
				UpLeft, UpRight, DownLeft, DownRight
			};
			std::array<ButtonSet, N> directions;
			std::array<uint8_t, N> lasts;
			bool normalize = true; // When true the maximum value returned for a given axis is 1, if false the value of each direction will be the sum of the buttons pressed pointing one direction minus the sum of the pressed buttons pointing the other direction
		};

		union Data {
			struct Button {
				ButtonSet* buttons;
				bool combo = false; // When true all buttons in the set must be pressed for the action to trigger
				uint8_t last_state = 0;
			} button;

			struct Axis {
				enum class Type {
					Invalid = 0,
					Gamepad,
					MouseWheel
				} type;

				Gamepad gamepad;
				float last_state = 0;
			} axis;

			struct Vector {
				enum class Type {
					Invalid = 0,
					MouseWheel,
					MousePosition,
					// MouseDelta,
					GamepadAxes,
					// QuadButtons,
				} type;

				struct GamepadAxes{
					Gamepad horizontal;
					Gamepad vertical;
				} gamepad;
				Vector2 last_state = { 0, 0 };
			} vector;

			struct MultiButton {
				MultiButtonData<4>* quadButtons;
				Vector2 last_state;
			} multi;
		} data; 

		// Delegate<void(const std::string_view name, uint8_t state, bool wasPressed)> callback;
		// Delegate<void(const std::string_view name, float state, float delta)> callback;
		Delegate<void(const std::string_view name, Vector2 state, Vector2 delta)> callback;

		Action() : type(Type::Invalid), data({.button = {}}) {}
		~Action() {
			if(type == Type::Button && data.button.buttons) delete data.button.buttons;
			else if(type == Type::MultiButton && data.multi.quadButtons) delete data.multi.quadButtons;
		}
		Action(Type t, Data d = {.button = {nullptr, false, 0}}) : type(t), data(d) {}
		Action(const Action&) = delete;
		Action(Action&& o) : Action() { *this = std::move(o); } 
		Action& operator=(const Action&) = delete;
		Action& operator=(Action&& o);

		Action& add_callback(is::signals::signal<void(const std::string_view name, Vector2 state, Vector2 delta)>::slot_type callback) {
			this->callback.connect(callback);
			return *this;
		}
		Action& set_callback(is::signals::signal<void(const std::string_view name, Vector2 state, Vector2 delta)>::slot_type callback) {
			this->callback.disconnect_all_slots();
			return add_callback(callback);
		}

		Action& add_callback(is::signals::signal<void(const std::string_view name, float state, float delta)>::slot_type callback) {
			return add_callback([callback](const std::string_view name, Vector2 state, Vector2 delta) {
				callback(name, state.x, delta.x);
			});
		}
		Action& set_callback(is::signals::signal<void(const std::string_view name, float state, float delta)>::slot_type callback) {
			this->callback.disconnect_all_slots();
			return add_callback(callback);
		}

		static Action button(Button button, bool combo = false) { return Action{Action::Type::Button, Action::Data{ .button = {new ButtonSet{button}, combo}}}; }
		static Action key(KeyboardKey key, bool combo = false) { return button({ Button::Type::Keyboard, key}, combo); }
		static Action mouse_button(MouseButton b, bool combo = false) { return button({ Button::Type::Mouse, {.mouse = b}}, combo); }
		static Action pad(GamepadButton b, int gamepad = 0, bool combo = false) { return button({ Button::Type::Keyboard, {.gamepad = {gamepad, b}}}, combo); }
		static Action joy(GamepadButton b, int gamepad = 0, bool combo = false) { return pad(b, gamepad, combo); }
		static Action gamepad_button(GamepadButton b, int gamepad = 0, bool combo = false) { return pad(b, gamepad, combo); }
		static Action button_set(ButtonSet buttons = {}, bool combo = false) { return Action{Action::Type::Button, Action::Data{ .button = { new ButtonSet(buttons), combo }}}; }
		static Action gamepad_axis(GamepadAxis axis = GAMEPAD_AXIS_LEFT_X, int gamepad = 0) {
			return {Action::Type::Axis, {.axis = {Data::Axis::Type::Gamepad, {gamepad, axis}}}};
		}
		static Action mouse_wheel() {
			return {Action::Type::Axis, {.axis = { Data::Axis::Type::MouseWheel }}};
		}
		static Action mouse_wheel_vectoe() {
			return {Action::Type::Vector, {.vector = { Data::Vector::Type::MouseWheel }}};
		}
		static Action mouse_position() {
			return {Action::Type::Vector, {.vector = { Data::Vector::Type::MousePosition }}};
		}
		static Action gamepad_axes(GamepadAxis horizontal = GAMEPAD_AXIS_LEFT_X, GamepadAxis vertical = GAMEPAD_AXIS_LEFT_Y, int gamepadHorizontal = 0, int gamepadVertical = -1);
		static Action quad(ButtonSet up, ButtonSet down, ButtonSet left, ButtonSet right, bool normalized = true) {
			Action out{Action::Type::MultiButton};
			out.data.multi.quadButtons = new MultiButtonData<4>{{ up, down, left, right }, {}, normalized};
			return out;
		}
		static Action wasd(
			ButtonSet up = {Button::key(KEY_W), Button::key(KEY_UP)},
			ButtonSet left = {Button::key(KEY_A), Button::key(KEY_LEFT)},
			ButtonSet down = {Button::key(KEY_S), Button::key(KEY_DOWN)},
			ButtonSet right = {Button::key(KEY_D), Button::key(KEY_RIGHT)},
			bool normalized = true
		) { return quad(up, down, left, right, normalized); }

		void pump_button(std::string_view name);
		void pump_axis(std::string_view name);
		void pump_vector(std::string_view name);
		void pump_multi_button(std::string_view name);

		// NOTE: After calling this function "this" will have been moved into the reciever and shouldn't be accessed anymore!
		Action&& move() { return std::move(*this); }
	};


	struct BufferedInput {
		std::map<std::string, Action> actions;

		void PumpMessages(bool whileUnfocused = false);
	};

}