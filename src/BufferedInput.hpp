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

	struct Button;
	struct ButtonAction;
	struct AxisAction;
	struct VectorAction;
	struct MultiButtonsAction;

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

		static ButtonAction button(Button button, bool combo = false);
		static ButtonAction key(KeyboardKey key, bool combo = false);
		static ButtonAction mouse_button(MouseButton b, bool combo = false);
		static ButtonAction pad(GamepadButton b, int gamepad = 0, bool combo = false);
		static ButtonAction joy(GamepadButton b, int gamepad = 0, bool combo = false);
		static ButtonAction gamepad_button(GamepadButton b, int gamepad = 0, bool combo = false);
		static ButtonAction button_set(ButtonSet buttons = {}, bool combo = false);
		static AxisAction gamepad_axis(GamepadAxis axis = GAMEPAD_AXIS_LEFT_X, int gamepad = 0);
		static AxisAction mouse_wheel();
		static VectorAction mouse_wheel_vector();
		static VectorAction mouse_position();
		static VectorAction gamepad_axes(GamepadAxis horizontal = GAMEPAD_AXIS_LEFT_X, GamepadAxis vertical = GAMEPAD_AXIS_LEFT_Y, int gamepadHorizontal = 0, int gamepadVertical = -1);
		static MultiButtonsAction quad_buttons(ButtonSet up, ButtonSet down, ButtonSet left, ButtonSet right, bool normalized = true);
		static MultiButtonsAction wasd(ButtonSet up, ButtonSet left, ButtonSet down, ButtonSet right, bool normalized = true);
		static MultiButtonsAction wasd(bool normalized = true);
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

	struct ButtonAction: public Action {
		ButtonSet buttons;
		bool combo = false; // When true all buttons in the set must be pressed for the action to trigger
		uint8_t last_state = 0;
		Delegate<void(const std::string_view name, uint8_t state, bool wasPressed)> callback;

		ButtonAction& set_callback(is::signals::signal<void(const std::string_view name, uint8_t state, bool wasPressed)>::slot_type callback);

		// NOTE: After calling this function "this" will have been moved into the pointer and shouldn't be accessed anymore!
		std::unique_ptr<Action> make_unique() {
			return std::make_unique<ButtonAction>(std::move(*this));
		}

		void Pump(std::string_view name);

		static ButtonAction button(Button button = {Button::Type::Mouse, {.mouse = MOUSE_BUTTON_LEFT}}, bool combo = false) {
			return {Action::Type::Button, {button}, combo};
		}

		static ButtonAction key(KeyboardKey key, bool combo = false) { return button({ Button::Type::Keyboard, key}, combo); }
		static ButtonAction btn(MouseButton b, bool combo = false) { return button({ Button::Type::Mouse, {.mouse = b}}, combo); }
		static ButtonAction mouse_button(MouseButton b, bool combo = false) { return btn(b, combo); }
		static ButtonAction pad(GamepadButton b, int gamepad = 0, bool combo = false) { return button({ Button::Type::Keyboard, {.gamepad = {gamepad, b}}}, combo); }
		static ButtonAction joy(GamepadButton b, int gamepad = 0, bool combo = false) { return pad(b, gamepad, combo); }
		static ButtonAction gamepad_button(GamepadButton b, int gamepad = 0, bool combo = false) { return pad(b, gamepad, combo); }

		static ButtonAction set(ButtonSet buttons = {}, bool combo = false) {
			return {Action::Type::Button, buttons, combo};
		}
	};

	struct AxisAction: public Action {
		enum class Type {
			Invalid = 0,
			Gamepad,
			MouseWheel
		} axis;

		struct Gamepad {
			int id;
			GamepadAxis axis;
		};

		union {
			Gamepad gamepad;
		};
		float last_state = 0;
		Delegate<void(const std::string_view name, float state, float delta)> callback;

		AxisAction& set_callback(is::signals::signal<void(const std::string_view name, float state, float delta)>::slot_type callback);

		// NOTE: After calling this function "this" will have been moved into the pointer and shouldn't be accessed anymore!
		std::unique_ptr<Action> make_unique() {
			return std::make_unique<AxisAction>(std::move(*this));
		}

		void Pump(std::string_view name);

		static AxisAction gamepad_axis(GamepadAxis axis = GAMEPAD_AXIS_LEFT_X, int gamepad = 0) {
			AxisAction a = {Action::Type::Axis, AxisAction::Type::MouseWheel};
			a.gamepad = {gamepad, axis};
			return a;
		}

		static AxisAction mouse_wheel() {
			return {Action::Type::Axis, AxisAction::Type::MouseWheel};
		}
	};

	struct VectorAction: public Action {
		enum class Type {
			Invalid = 0,
			MouseWheel,
			MousePosition,
			// MouseDelta,
			GamepadAxes,
			// QuadButtons,
		} vector;

		union {
			struct {
				AxisAction::Gamepad horizontal;
				AxisAction::Gamepad vertical;
			} gamepad;
		};
		Vector2 last_state = { 0, 0};
		Delegate<void(const std::string_view name, Vector2 state, Vector2 delta)> callback;

		VectorAction& set_callback(is::signals::signal<void(const std::string_view name, Vector2 state, Vector2 delta)>::slot_type callback);

		// NOTE: After calling this function "this" will have been moved into the pointer and shouldn't be accessed anymore!
		std::unique_ptr<Action> make_unique() {
			return std::make_unique<VectorAction>(std::move(*this));
		}

		void Pump(std::string_view name);

		static VectorAction mouse_wheel() {
			return {Action::Type::Vector, VectorAction::Type::MouseWheel};
		}

		static VectorAction mouse_position() {
			return {Action::Type::Vector, VectorAction::Type::MousePosition};
		}

		static VectorAction gamepad_axes(GamepadAxis horizontal = GAMEPAD_AXIS_LEFT_X, GamepadAxis vertical = GAMEPAD_AXIS_LEFT_Y, int gamepadHorizontal = 0, int gamepadVertical = -1);
	};

	struct MultiButtonsAction: public Action {
		template<size_t N>
		struct ButtonData {
			enum Direction {
				Up, Down, Left, Right,
				UpLeft, UpRight, DownLeft, DownRight
			};
			std::array<ButtonSet, N> directions;
			std::array<uint8_t, N> lasts;
			bool normalize = true; // When true the maximum value returned for a given axis is 1, if false the value of each direction will be the sum of the buttons pressed pointing one direction minus the sum of the pressed buttons pointing the other direction
		};

		ButtonData<4> quadButtons;
		Vector2 last_state;
		Delegate<void(const std::string_view name, Vector2 state, Vector2 delta)> callback;

		MultiButtonsAction& set_callback(is::signals::signal<void(const std::string_view name, Vector2 state, Vector2 delta)>::slot_type callback);

		// NOTE: After calling this function "this" will have been moved into the pointer and shouldn't be accessed anymore!
		std::unique_ptr<Action> make_unique() {
			return std::make_unique<MultiButtonsAction>(std::move(*this));
		}

		void Pump(std::string_view name);

		static MultiButtonsAction quad(ButtonSet up, ButtonSet down, ButtonSet left, ButtonSet right, bool normalized = true) {
			MultiButtonsAction out{Action::Type::MultiButton};
			out.quadButtons = {{ up, down, left, right }, {}, normalized};
			return out;
		}
		static MultiButtonsAction wasd(
			ButtonSet up = {Button::key(KEY_W), Button::key(KEY_UP)},
			ButtonSet left = {Button::key(KEY_A), Button::key(KEY_LEFT)},
			ButtonSet down = {Button::key(KEY_S), Button::key(KEY_DOWN)},
			ButtonSet right = {Button::key(KEY_D), Button::key(KEY_RIGHT)},
			bool normalized = true
		) { return quad(up, down, left, right, normalized); }
	};


	struct BufferedInput {
		std::map<std::string, std::unique_ptr<Action>> actions;

		void PumpMessages(bool whileUnfocused = false);

		// void SetExitAction(std::unique_ptr<ButtonAction>&& action) {
		// 	SetExitKey(KEY_NULL);

		// 	action->set_callback([](const std::string_view, uint8_t pressed, bool wasPressed){
		// 		if(pressed) CloseWindow();
		// 	});
		// 	actions["exit"] = std::move(action);
		// }
		// void SetExitAction(ButtonAction&& action) { SetExitAction(std::make_unique<ButtonAction>(std::move(action))); }
	};

}