#pragma once

#include "DirectXPage.g.h"
#include "Cemu_UWP_HostMain.h"
#include "Common/DeviceResources.h"
#include "T98Bridge.h"
#include <array>
#include <chrono>
#include <string>

namespace Cemu_UWP_Host
{
	public ref class GraphicPackViewModel sealed
	{
	public:
		property Platform::String^ Identity;
		property Platform::String^ Name;
		property Platform::String^ Category;
		property Platform::String^ Description;
		property bool Enabled;
	};

	public ref class DirectXPage sealed
	{
	public:
		DirectXPage();
		virtual ~DirectXPage();
		void SaveInternalState(Windows::Foundation::Collections::IPropertySet^ state);
		void LoadInternalState(Windows::Foundation::Collections::IPropertySet^ state);
		void ShutdownRuntime();
		void ResumeRuntime();

	private:
		void ConfigureXboxUiScale();
		void Page_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ args);
		void InitializeEmulator(float width, float height);
		void UpdateEmulatorSurfaceSize(float width, float height);
		void OnRendering(Platform::Object^ sender, Platform::Object^ args);
		void OnGamepadAdded(Platform::Object^ sender, Windows::Gaming::Input::Gamepad^ gamepad);
		void OnGamepadRemoved(Platform::Object^ sender, Windows::Gaming::Input::Gamepad^ gamepad);
		// T98_BRIDGE_V2: one racing wheel acts as one independent player.
		void OnRacingWheelAdded(Platform::Object^ sender, Windows::Gaming::Input::RacingWheel^ wheel);
		void OnRacingWheelRemoved(Platform::Object^ sender, Windows::Gaming::Input::RacingWheel^ wheel);
		void WheelSelectionChanged(Platform::Object^ sender,
			Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ args);
		void WheelCheckChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void WheelValueChanged(Platform::Object^ sender,
			Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ args);
		void ResetWheelBindings_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void OnCoreWindowKeyDown(Windows::UI::Core::CoreWindow^ sender,
			Windows::UI::Core::KeyEventArgs^ args);
		void OnBackRequested(Platform::Object^ sender,
			Windows::UI::Core::BackRequestedEventArgs^ args);
		void InstallContent_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void ImportKeys_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void DownloadGraphicPacks_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void InstallGraphicPacks_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void GraphicPackGame_SelectionChanged(Platform::Object^ sender,
			Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ args);
		void GraphicPack_Toggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void ClearShaderCache_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void RefreshLibrary_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void ScanExternalStorage_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void RescanExternalStorage_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void ForgetExternalStorage_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void InstalledGames_SelectionChanged(Platform::Object^ sender,
			Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ args);
		void InstalledGames_ItemClick(Platform::Object^ sender,
			Windows::UI::Xaml::Controls::ItemClickEventArgs^ args);
		void StartGame_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void PerformanceMetricsCheckChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void NavigationButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void ToolTabs_SelectionChanged(Platform::Object^ sender,
			Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ args);
		void SettingsSelectionChanged(Platform::Object^ sender,
			Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ args);
		void SettingsCheckChanged(Platform::Object^ sender,
			Windows::UI::Xaml::RoutedEventArgs^ args);
		void SettingsValueChanged(Platform::Object^ sender,
			Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ args);
		void PlaceDimensionsFigure_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void RemoveDimensionsFigure_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void MoveDimensionsFigure_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void ClearErrors_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void EmulatorViewport_PointerPressed(Platform::Object^ sender,
			Windows::UI::Xaml::Input::PointerRoutedEventArgs^ args);
		void EmulatorViewport_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ args);
		void GraphicPacksList_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ args);
		void EmulatorSurface_CompositionScaleChanged(
			Windows::UI::Xaml::Controls::SwapChainPanel^ sender,
			Platform::Object^ args);
		void FocusEmulatorInput();
		void SetSystemPointerForUi(bool enabled);
		void SetTabsVisible(bool visible);
		void SetGamePresentation(bool running);
		void SetExternalLoadingVisible(bool visible);
		void AppendError(const std::string& message);
		void OnCemuStateChanged(CemuEmbedState state);
		void OnBrokeredProgress(uint64_t bytesCopied, uint64_t totalBytes, const std::string& relativePath);
		void BeginInstall();
		void BeginExternalLaunch(std::function<bool()> launchOperation);
		void BeginExternalStorageScan(const std::vector<Windows::Storage::StorageFolder^>& storageRoots);
		void RememberExternalStorageFolder(Windows::Storage::StorageFolder^ storageRoot);
		void RestoreExternalStorageFolders(bool showEmptyStatus = false);
		void ForgetExternalStorageFolders();
		void RefreshLibrary(bool scanLocalFolder = false);
		void DeleteInstalledTitle(uint64_t titleId);
		void SetLibraryActionsEnabled(bool enabled);
		void UpdateStartButton();
		void UpdateSelectedShaderCount();
		int FindInstalledTitleIndex(uint64_t titleId) const;
		int FindGamepadSlot(Windows::Gaming::Input::Gamepad^ gamepad) const;
		std::wstring GetGamepadIdentity(Windows::Gaming::Input::Gamepad^ gamepad) const;
		std::wstring GetGamepadDisplayName(Windows::Gaming::Input::Gamepad^ gamepad) const;
		int AssignGamepadSlot(Windows::Gaming::Input::Gamepad^ gamepad);
		void DisconnectGamepadSlot(size_t slot);
		void RefreshGamepads();
		void PublishSlotDisconnected(size_t slot);
		void InitializeWheelBridge();
		void LoadWheelSettings();
		void SaveWheelSettings();
		void ApplyWheelConfigToUi();
		void WheelSettingsChanged();
		void RefreshRacingWheel();
		void AttachWheel(Windows::Gaming::Input::RacingWheel^ wheel);
		void DetachWheel(const wchar_t* reason, int hresult);
		void UpdateWheelClaim();
		T98::WheelInput ReadWheelInput();
		void UpdateWheelStatus();
		void UpdateWheelMonitor();
		void UpdateWheelUiNavigation();
		void ExecuteWheelUiAction(uint32_t action);
		void CommitInstalledGameIndex(unsigned int index);
		void ShowWheelToast(Platform::String^ text);
		void WheelLog(const std::wstring& message);
		void UpdateGamepadStatus();
		void InitializeGlobalGamepadMappingUi();
		void LoadGlobalGamepadMapping();
		void SaveGlobalGamepadMapping();
		std::array<Windows::UI::Xaml::Controls::ComboBox^, 16> GetGlobalGamepadBindingBoxes() const;
		CemuEmbedGamepadState ApplyGlobalGamepadMapping(const CemuEmbedGamepadState& raw) const;
		void ResetGamepadMapping_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void UpdateGamepadVibration();
		void StopGamepadVibration(Windows::Gaming::Input::Gamepad^ gamepad);
		CemuEmbedGamepadState PublishGamepadStates();
		void UpdateActiveAccount();
		void RefreshDimensionsFigures();
		void RefreshGraphicPackGames();
		void RefreshGraphicPacksForSelectedGame();
		void UpdateGraphicPackGridColumns();
		void LoadSettings();
		void SaveSettings();
		void TryConfigureDefaultGamepad();
		void UpdateVirtualMouse(const CemuEmbedGamepadState& gamepad);
		void SetVirtualMouseEnabled(bool enabled);
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<Cemu_UWP_HostMain> m_main;
		std::vector<InstalledTitle> m_installedTitles;
		std::vector<InstalledTitle> m_externalTitles;
		std::vector<DimensionsFigure> m_dimensionsFigures;
		std::vector<uint64_t> m_graphicPackGameIds;
		Windows::Foundation::EventRegistrationToken m_renderingToken{};
		Windows::Foundation::EventRegistrationToken m_gamepadAddedToken{};
		Windows::Foundation::EventRegistrationToken m_gamepadRemovedToken{};
		Windows::Foundation::EventRegistrationToken m_wheelAddedToken{};
		Windows::Foundation::EventRegistrationToken m_wheelRemovedToken{};
		Windows::Foundation::EventRegistrationToken m_coreWindowKeyDownToken{};
		Windows::Foundation::EventRegistrationToken m_coreWindowKeyUpToken{};
		Windows::Foundation::EventRegistrationToken m_backRequestedToken{};
		// Keep WinRT controllers discovered on the XAML apartment. Querying
		// Gamepad::Gamepads on every composition frame re-enters Xbox PnP/user
		// association code and can produce E_INVALIDARG/E_ACCESSDENIED failures.
		std::array<Windows::Gaming::Input::Gamepad^, CEMU_EMBED_MAX_GAMEPADS> m_gamepads{};
		// T98_BRIDGE_V2: player-slot ownership lives in the platform-independent
		// table so the wheel's slot can never be shared with a gamepad.
		// m_gamepads[i] is non-null exactly when m_slots.connected[i] is true.
		T98::SlotTable m_slots;
		// The wheel, like the gamepads, is only touched on the XAML apartment.
		Windows::Gaming::Input::RacingWheel^ m_wheel = nullptr;
		Windows::Gaming::Input::UINavigationController^ m_wheelNav = nullptr;
		std::wstring m_wheelId;
		std::wstring m_wheelName;
		T98::Config m_wheelConfig;
		T98::Mapper m_wheelMapper;
		T98::UiNavigator m_wheelUiNavigator;
		T98::WheelInput m_lastWheelInput;
		T98::PadState m_lastWheelPad;
		uint64_t m_lastWheelSources = 0;
		std::array<std::array<Windows::UI::Xaml::Controls::ComboBox^, T98::kTargetCount>, T98::kLayerCount>
			m_wheelBindingBoxes{};
		bool m_wheelSeen = false;
		bool m_wheelUiReady = false;
		bool m_loadingWheelSettings = false;
		bool m_wheelClaimDeferredLogged = false;
		bool m_wheelHostNavigationLogged = false;
		uint64_t m_wheelToastUntilMs = 0;
		unsigned int m_wheelMonitorFrames = 0;
		Windows::UI::Core::CoreCursor^ m_savedSystemPointerCursor = nullptr;
		bool m_systemPointerHidden = false;
		bool m_cemuReady = false;
		bool m_libraryBusy = false;
		bool m_gamepadProfileReady = false;
		bool m_gameRunning = false;
		bool m_externalLoadingVisible = false;
		bool m_virtualMouseEnabled = false;
		bool m_virtualMouseChordHeld = false;
		bool m_optionsChordHeld = false;
		bool m_virtualMouseLeftDown = false;
		bool m_performanceMetricsVisible = false;
		bool m_loadingSettings = false;
		bool m_loadingGraphicPacks = false;
		bool m_runtimeSuspended = false;
		uint64_t m_selectedTitleId = 0;
		bool m_restoringCommittedSelection = false;
		// Xbox input objects belong to the XAML apartment. Keep one snapshot per
		// player and send it to the DLL only when that player's state changed.
		std::array<CemuEmbedGamepadState, CEMU_EMBED_MAX_GAMEPADS> m_lastPublishedGamepadStates{};
		std::array<bool, CEMU_EMBED_MAX_GAMEPADS> m_hasPublishedGamepadStates{};
		std::array<float, CEMU_EMBED_MAX_GAMEPADS> m_lastAppliedGamepadRumble{};
		std::array<int32_t, 16> m_globalGamepadBindings{
			1, 0, 3, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
		double m_virtualMouseX = 0.0;
		double m_virtualMouseY = 0.0;
		std::chrono::steady_clock::time_point m_virtualMouseLastUpdate{};
		unsigned int m_gamepadRetryFrames = 0;
		unsigned int m_controllerPollFrames = 0;
	};
}
