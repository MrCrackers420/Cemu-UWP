#pragma once

// T98_BRIDGE_V2
//
// Platform-independent core of the racing-wheel bridge for the Cemu UWP host.
// Nothing in this header touches WinRT, XAML or the Cemu runtime, so the exact
// code that ships in the Xbox package is also compiled and exercised by
// tests/t98_bridge_tests.cpp on any C++17 compiler.
//
// Responsibilities:
//   * SlotTable   - stable player-slot ownership for Xbox gamepads plus one
//                   slot that is reserved exclusively for the wheel.
//   * Mapper      - wheel reading -> one emulated controller state, through
//                   two remappable binding layers (menu and racing).
//   * UiNavigator - wheel-driven navigation of the host's own interface,
//                   de-duplicated against navigation keys the Xbox shell may
//                   already project for the same press.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace T98
{
constexpr int kMaxPlayers = 4;
constexpr size_t kTargetCount = 16;
constexpr size_t kLayerCount = 2;

enum Layer : uint32_t
{
	LayerMenu = 0,
	LayerRace = 1,
};
constexpr uint32_t kLayerMaskMenu = 1u << LayerMenu;
constexpr uint32_t kLayerMaskRace = 1u << LayerRace;

// Physical wheel inputs that can be bound to an emulated Wii U control.
enum Source : int32_t
{
	SourceNone = 0,
	SourceAccelerator,
	SourceBrake,
	SourceClutch,
	SourceHandbrake,
	SourcePaddleRight,
	SourcePaddleLeft,
	SourceDpadUp,
	SourceDpadDown,
	SourceDpadLeft,
	SourceDpadRight,
	SourceButtonA,
	SourceButtonB,
	SourceButtonX,
	SourceButtonY,
	SourceButtonMenu,
	SourceButtonView,
	SourceWheelButton1, // ..SourceWheelButton16 are contiguous
	SourceWheelButton16 = SourceWheelButton1 + 15,
	SourceSteerLeft,
	SourceSteerRight,
	SourceCount
};

// Emulated Wii U controls, in the same order as the host's global layout.
enum Target : size_t
{
	TargetA = 0, TargetB, TargetX, TargetY,
	TargetL, TargetR, TargetZL, TargetZR,
	TargetPlus, TargetMinus,
	TargetUp, TargetDown, TargetLeft, TargetRight,
	TargetStickL, TargetStickR,
};

inline const wchar_t* SourceName(int32_t source)
{
	static const wchar_t* const names[SourceCount] = {
		L"(not bound)",
		L"Accelerator pedal", L"Brake pedal", L"Clutch pedal", L"Handbrake",
		L"Right paddle", L"Left paddle",
		L"D-pad Up", L"D-pad Down", L"D-pad Left", L"D-pad Right",
		L"A button", L"B button", L"X button", L"Y button",
		L"Menu button", L"View button",
		L"Wheel button 1", L"Wheel button 2", L"Wheel button 3", L"Wheel button 4",
		L"Wheel button 5", L"Wheel button 6", L"Wheel button 7", L"Wheel button 8",
		L"Wheel button 9", L"Wheel button 10", L"Wheel button 11", L"Wheel button 12",
		L"Wheel button 13", L"Wheel button 14", L"Wheel button 15", L"Wheel button 16",
		L"Steer left", L"Steer right",
	};
	return source >= 0 && source < SourceCount ? names[source] : names[0];
}

inline const wchar_t* TargetName(size_t target)
{
	static const wchar_t* const names[kTargetCount] = {
		L"Wii U A", L"Wii U B", L"Wii U X", L"Wii U Y",
		L"Wii U L", L"Wii U R", L"Wii U ZL", L"Wii U ZR",
		L"Wii U +", L"Wii U −",
		L"D-pad Up", L"D-pad Down", L"D-pad Left", L"D-pad Right",
		L"Left stick click", L"Right stick click",
	};
	return target < kTargetCount ? names[target] : L"";
}

// Bit layouts filled in by the WinRT glue. They are private to the bridge and
// deliberately independent of the numeric values of the WinRT enums.
constexpr uint32_t kWheelBitPaddleLeft = 1u << 0;  // RacingWheelButtons::PreviousGear
constexpr uint32_t kWheelBitPaddleRight = 1u << 1; // RacingWheelButtons::NextGear
constexpr uint32_t kWheelBitDpadUp = 1u << 2;
constexpr uint32_t kWheelBitDpadDown = 1u << 3;
constexpr uint32_t kWheelBitDpadLeft = 1u << 4;
constexpr uint32_t kWheelBitDpadRight = 1u << 5;
constexpr uint32_t kWheelBitButton1 = 1u << 6;     // Button1..Button16 -> bits 6..21

constexpr uint32_t kNavMenu = 1u << 0;
constexpr uint32_t kNavView = 1u << 1;
constexpr uint32_t kNavAccept = 1u << 2;  // A
constexpr uint32_t kNavCancel = 1u << 3;  // B
constexpr uint32_t kNavUp = 1u << 4;
constexpr uint32_t kNavDown = 1u << 5;
constexpr uint32_t kNavLeft = 1u << 6;
constexpr uint32_t kNavRight = 1u << 7;
constexpr uint32_t kNavContext1 = 1u << 8; // X
constexpr uint32_t kNavContext2 = 1u << 9; // Y

// One polled wheel sample.
struct WheelInput
{
	bool valid = false;
	double steer = 0.0;      // -1 (full left) .. +1 (full right)
	double throttle = 0.0;   // 0 .. 1
	double brake = 0.0;      // 0 .. 1
	double clutch = 0.0;     // 0 .. 1
	double handbrake = 0.0;  // 0 .. 1
	uint32_t wheelButtons = 0; // kWheelBit*
	uint32_t nav = 0;          // kNav*
};

struct Config
{
	bool enabled = true;
	int32_t player = 0;       // 0..3
	int32_t layerMode = 0;    // 0 = both layers active, 1 = switch with a wheel button
	int32_t toggleSource = SourceButtonY;
	int32_t uiNavMode = 0;    // UiNavigator::Mode
	bool invertSteering = false;
	double pedalThreshold = 0.30;       // pedal travel that counts as pressed
	double steerDeadzone = 0.02;        // wheel centre deadzone
	double steerSaturation = 0.70;      // wheel travel that gives a full stick
	double coreDeadzoneCompensation = 0.25; // Cemu's own stick deadzone
	std::array<std::array<int32_t, kTargetCount>, kLayerCount> bindings{};
};

inline std::array<int32_t, kTargetCount> DefaultBindings(Layer layer)
{
	std::array<int32_t, kTargetCount> result{};
	result.fill(SourceNone);
	result[TargetPlus] = SourceButtonMenu;
	result[TargetMinus] = SourceButtonView;
	result[TargetUp] = SourceDpadUp;
	result[TargetDown] = SourceDpadDown;
	result[TargetLeft] = SourceDpadLeft;
	result[TargetRight] = SourceDpadRight;
	if (layer == LayerMenu)
	{
		result[TargetA] = SourceButtonA; // confirm
		result[TargetB] = SourceButtonB; // back
		result[TargetX] = SourceButtonX;
		result[TargetY] = SourceButtonY;
	}
	else
	{
		result[TargetA] = SourceAccelerator;
		result[TargetB] = SourceBrake;
		result[TargetZR] = SourcePaddleRight; // drift / hop
		result[TargetZL] = SourcePaddleLeft;  // item
	}
	return result;
}

inline Config DefaultConfig()
{
	Config config;
	config.bindings[LayerMenu] = DefaultBindings(LayerMenu);
	config.bindings[LayerRace] = DefaultBindings(LayerRace);
	return config;
}

inline double Clamp(double value, double low, double high)
{
	if (!std::isfinite(value))
		return low;
	return (std::max)(low, (std::min)(value, high));
}

// Forces every field into its supported range. Stored settings and UI values
// always pass through here before they are used.
inline void Sanitize(Config& config)
{
	config.player = (std::max)(0, (std::min)(config.player, kMaxPlayers - 1));
	config.layerMode = config.layerMode == 1 ? 1 : 0;
	if (config.toggleSource < 0 || config.toggleSource >= SourceCount)
		config.toggleSource = SourceButtonY;
	if (config.uiNavMode < 0 || config.uiNavMode > 2)
		config.uiNavMode = 0;
	config.pedalThreshold = Clamp(config.pedalThreshold, 0.05, 0.90);
	config.steerDeadzone = Clamp(config.steerDeadzone, 0.0, 0.20);
	config.steerSaturation = Clamp(config.steerSaturation, 0.20, 1.0);
	config.coreDeadzoneCompensation = Clamp(config.coreDeadzoneCompensation, 0.0, 0.40);
	for (auto& layer : config.bindings)
		for (auto& source : layer)
			if (source < 0 || source >= SourceCount)
				source = SourceNone;
}

// Emulated controller state in the host's canonical layout (the layout that
// Cemu's host-gamepad profile consumes): Wii U A = bit 1, B = bit 0, X = bit 3,
// Y = bit 2, Minus = 4, Plus = 6, stick clicks = 7/8, L/R = 9/10,
// D-pad = 11..14, ZL/ZR = triggers.
struct PadState
{
	uint32_t buttons = 0;
	float leftX = 0.0f;
	float leftTrigger = 0.0f;
	float rightTrigger = 0.0f;
};

inline uint64_t SourceBit(int32_t source)
{
	return source > SourceNone && source < SourceCount ? (1ull << source) : 0ull;
}

class Mapper
{
public:
	void Reset()
	{
		m_latched = 0;
		m_toggleHeld = false;
		m_activeLayer = LayerMenu;
	}

	// Which sources are currently pressed. Analog sources use hysteresis so a
	// pedal resting at the threshold cannot chatter.
	uint64_t ActiveSources(const WheelInput& input, const Config& config)
	{
		if (!input.valid)
		{
			m_latched = 0;
			return 0;
		}
		uint64_t active = 0;
		const double press = config.pedalThreshold;
		const double release = (std::max)(0.02, press - 0.06);
		auto analog = [&](int32_t source, double value, double on, double off)
		{
			const uint64_t bit = SourceBit(source);
			const bool wasDown = (m_latched & bit) != 0;
			const bool down = std::isfinite(value) && (wasDown ? value > off : value >= on);
			if (down)
				active |= bit;
		};
		analog(SourceAccelerator, input.throttle, press, release);
		analog(SourceBrake, input.brake, press, release);
		analog(SourceClutch, input.clutch, press, release);
		analog(SourceHandbrake, input.handbrake, press, release);
		const double steer = config.invertSteering ? -input.steer : input.steer;
		analog(SourceSteerRight, steer, 0.50, 0.40);
		analog(SourceSteerLeft, -steer, 0.50, 0.40);

		auto digital = [&](int32_t source, bool down)
		{
			if (down)
				active |= SourceBit(source);
		};
		digital(SourcePaddleRight, (input.wheelButtons & kWheelBitPaddleRight) != 0);
		digital(SourcePaddleLeft, (input.wheelButtons & kWheelBitPaddleLeft) != 0);
		digital(SourceDpadUp, (input.wheelButtons & kWheelBitDpadUp) != 0 || (input.nav & kNavUp) != 0);
		digital(SourceDpadDown, (input.wheelButtons & kWheelBitDpadDown) != 0 || (input.nav & kNavDown) != 0);
		digital(SourceDpadLeft, (input.wheelButtons & kWheelBitDpadLeft) != 0 || (input.nav & kNavLeft) != 0);
		digital(SourceDpadRight, (input.wheelButtons & kWheelBitDpadRight) != 0 || (input.nav & kNavRight) != 0);
		digital(SourceButtonA, (input.nav & kNavAccept) != 0);
		digital(SourceButtonB, (input.nav & kNavCancel) != 0);
		digital(SourceButtonX, (input.nav & kNavContext1) != 0);
		digital(SourceButtonY, (input.nav & kNavContext2) != 0);
		digital(SourceButtonMenu, (input.nav & kNavMenu) != 0);
		digital(SourceButtonView, (input.nav & kNavView) != 0);
		for (int index = 0; index < 16; ++index)
			digital(SourceWheelButton1 + index,
				(input.wheelButtons & (kWheelBitButton1 << index)) != 0);
		m_latched = active;
		return active;
	}

	// Returns the mask of layers that apply to this frame. `toggled` is set on
	// the frame the active layer changed.
	uint32_t UpdateLayers(uint64_t sources, const Config& config, bool gameRunning, bool* toggled = nullptr)
	{
		if (toggled)
			*toggled = false;
		if (config.layerMode != 1)
		{
			m_toggleHeld = false;
			m_activeLayer = LayerMenu;
			return kLayerMaskMenu | kLayerMaskRace;
		}
		if (!gameRunning)
		{
			// Every title starts in the menu layer.
			m_activeLayer = LayerMenu;
			m_toggleHeld = (sources & SourceBit(config.toggleSource)) != 0;
			return kLayerMaskMenu;
		}
		const bool held = (sources & SourceBit(config.toggleSource)) != 0;
		if (held && !m_toggleHeld)
		{
			m_activeLayer = m_activeLayer == LayerMenu ? LayerRace : LayerMenu;
			if (toggled)
				*toggled = true;
		}
		m_toggleHeld = held;
		return 1u << m_activeLayer;
	}

	Layer ActiveLayer() const { return m_activeLayer; }

	static float SteeringOutput(const WheelInput& input, const Config& config)
	{
		if (!input.valid || !std::isfinite(input.steer))
			return 0.0f;
		double value = Clamp(input.steer, -1.0, 1.0);
		if (config.invertSteering)
			value = -value;
		const double magnitude = std::abs(value);
		const double deadzone = Clamp(config.steerDeadzone, 0.0, 0.20);
		if (magnitude <= deadzone)
			return 0.0f;
		const double saturation = (std::max)(deadzone + 0.01, Clamp(config.steerSaturation, 0.20, 1.0));
		const double normalized = Clamp((magnitude - deadzone) / (saturation - deadzone), 0.0, 1.0);
		// Cemu removes its own stick deadzone again; start just above it so the
		// first degrees of wheel travel already steer.
		const double compensation = Clamp(config.coreDeadzoneCompensation, 0.0, 0.40);
		const double output = compensation + (1.0 - compensation) * normalized;
		return static_cast<float>(std::copysign(Clamp(output, 0.0, 1.0), value));
	}

	static PadState BuildPad(const WheelInput& input, uint64_t sources, uint32_t layerMask, const Config& config)
	{
		PadState pad;
		if (!input.valid)
			return pad;
		// The layer-switch button never reaches the game.
		if (config.layerMode == 1)
			sources &= ~SourceBit(config.toggleSource);
		static const int8_t targetBits[kTargetCount] =
			{ 1, 0, 3, 2, 9, 10, -1, -2, 6, 4, 11, 12, 13, 14, 7, 8 };
		for (size_t layer = 0; layer < kLayerCount; ++layer)
		{
			if ((layerMask & (1u << layer)) == 0)
				continue;
			for (size_t target = 0; target < kTargetCount; ++target)
			{
				if ((sources & SourceBit(config.bindings[layer][target])) == 0)
					continue;
				const int8_t bit = targetBits[target];
				if (bit == -1)
					pad.leftTrigger = 1.0f;
				else if (bit == -2)
					pad.rightTrigger = 1.0f;
				else
					pad.buttons |= 1u << bit;
			}
		}
		if ((layerMask & kLayerMaskRace) != 0)
			pad.leftX = SteeringOutput(input, config);
		return pad;
	}

private:
	uint64_t m_latched = 0;
	bool m_toggleHeld = false;
	Layer m_activeLayer = LayerMenu;
};

// Player-slot ownership. Index == emulated player. A slot is in exactly one
// of these states: free, owned by a connected gamepad, remembered for a
// disconnected gamepad, or reserved for the wheel. The wheel's slot can never
// be handed to a gamepad, and no identity can own two slots.
struct SlotTable
{
	std::array<bool, kMaxPlayers> connected{};
	std::array<std::wstring, kMaxPlayers> ids{};
	std::array<std::wstring, kMaxPlayers> names{};
	std::array<uint64_t, kMaxPlayers> disconnectedOrder{};
	uint64_t disconnectSequence = 0;
	int reserved = -1;            // slot owned by the wheel, or -1
	std::wstring blockedIdentity; // the wheel's own device identity

	struct AssignResult
	{
		int slot = -1;
		bool replacesLiveHandle = false; // same identity reconnected while still marked connected
	};

	struct ReserveResult
	{
		bool changed = false;
		int previousReserved = -1;
		int movedFrom = -1; // gamepad slot that had to make room, or -1
		int movedTo = -1;   // where it went, or -1 when it could not be kept
	};

	bool IsFree(int slot) const
	{
		return slot >= 0 && slot < kMaxPlayers && slot != reserved &&
			!connected[slot] && ids[slot].empty();
	}

	bool HasContent(int slot) const
	{
		return slot >= 0 && slot < kMaxPlayers && (connected[slot] || !ids[slot].empty());
	}

	int FindIdentity(const std::wstring& identity) const
	{
		if (identity.empty())
			return -1;
		for (int index = 0; index < kMaxPlayers; ++index)
			if (index != reserved && ids[index] == identity)
				return index;
		return -1;
	}

	void Forget(int slot)
	{
		if (slot < 0 || slot >= kMaxPlayers)
			return;
		connected[slot] = false;
		ids[slot].clear();
		names[slot].clear();
		disconnectedOrder[slot] = 0;
	}

	// Chooses the player slot for a gamepad that just appeared.
	AssignResult Assign(const std::wstring& identity, const std::wstring& name)
	{
		AssignResult result;
		// The wheel is one player. If the system also projects it as a gamepad,
		// that projection must not become a second player.
		if (!identity.empty() && !blockedIdentity.empty() && identity == blockedIdentity)
			return result;

		const int remembered = FindIdentity(identity);
		if (remembered >= 0)
		{
			result.slot = remembered;
			result.replacesLiveHandle = connected[remembered];
			connected[remembered] = true;
			disconnectedOrder[remembered] = 0;
			if (!name.empty())
				names[remembered] = name;
			return result;
		}

		// Prefer a slot that has never belonged to another physical controller.
		for (int index = 0; index < kMaxPlayers; ++index)
		{
			if (!IsFree(index))
				continue;
			Occupy(index, identity, name);
			result.slot = index;
			return result;
		}

		// Every remaining slot is remembered for some pad. A genuinely new pad
		// replaces the oldest disconnected reservation.
		int replacement = -1;
		uint64_t oldestOrder = (std::numeric_limits<uint64_t>::max)();
		for (int index = 0; index < kMaxPlayers; ++index)
		{
			if (index == reserved || connected[index])
				continue;
			const uint64_t order = disconnectedOrder[index];
			if (replacement < 0 || (order != 0 && order < oldestOrder))
			{
				replacement = index;
				oldestOrder = order == 0 ? (std::numeric_limits<uint64_t>::max)() : order;
			}
		}
		if (replacement >= 0)
		{
			Occupy(replacement, identity, name);
			result.slot = replacement;
		}
		return result;
	}

	// Marks a gamepad slot as unplugged but keeps it remembered for that pad.
	void Disconnect(int slot)
	{
		if (slot < 0 || slot >= kMaxPlayers || !connected[slot])
			return;
		connected[slot] = false;
		if (!ids[slot].empty())
			disconnectedOrder[slot] = ++disconnectSequence;
		else
			names[slot].clear();
	}

	// A wheel may take its configured slot at any time unless that would pull
	// a connected gamepad out of a running game.
	bool CanReserve(int slot, bool gameRunning) const
	{
		if (slot < 0 || slot >= kMaxPlayers)
			return false;
		if (slot == reserved)
			return true;
		return !(gameRunning && connected[slot]);
	}

	// Gives `slot` to the wheel. A gamepad that owned it is moved, never merged.
	ReserveResult Reserve(int slot)
	{
		ReserveResult result;
		result.previousReserved = reserved;
		if (slot < 0 || slot >= kMaxPlayers || slot == reserved)
			return result;
		result.changed = true;
		const int previous = reserved;
		reserved = -1;
		if (HasContent(slot))
		{
			int destination = -1;
			if (previous >= 0)
				destination = previous; // wheel changed player: swap places
			else
			{
				for (int index = 0; index < kMaxPlayers && destination < 0; ++index)
					if (index != slot && IsFree(index))
						destination = index;
				if (destination < 0)
				{
					uint64_t oldestOrder = (std::numeric_limits<uint64_t>::max)();
					for (int index = 0; index < kMaxPlayers; ++index)
					{
						if (index == slot || connected[index])
							continue;
						const uint64_t order = disconnectedOrder[index];
						if (destination < 0 || (order != 0 && order < oldestOrder))
						{
							destination = index;
							oldestOrder = order == 0 ? (std::numeric_limits<uint64_t>::max)() : order;
						}
					}
				}
			}
			if (connected[slot])
			{
				result.movedFrom = slot;
				result.movedTo = destination;
			}
			if (destination >= 0)
			{
				connected[destination] = connected[slot];
				ids[destination] = ids[slot];
				names[destination] = names[slot];
				disconnectedOrder[destination] = disconnectedOrder[slot];
			}
			Forget(slot);
		}
		reserved = slot;
		return result;
	}

	// Returns the wheel's slot to the pool (wheel support switched off).
	int Release()
	{
		const int previous = reserved;
		reserved = -1;
		return previous;
	}

	int ConnectedGamepads() const
	{
		int count = 0;
		for (int index = 0; index < kMaxPlayers; ++index)
			if (index != reserved && connected[index])
				++count;
		return count;
	}

private:
	void Occupy(int slot, const std::wstring& identity, const std::wstring& name)
	{
		connected[slot] = true;
		ids[slot] = identity;
		names[slot] = name;
		disconnectedOrder[slot] = 0;
	}
};

// Wheel-driven navigation of the host interface.
class UiNavigator
{
public:
	enum Mode : int32_t
	{
		ModeAuto = 0,   // act only when the system did not already navigate
		ModeHost = 1,   // always act
		ModeSystem = 2, // never act
	};
	enum Action : uint32_t
	{
		ActionUp = 1u << 0,
		ActionDown = 1u << 1,
		ActionLeft = 1u << 2,
		ActionRight = 1u << 3,
		ActionAccept = 1u << 4,
		ActionCancel = 1u << 5,
	};
	static constexpr int kActionCount = 6;
	static constexpr uint64_t kSystemWaitMs = 90;      // time given to the system to project a key
	static constexpr uint64_t kSystemLookbackMs = 150; // tolerated lead of a projected key
	static constexpr uint64_t kRepeatDelayMs = 450;
	static constexpr uint64_t kRepeatIntervalMs = 130;

	static uint32_t ActionsFromNav(uint32_t nav, uint32_t wheelButtons)
	{
		uint32_t actions = 0;
		if ((nav & kNavUp) != 0 || (wheelButtons & kWheelBitDpadUp) != 0) actions |= ActionUp;
		if ((nav & kNavDown) != 0 || (wheelButtons & kWheelBitDpadDown) != 0) actions |= ActionDown;
		if ((nav & kNavLeft) != 0 || (wheelButtons & kWheelBitDpadLeft) != 0) actions |= ActionLeft;
		if ((nav & kNavRight) != 0 || (wheelButtons & kWheelBitDpadRight) != 0) actions |= ActionRight;
		if ((nav & kNavAccept) != 0) actions |= ActionAccept;
		if ((nav & kNavCancel) != 0) actions |= ActionCancel;
		return actions;
	}

	void Reset()
	{
		for (auto& state : m_states)
			state = State{};
	}

	// The Xbox shell projected a navigation key for this action.
	void OnSystemKey(uint32_t action, uint64_t nowMs)
	{
		for (int index = 0; index < kActionCount; ++index)
		{
			if ((action & (1u << index)) == 0)
				continue;
			m_states[index].systemKeyMs = nowMs;
			m_states[index].systemKeySeen = true;
		}
	}

	// `held` is the set of actions currently pressed on the wheel. Returns the
	// actions the host must perform now.
	uint32_t Update(uint32_t held, uint64_t nowMs, int32_t mode)
	{
		uint32_t fire = 0;
		for (int index = 0; index < kActionCount; ++index)
		{
			auto& state = m_states[index];
			const uint32_t bit = 1u << index;
			const bool down = (held & bit) != 0;
			const bool repeats = index < 4;
			if (down && !state.down)
			{
				state.pressMs = nowMs;
				state.systemOwned = false;
				state.nextRepeatMs = nowMs + kRepeatDelayMs;
				if (mode == ModeHost)
					fire |= bit;
				else if (mode == ModeAuto)
				{
					state.pending = true;
					state.pendingDueMs = nowMs + kSystemWaitMs;
				}
			}
			state.down = down;
			if (state.pending && nowMs >= state.pendingDueMs)
			{
				state.pending = false;
				const bool systemHandled = state.systemKeySeen &&
					state.systemKeyMs + kSystemLookbackMs >= state.pressMs;
				if (systemHandled)
					state.systemOwned = true;
				else if (mode == ModeAuto)
					fire |= bit;
			}
			if (down && repeats && !state.pending && nowMs >= state.nextRepeatMs)
			{
				state.nextRepeatMs = nowMs + kRepeatIntervalMs;
				if (mode == ModeHost || (mode == ModeAuto && !state.systemOwned))
					fire |= bit;
			}
		}
		return fire;
	}

private:
	struct State
	{
		bool down = false;
		bool pending = false;
		bool systemOwned = false;
		bool systemKeySeen = false;
		uint64_t pressMs = 0;
		uint64_t pendingDueMs = 0;
		uint64_t nextRepeatMs = 0;
		uint64_t systemKeyMs = 0;
	};
	std::array<State, kActionCount> m_states{};
};
}
