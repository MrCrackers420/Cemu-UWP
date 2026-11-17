// Simulated tests for the platform-independent part of the T98 wheel bridge.
// These exercise the same T98Bridge.h that is compiled into the Xbox package.
// They are NOT hardware tests: no wheel, console or game is involved.

#include "../T98Bridge.h"

#include <cstdio>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace
{
int g_failures = 0;
int g_checks = 0;

#define CHECK(condition) \
	do { \
		++g_checks; \
		if (!(condition)) { \
			++g_failures; \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
		} \
	} while (0)

using namespace T98;

constexpr uint32_t kBitWiiUA = 1u << 1;
constexpr uint32_t kBitWiiUB = 1u << 0;
constexpr uint32_t kBitWiiUX = 1u << 3;
constexpr uint32_t kBitWiiUY = 1u << 2;
constexpr uint32_t kBitMinus = 1u << 4;
constexpr uint32_t kBitPlus = 1u << 6;
constexpr uint32_t kBitUp = 1u << 11;
constexpr uint32_t kBitDown = 1u << 12;
constexpr uint32_t kBitLeft = 1u << 13;
constexpr uint32_t kBitRight = 1u << 14;

// Mirrors what DirectXPage publishes to Cemu for each player.
struct Published
{
	bool connected = false;
	uint32_t buttons = 0;
	float leftX = 0.0f;
	float leftTrigger = 0.0f;
	float rightTrigger = 0.0f;
};

struct GamepadSample
{
	uint32_t buttons = 0;
	float leftX = 0.0f;
};

// Minimal stand-in for the host page: the same ownership rules, no WinRT.
struct Host
{
	SlotTable slots;
	Config config = DefaultConfig();
	Mapper mapper;
	bool wheelPresent = false;
	bool wheelSeen = false;
	bool gameRunning = false;
	std::wstring wheelId = L"wheel-T98";
	WheelInput wheelInput;
	std::array<GamepadSample, kMaxPlayers> pads{};

	void UpdateClaim()
	{
		if (!config.enabled)
		{
			slots.Release();
			slots.blockedIdentity.clear();
			return;
		}
		if (!wheelSeen)
			return;
		if (slots.reserved == config.player)
			return;
		if (!slots.CanReserve(config.player, gameRunning))
			return;
		const auto move = slots.Reserve(config.player);
		if (move.movedFrom >= 0)
		{
			const auto sample = pads[move.movedFrom];
			pads[move.movedFrom] = {};
			if (move.movedTo >= 0)
				pads[move.movedTo] = sample;
		}
	}

	void PlugWheel()
	{
		wheelPresent = true;
		wheelSeen = true;
		wheelInput = {};
		wheelInput.valid = true;
		mapper.Reset();
		if (config.enabled)
		{
			slots.blockedIdentity = wheelId;
			const int duplicate = slots.FindIdentity(wheelId);
			if (duplicate >= 0)
			{
				slots.Forget(duplicate);
				pads[duplicate] = {};
			}
		}
		UpdateClaim();
	}

	void UnplugWheel()
	{
		wheelPresent = false;
		wheelInput = {};
		mapper.Reset();
	}

	int PlugPad(const std::wstring& id)
	{
		const auto result = slots.Assign(id, id);
		if (result.slot >= 0)
			pads[result.slot] = {};
		return result.slot;
	}

	void UnplugPad(const std::wstring& id)
	{
		const int slot = slots.FindIdentity(id);
		if (slot >= 0)
		{
			slots.Disconnect(slot);
			pads[slot] = {};
		}
	}

	std::array<Published, kMaxPlayers> Publish()
	{
		std::array<Published, kMaxPlayers> out{};
		const int wheelSlot = config.enabled ? slots.reserved : -1;
		for (int player = 0; player < kMaxPlayers; ++player)
		{
			if (player == wheelSlot)
			{
				if (!wheelPresent)
					continue;
				WheelInput input = wheelInput;
				input.valid = true;
				const auto sources = mapper.ActiveSources(input, config);
				const auto layers = mapper.UpdateLayers(sources, config, gameRunning);
				const auto pad = Mapper::BuildPad(input, sources, layers, config);
				out[player] = { true, pad.buttons, pad.leftX, pad.leftTrigger, pad.rightTrigger };
				continue;
			}
			if (!slots.connected[player])
				continue;
			out[player] = { true, pads[player].buttons, pads[player].leftX, 0.0f, 0.0f };
		}
		return out;
	}
};

int ConnectedPlayers(const std::array<Published, kMaxPlayers>& out)
{
	int count = 0;
	for (const auto& player : out)
		if (player.connected)
			++count;
	return count;
}

void CheckInvariants(const SlotTable& slots)
{
	std::set<std::wstring> seen;
	for (int index = 0; index < kMaxPlayers; ++index)
	{
		if (index == slots.reserved)
		{
			CHECK(!slots.connected[index]);
			CHECK(slots.ids[index].empty());
			continue;
		}
		if (!slots.ids[index].empty())
		{
			CHECK(seen.insert(slots.ids[index]).second); // one identity, one slot
			CHECK(slots.blockedIdentity.empty() || slots.ids[index] != slots.blockedIdentity);
		}
	}
}

void TestDefaultMapping()
{
	const Config config = DefaultConfig();
	Mapper mapper;
	const uint32_t both = kLayerMaskMenu | kLayerMaskRace;
	auto build = [&](WheelInput input)
	{
		input.valid = true;
		const auto sources = mapper.ActiveSources(input, config);
		CHECK(mapper.UpdateLayers(sources, config, true) == both);
		return Mapper::BuildPad(input, sources, both, config);
	};

	WheelInput idle;
	auto pad = build(idle);
	CHECK(pad.buttons == 0 && pad.leftX == 0.0f && pad.leftTrigger == 0.0f && pad.rightTrigger == 0.0f);

	WheelInput accelerate; accelerate.throttle = 1.0;
	CHECK(build(accelerate).buttons == kBitWiiUA);          // accelerator -> A
	mapper.Reset();
	WheelInput brake; brake.brake = 1.0;
	CHECK(build(brake).buttons == kBitWiiUB);               // brake -> B
	mapper.Reset();
	WheelInput right; right.wheelButtons = kWheelBitPaddleRight;
	pad = build(right);
	CHECK(pad.rightTrigger == 1.0f && pad.leftTrigger == 0.0f && pad.buttons == 0); // right paddle -> ZR (drift)
	WheelInput left; left.wheelButtons = kWheelBitPaddleLeft;
	pad = build(left);
	CHECK(pad.leftTrigger == 1.0f && pad.rightTrigger == 0.0f && pad.buttons == 0); // left paddle -> ZL (item)

	WheelInput dpad; dpad.nav = kNavUp;
	CHECK(build(dpad).buttons == kBitUp);
	dpad.nav = kNavDown; CHECK(build(dpad).buttons == kBitDown);
	dpad.nav = kNavLeft; CHECK(build(dpad).buttons == kBitLeft);
	dpad.nav = kNavRight; CHECK(build(dpad).buttons == kBitRight);
	dpad.nav = 0; dpad.wheelButtons = kWheelBitDpadUp; CHECK(build(dpad).buttons == kBitUp);

	WheelInput action; action.nav = kNavAccept;
	CHECK(build(action).buttons == kBitWiiUA);               // A button -> confirm
	action.nav = kNavCancel; CHECK(build(action).buttons == kBitWiiUB); // B button -> back
	action.nav = kNavMenu; CHECK(build(action).buttons == kBitPlus);    // Menu -> Start / pause
	action.nav = kNavView; CHECK(build(action).buttons == kBitMinus);   // View -> Minus
	action.nav = kNavContext1; CHECK(build(action).buttons == kBitWiiUX);
	action.nav = kNavContext2; CHECK(build(action).buttons == kBitWiiUY);

	// Steering is analog, signed, saturates, and survives Cemu's 0.25 deadzone.
	WheelInput steer; steer.steer = 0.0;
	CHECK(build(steer).leftX == 0.0f);
	steer.steer = 0.01; CHECK(build(steer).leftX == 0.0f);   // inside centre deadzone
	steer.steer = 0.05;
	const float small = build(steer).leftX;
	CHECK(small > 0.25f && small < 0.35f);
	steer.steer = 0.36;
	const float half = build(steer).leftX;
	CHECK(half > small && half < 1.0f);
	steer.steer = 0.70; CHECK(build(steer).leftX == 1.0f);
	steer.steer = 1.0; CHECK(build(steer).leftX == 1.0f);
	steer.steer = -1.0; CHECK(build(steer).leftX == -1.0f);
	steer.steer = -0.05; CHECK(build(steer).leftX == -small);
	steer.steer = 7.0; CHECK(build(steer).leftX == 1.0f);
	steer.steer = std::numeric_limits<double>::quiet_NaN(); CHECK(build(steer).leftX == 0.0f);
	steer.steer = std::numeric_limits<double>::infinity(); CHECK(build(steer).leftX == 0.0f);

	// Monotonic response over the whole travel.
	float previous = -2.0f;
	for (int step = -100; step <= 100; ++step)
	{
		steer.steer = step / 100.0;
		const float value = build(steer).leftX;
		CHECK(value >= previous);
		CHECK(value >= -1.0f && value <= 1.0f);
		previous = value;
	}

	// Simultaneous inputs combine without interfering.
	WheelInput racing; racing.throttle = 1.0; racing.steer = -0.4;
	racing.wheelButtons = kWheelBitPaddleRight | kWheelBitPaddleLeft;
	pad = build(racing);
	CHECK(pad.buttons == kBitWiiUA && pad.leftX < 0.0f && pad.leftTrigger == 1.0f && pad.rightTrigger == 1.0f);

	// An invalid sample (wheel gone) is completely neutral.
	WheelInput gone = racing;
	gone.valid = false;
	const auto sources = mapper.ActiveSources(gone, config);
	CHECK(sources == 0);
	pad = Mapper::BuildPad(gone, sources, both, config);
	CHECK(pad.buttons == 0 && pad.leftX == 0.0f && pad.leftTrigger == 0.0f && pad.rightTrigger == 0.0f);
}

void TestPedalHysteresis()
{
	Config config = DefaultConfig();
	Mapper mapper;
	WheelInput input; input.valid = true;
	auto accel = [&](double value)
	{
		input.throttle = value;
		return (mapper.ActiveSources(input, config) & SourceBit(SourceAccelerator)) != 0;
	};
	CHECK(!accel(0.0));
	CHECK(!accel(0.29));
	CHECK(accel(0.30));
	CHECK(accel(0.27));  // stays pressed inside the hysteresis band
	CHECK(!accel(0.20));
	CHECK(!accel(0.27)); // and stays released inside it
	CHECK(accel(0.95));
	CHECK(!accel(std::numeric_limits<double>::quiet_NaN()));
	config.pedalThreshold = 0.80;
	mapper.Reset();
	CHECK(!accel(0.5));
	CHECK(accel(0.85));
}

void TestLayers()
{
	Config config = DefaultConfig();
	config.layerMode = 1;
	config.toggleSource = SourceButtonY;
	Mapper mapper;
	WheelInput input; input.valid = true;
	bool toggled = false;
	auto frame = [&](bool gameRunning)
	{
		const auto sources = mapper.ActiveSources(input, config);
		const auto layers = mapper.UpdateLayers(sources, config, gameRunning, &toggled);
		return Mapper::BuildPad(input, sources, layers, config);
	};

	// Outside a game only the menu layer applies: pedals and steering are inert.
	input.throttle = 1.0; input.steer = 0.9;
	auto pad = frame(false);
	CHECK(pad.buttons == 0 && pad.leftX == 0.0f);
	// A new game starts in the menu layer.
	pad = frame(true);
	CHECK(pad.buttons == 0 && pad.leftX == 0.0f && mapper.ActiveLayer() == LayerMenu);
	input.nav = kNavAccept;
	CHECK(frame(true).buttons == kBitWiiUA);
	// Press the switch button: racing layer, and Y itself never reaches the game.
	input.nav = kNavContext2;
	pad = frame(true);
	CHECK(toggled && mapper.ActiveLayer() == LayerRace);
	CHECK(pad.buttons == kBitWiiUA && pad.leftX == 1.0f); // accelerator now drives A
	pad = frame(true); // still held: no second toggle
	CHECK(!toggled && mapper.ActiveLayer() == LayerRace);
	input.nav = 0;
	frame(true);
	input.nav = kNavAccept; // the wheel's A button is a menu-layer binding
	input.throttle = 0.0;
	CHECK(frame(true).buttons == 0);
	input.nav = kNavContext2;
	frame(true);
	CHECK(toggled && mapper.ActiveLayer() == LayerMenu);
	input.nav = 0;
	frame(true);
	// Leaving the game resets to the menu layer.
	input.nav = kNavContext2; frame(true); input.nav = 0; frame(true);
	CHECK(mapper.ActiveLayer() == LayerRace);
	frame(false);
	CHECK(mapper.ActiveLayer() == LayerMenu);

	// Custom bindings: steering as menu navigation, clutch as Y.
	Config custom = DefaultConfig();
	custom.layerMode = 1;
	custom.bindings[LayerMenu][TargetLeft] = SourceSteerLeft;
	custom.bindings[LayerMenu][TargetRight] = SourceSteerRight;
	custom.bindings[LayerMenu][TargetY] = SourceClutch;
	Mapper customMapper;
	WheelInput steer; steer.valid = true; steer.steer = -0.8; steer.clutch = 1.0;
	auto sources = customMapper.ActiveSources(steer, custom);
	pad = Mapper::BuildPad(steer, sources, kLayerMaskMenu, custom);
	CHECK(pad.buttons == (kBitLeft | kBitWiiUY) && pad.leftX == 0.0f);
	custom.invertSteering = true;
	customMapper.Reset();
	sources = customMapper.ActiveSources(steer, custom);
	pad = Mapper::BuildPad(steer, sources, kLayerMaskMenu, custom);
	CHECK(pad.buttons == (kBitRight | kBitWiiUY));

	// Out-of-range stored values are repaired, never trusted.
	Config broken = DefaultConfig();
	broken.player = 9; broken.layerMode = 7; broken.toggleSource = 999; broken.uiNavMode = -4;
	broken.pedalThreshold = 5.0; broken.steerSaturation = 0.0; broken.steerDeadzone = -1.0;
	broken.coreDeadzoneCompensation = std::numeric_limits<double>::quiet_NaN();
	broken.bindings[LayerRace][TargetA] = -3;
	broken.bindings[LayerMenu][TargetB] = SourceCount;
	Sanitize(broken);
	CHECK(broken.player == 3 && broken.layerMode == 0 && broken.toggleSource == SourceButtonY);
	CHECK(broken.uiNavMode == 0 && broken.pedalThreshold == 0.90 && broken.steerSaturation == 0.20);
	CHECK(broken.steerDeadzone == 0.0 && broken.coreDeadzoneCompensation == 0.0);
	CHECK(broken.bindings[LayerRace][TargetA] == SourceNone && broken.bindings[LayerMenu][TargetB] == SourceNone);
}

void TestWheelOnly()
{
	Host host;
	host.PlugWheel();
	CHECK(host.slots.reserved == 0);
	host.gameRunning = true;
	host.wheelInput.throttle = 1.0;
	host.wheelInput.steer = 0.7;
	auto out = host.Publish();
	CHECK(ConnectedPlayers(out) == 1);
	CHECK(out[0].connected && out[0].buttons == kBitWiiUA && out[0].leftX == 1.0f);
	CHECK(!out[1].connected && !out[2].connected && !out[3].connected);
	for (int player = 1; player < kMaxPlayers; ++player)
		CHECK(out[player].buttons == 0 && out[player].leftX == 0.0f);
	CheckInvariants(host.slots);
}

void TestWheelPlusGamepads()
{
	// Wheel is player 1, three gamepads become players 2-4.
	Host host;
	host.PlugWheel();
	CHECK(host.PlugPad(L"padA") == 1);
	CHECK(host.PlugPad(L"padB") == 2);
	CHECK(host.PlugPad(L"padC") == 3);
	CHECK(host.PlugPad(L"padD") == -1); // no fifth player, and never the wheel's slot
	host.gameRunning = true;
	host.wheelInput.throttle = 1.0;
	host.wheelInput.steer = -1.0;
	host.pads[1].buttons = kBitWiiUX; host.pads[1].leftX = 0.5f;
	host.pads[2].buttons = kBitPlus;
	host.pads[3].buttons = 0;
	auto out = host.Publish();
	CHECK(ConnectedPlayers(out) == 4);
	CHECK(out[0].buttons == kBitWiiUA && out[0].leftX == -1.0f);  // wheel only
	CHECK(out[1].buttons == kBitWiiUX && out[1].leftX == 0.5f);   // padA only
	CHECK(out[2].buttons == kBitPlus && out[2].leftX == 0.0f);    // padB only
	CHECK(out[3].buttons == 0 && out[3].leftX == 0.0f);           // padC only
	CheckInvariants(host.slots);

	// Releasing the wheel changes nobody else.
	host.wheelInput = {};
	auto idle = host.Publish();
	CHECK(idle[0].connected && idle[0].buttons == 0 && idle[0].leftX == 0.0f);
	CHECK(idle[1].buttons == out[1].buttons && idle[2].buttons == out[2].buttons);

	// Unplugging the wheel mid-race releases exactly its player.
	host.wheelInput.throttle = 1.0;
	host.UnplugWheel();
	auto unplugged = host.Publish();
	CHECK(!unplugged[0].connected && unplugged[0].buttons == 0 && unplugged[0].leftX == 0.0f);
	CHECK(unplugged[1].connected && unplugged[1].buttons == kBitWiiUX && unplugged[1].leftX == 0.5f);
	CHECK(unplugged[2].connected && unplugged[3].connected);
	CHECK(host.slots.reserved == 0);
	CHECK(host.slots.FindIdentity(L"padA") == 1 && host.slots.FindIdentity(L"padB") == 2 &&
		host.slots.FindIdentity(L"padC") == 3);
	// A gamepad arriving while the wheel is away cannot become the wheel's player.
	CHECK(host.PlugPad(L"padD") == -1);
	// Reconnect: same player, clean state.
	host.PlugWheel();
	CHECK(host.slots.reserved == 0);
	auto back = host.Publish();
	CHECK(back[0].connected && back[0].buttons == 0);
	CHECK(back[1].buttons == kBitWiiUX);
	CheckInvariants(host.slots);
}

void TestWheelAsPlayerTwo()
{
	// Wheel first, then a gamepad.
	{
		Host host;
		host.config.player = 1;
		host.PlugWheel();
		CHECK(host.slots.reserved == 1);
		CHECK(host.PlugPad(L"padA") == 0);
		CHECK(host.PlugPad(L"padB") == 2);
		CHECK(host.PlugPad(L"padC") == 3);
		host.gameRunning = true;
		host.wheelInput.brake = 1.0;
		host.pads[0].buttons = kBitWiiUA;
		auto out = host.Publish();
		CHECK(out[0].buttons == kBitWiiUA && out[1].buttons == kBitWiiUB);
		CHECK(ConnectedPlayers(out) == 4);
		CheckInvariants(host.slots);
	}
	// Gamepads first (they occupy players 1 and 2), then the wheel appears.
	{
		Host host;
		host.config.player = 1;
		CHECK(host.PlugPad(L"padA") == 0);
		CHECK(host.PlugPad(L"padB") == 1);
		host.pads[1].buttons = kBitWiiUX;
		host.PlugWheel();
		CHECK(host.slots.reserved == 1);
		CHECK(host.slots.FindIdentity(L"padA") == 0); // player 1 untouched
		CHECK(host.slots.FindIdentity(L"padB") == 2); // moved, not merged
		host.wheelInput.throttle = 1.0;
		auto out = host.Publish();
		CHECK(out[1].buttons == kBitWiiUA);  // wheel
		CHECK(out[2].buttons == kBitWiiUX);  // padB kept its own input
		CHECK(ConnectedPlayers(out) == 3);
		CheckInvariants(host.slots);
	}
}

void TestClaimDeferredWhileRunning()
{
	Host host;
	CHECK(host.PlugPad(L"padA") == 0);
	host.gameRunning = true;
	host.pads[0].buttons = kBitWiiUA;
	host.PlugWheel(); // first seen mid-game while a gamepad is player 1
	CHECK(host.slots.reserved == -1);
	host.wheelInput.throttle = 1.0; host.wheelInput.steer = 1.0;
	auto out = host.Publish();
	CHECK(ConnectedPlayers(out) == 1);
	CHECK(out[0].buttons == kBitWiiUA && out[0].leftX == 0.0f); // gamepad only; wheel leaks nothing
	// Once the game stops the wheel takes player 1 and the gamepad moves to player 2.
	host.gameRunning = false;
	host.UpdateClaim();
	CHECK(host.slots.reserved == 0 && host.slots.FindIdentity(L"padA") == 1);
	CheckInvariants(host.slots);

	// A free configured slot is claimed immediately, even mid-game.
	Host free;
	free.config.player = 2;
	free.PlugPad(L"padA");
	free.gameRunning = true;
	free.PlugWheel();
	CHECK(free.slots.reserved == 2 && free.slots.FindIdentity(L"padA") == 0);
}

void TestGamepadHotplugStability()
{
	Host host;
	host.PlugWheel();
	host.PlugPad(L"padA"); host.PlugPad(L"padB"); host.PlugPad(L"padC");
	host.UnplugPad(L"padB");
	CHECK(host.slots.FindIdentity(L"padB") == 2 && !host.slots.connected[2]);
	auto out = host.Publish();
	CHECK(!out[2].connected && out[1].connected && out[3].connected && out[0].connected);
	CHECK(host.PlugPad(L"padB") == 2);     // returns to its own player
	host.UnplugPad(L"padA");
	host.UnplugPad(L"padC");
	CHECK(host.PlugPad(L"padC") == 3);
	CHECK(host.PlugPad(L"padNew") == 1);   // replaces the oldest unplugged pad, never the wheel
	CHECK(host.slots.reserved == 0);
	CHECK(host.PlugPad(L"padA") == -1);    // all four players are in use
	CheckInvariants(host.slots);

	// Pads without an identity still get exactly one slot each.
	Host anonymous;
	anonymous.PlugWheel();
	CHECK(anonymous.slots.Assign(L"", L"").slot == 1);
	CHECK(anonymous.slots.Assign(L"", L"").slot == 2);
	anonymous.slots.Disconnect(1);
	CHECK(anonymous.slots.Assign(L"", L"").slot == 1);
	CheckInvariants(anonymous.slots);
}

void TestNoDuplicateWheel()
{
	// The system projects the wheel as a gamepad too: still exactly one player.
	Host host;
	host.PlugWheel();
	CHECK(host.PlugPad(host.wheelId) == -1);
	CHECK(ConnectedPlayers(host.Publish()) == 1);

	// The gamepad projection is seen before the wheel itself.
	Host late;
	CHECK(late.PlugPad(late.wheelId) == 0);
	CHECK(late.PlugPad(L"padA") == 1);
	late.PlugWheel();
	CHECK(late.slots.reserved == 0);
	CHECK(late.slots.FindIdentity(late.wheelId) == -1);
	CHECK(late.slots.FindIdentity(L"padA") == 1);
	CHECK(ConnectedPlayers(late.Publish()) == 2);
	CheckInvariants(late.slots);
}

void TestChangeWheelPlayer()
{
	Host host;
	host.PlugWheel();
	host.PlugPad(L"padA"); host.PlugPad(L"padB");
	host.pads[2].buttons = kBitWiiUY;
	host.config.player = 2;   // user moves the wheel to player 3
	host.UpdateClaim();
	CHECK(host.slots.reserved == 2);
	CHECK(host.slots.FindIdentity(L"padA") == 1); // unaffected
	CHECK(host.slots.FindIdentity(L"padB") == 0); // swapped with the wheel
	auto out = host.Publish();
	CHECK(out[0].buttons == kBitWiiUY && out[2].connected && out[2].buttons == 0);
	CheckInvariants(host.slots);

	// Switching wheel support off returns its slot to gamepads.
	host.config.enabled = false;
	host.UpdateClaim();
	CHECK(host.slots.reserved == -1);
	CHECK(host.PlugPad(L"padC") == 2);
	auto disabled = host.Publish();
	CHECK(ConnectedPlayers(disabled) == 3);
	host.wheelInput.throttle = 1.0;
	disabled = host.Publish();
	CHECK(disabled[0].buttons == kBitWiiUY && disabled[1].buttons == 0 && disabled[2].buttons == 0);
}

void TestFiveDevices()
{
	// Four gamepads are already playing; the wheel shows up.
	Host host;
	host.PlugPad(L"p1"); host.PlugPad(L"p2"); host.PlugPad(L"p3"); host.PlugPad(L"p4");
	host.gameRunning = true;
	host.PlugWheel();
	CHECK(host.slots.reserved == -1); // nothing changes mid-game
	CHECK(ConnectedPlayers(host.Publish()) == 4);
	host.gameRunning = false;
	host.UpdateClaim();
	CHECK(host.slots.reserved == 0);
	CHECK(host.slots.FindIdentity(L"p1") == -1); // no room left: dropped, not doubled up
	CHECK(ConnectedPlayers(host.Publish()) == 4);
	CheckInvariants(host.slots);
}

void TestUiNavigator()
{
	using Nav = UiNavigator;
	// Host mode: immediate edges, auto-repeat on directions only.
	{
		Nav nav;
		CHECK(nav.Update(0, 0, Nav::ModeHost) == 0);
		CHECK(nav.Update(Nav::ActionDown, 10, Nav::ModeHost) == Nav::ActionDown);
		CHECK(nav.Update(Nav::ActionDown, 26, Nav::ModeHost) == 0);
		CHECK(nav.Update(Nav::ActionDown, 459, Nav::ModeHost) == 0);
		CHECK(nav.Update(Nav::ActionDown, 460, Nav::ModeHost) == Nav::ActionDown);
		CHECK(nav.Update(Nav::ActionDown, 500, Nav::ModeHost) == 0);
		CHECK(nav.Update(Nav::ActionDown, 590, Nav::ModeHost) == Nav::ActionDown);
		CHECK(nav.Update(0, 600, Nav::ModeHost) == 0);
		CHECK(nav.Update(Nav::ActionAccept, 700, Nav::ModeHost) == Nav::ActionAccept);
		CHECK(nav.Update(Nav::ActionAccept, 2000, Nav::ModeHost) == 0); // confirm never repeats
		CHECK(nav.Update(0, 2010, Nav::ModeHost) == 0);
		CHECK(nav.Update(Nav::ActionAccept | Nav::ActionLeft, 2100, Nav::ModeHost) ==
			(Nav::ActionAccept | Nav::ActionLeft));
	}
	// Auto mode, system silent: the host acts after the wait.
	{
		Nav nav;
		CHECK(nav.Update(Nav::ActionRight, 1000, Nav::ModeAuto) == 0);
		CHECK(nav.Update(Nav::ActionRight, 1050, Nav::ModeAuto) == 0);
		CHECK(nav.Update(Nav::ActionRight, 1090, Nav::ModeAuto) == Nav::ActionRight);
		CHECK(nav.Update(Nav::ActionRight, 1300, Nav::ModeAuto) == 0);
		CHECK(nav.Update(Nav::ActionRight, 1450, Nav::ModeAuto) == Nav::ActionRight); // repeat
		CHECK(nav.Update(0, 1500, Nav::ModeAuto) == 0);
		// A tap shorter than the wait still counts once.
		CHECK(nav.Update(Nav::ActionAccept, 2000, Nav::ModeAuto) == 0);
		CHECK(nav.Update(0, 2030, Nav::ModeAuto) == 0);
		CHECK(nav.Update(0, 2090, Nav::ModeAuto) == Nav::ActionAccept);
		CHECK(nav.Update(0, 2200, Nav::ModeAuto) == 0);
	}
	// Auto mode, system already navigated: the host stays out, repeats included.
	{
		Nav nav;
		CHECK(nav.Update(Nav::ActionDown, 1000, Nav::ModeAuto) == 0);
		nav.OnSystemKey(Nav::ActionDown, 1012);
		for (uint64_t time = 1016; time < 3000; time += 16)
			CHECK(nav.Update(Nav::ActionDown, time, Nav::ModeAuto) == 0);
		CHECK(nav.Update(0, 3000, Nav::ModeAuto) == 0);
		// The projected key may arrive slightly before the polled reading.
		nav.OnSystemKey(Nav::ActionAccept, 3990);
		CHECK(nav.Update(Nav::ActionAccept, 4000, Nav::ModeAuto) == 0);
		CHECK(nav.Update(Nav::ActionAccept, 4100, Nav::ModeAuto) == 0);
		CHECK(nav.Update(0, 4200, Nav::ModeAuto) == 0);
		// An old key from another device does not swallow a later wheel press.
		CHECK(nav.Update(Nav::ActionAccept, 9000, Nav::ModeAuto) == 0);
		CHECK(nav.Update(Nav::ActionAccept, 9090, Nav::ModeAuto) == Nav::ActionAccept);
		// A key for a different action is irrelevant.
		nav.Update(0, 9500, Nav::ModeAuto);
		CHECK(nav.Update(Nav::ActionLeft, 10000, Nav::ModeAuto) == 0);
		nav.OnSystemKey(Nav::ActionRight, 10010);
		CHECK(nav.Update(Nav::ActionLeft, 10090, Nav::ModeAuto) == Nav::ActionLeft);
	}
	// System mode: never.
	{
		Nav nav;
		for (uint64_t time = 0; time < 2000; time += 16)
			CHECK(nav.Update(Nav::ActionDown | Nav::ActionAccept, time, Nav::ModeSystem) == 0);
	}
	CHECK(UiNavigator::ActionsFromNav(kNavUp | kNavAccept, kWheelBitDpadLeft | kWheelBitPaddleLeft) ==
		(Nav::ActionUp | Nav::ActionAccept | Nav::ActionLeft));
	CHECK(UiNavigator::ActionsFromNav(kNavMenu | kNavView | kNavContext1, kWheelBitButton1) == 0);
}

void TestRandomized()
{
	std::mt19937 random(98);
	const std::vector<std::wstring> ids = { L"a", L"b", L"c", L"d", L"e", L"f" };
	for (int round = 0; round < 400; ++round)
	{
		Host host;
		host.config.player = static_cast<int32_t>(random() % 4);
		for (int step = 0; step < 120; ++step)
		{
			const auto before = host.slots;
			const int wheelSlotBefore = host.slots.reserved;
			switch (random() % 9)
			{
			case 0: host.PlugWheel(); break;
			case 1: host.UnplugWheel(); break;
			case 2: case 3: case 4:
			{
				const auto& id = ids[random() % ids.size()];
				const bool known = host.slots.FindIdentity(id) >= 0;
				const int previous = host.slots.FindIdentity(id);
				const int slot = host.PlugPad(id);
				if (known)
					CHECK(slot == previous); // a returning pad never changes player
				if (slot >= 0)
				{
					CHECK(slot != host.slots.reserved);
					// Nobody else moved.
					for (int index = 0; index < kMaxPlayers; ++index)
						if (index != slot && before.connected[index])
							CHECK(host.slots.connected[index] && host.slots.ids[index] == before.ids[index]);
				}
				break;
			}
			case 5: case 6:
			{
				const auto& id = ids[random() % ids.size()];
				host.UnplugPad(id);
				for (int index = 0; index < kMaxPlayers; ++index)
					if (before.connected[index] && before.ids[index] != id)
						CHECK(host.slots.connected[index] && host.slots.ids[index] == before.ids[index]);
				break;
			}
			case 7: host.gameRunning = !host.gameRunning; host.UpdateClaim(); break;
			case 8:
				if (!host.gameRunning)
				{
					host.config.player = static_cast<int32_t>(random() % 4);
					host.UpdateClaim();
				}
				break;
			}
			CheckInvariants(host.slots);
			// While a game runs, the wheel never takes a slot from a connected pad.
			if (host.gameRunning && host.slots.reserved != wheelSlotBefore && host.slots.reserved >= 0)
				CHECK(!before.connected[host.slots.reserved]);

			host.wheelInput.valid = true;
			host.wheelInput.throttle = (random() % 2) ? 1.0 : 0.0;
			host.wheelInput.steer = (static_cast<int>(random() % 201) - 100) / 100.0;
			for (auto& pad : host.pads)
				pad = {};
			const auto out = host.Publish();
			for (int player = 0; player < kMaxPlayers; ++player)
			{
				const bool isWheel = player == host.slots.reserved;
				if (isWheel)
					CHECK(out[player].connected == host.wheelPresent);
				else
				{
					// Gamepads are idle here, so any activity would be wheel leakage.
					CHECK(out[player].connected == host.slots.connected[player]);
					CHECK(out[player].buttons == 0 && out[player].leftX == 0.0f);
				}
			}
			CHECK(ConnectedPlayers(out) <= kMaxPlayers);
		}
	}
}
}

int main()
{
	TestDefaultMapping();
	TestPedalHysteresis();
	TestLayers();
	TestWheelOnly();
	TestWheelPlusGamepads();
	TestWheelAsPlayerTwo();
	TestClaimDeferredWhileRunning();
	TestGamepadHotplugStability();
	TestNoDuplicateWheel();
	TestChangeWheelPlayer();
	TestFiveDevices();
	TestUiNavigator();
	TestRandomized();
	std::printf("%d checks, %d failures\n", g_checks, g_failures);
	if (g_failures == 0)
		std::printf("PASS: simulated T98 bridge logic (no hardware involved)\n");
	return g_failures == 0 ? 0 : 1;
}
