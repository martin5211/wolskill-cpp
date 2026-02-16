# WolSkill-cpp

A native Win32 system tray application that maintains a persistent WebSocket connection to an AWS backend for Wake-on-LAN using Alexa. It sends periodic heartbeats and MAC address data, so it can receive remote shutdown commands. This is a C++ port of [wolskill-cmd](https://github.com/oscarpenelo/wolskill-cmd). A license can be obtained from [https://www.wolskill.com/](https://www.wolskill.com/).

## Features

- **System tray operation** - Runs silently in the background with a colored tray icon (green = connected, red = disconnected)
- **WebSocket with auto-reconnect** - Connects to the AWS API Gateway endpoint and automatically reconnects on failures
- **Heartbeat & MAC reporting** - Sends all network adapter MAC/IP addresses every 30 seconds; 40-second heartbeat timeout triggers reconnection
- **Remote shutdown** - Responds to server commands matching a local MAC address by initiating system shutdown
- **Registry-persisted settings** - AWS Instance ID and License are stored in `HKCU\SOFTWARE\WolSkill` and loaded automatically on startup
- **Run on startup** - Optional auto-start via `HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run`, toggled from the tray menu
- **Windows dark mode**
- **Single instance** - A global mutex prevents duplicate instances
- **MSIX packaging** - Includes a Windows Application Packaging Project for modern distribution
- **Zero external dependencies** - Uses only Win32 APIs (WinHTTP, IP Helper, DWM, UxTheme, Shell)

## Requirements

- Windows 10 version 1903 or later
- Visual Studio 2026 (v145 toolset)
- Windows 10 SDK (10.0.26100.0 or later)

## Building

Open `WolSkill-cpp.sln` in Visual Studio and build the **WolSkill-cpp** project, or from a Developer Command Prompt:

```
msbuild WolSkill-cpp\WolSkill-cpp.vcxproj -p:Configuration=Release -p:Platform=x64
```

The output is placed in `bin\Release\x64\WolSkill-cpp.exe`.

To build the MSIX package, right-click the project in Visual Studio and select **Publish** > **Create App Packages**.

## Usage

1. Launch `WolSkill-cpp.exe`. It starts minimized to the system tray.
2. Right-click the tray icon to open the context menu:
   - **Connected / Disconnected** - Current connection status (read-only)
   - **Settings...** - Opens a dialog to enter the AWS Instance ID and License
   - **Run on startup** - Checked when auto-start is enabled; click to toggle
   - **Exit** - Closes the application
3. Double-click the tray icon to open Settings directly.
4. After entering valid credentials and clicking OK, the application connects to the AWS WebSocket endpoint and begins sending heartbeats.

## Project Structure

```
WolSkill-cpp.sln                    Solution file
WolSkill-cpp/
  main.cpp                          Entry point, message loop, system tray, settings dialog
  WebSocketClient.h/.cpp            WinHTTP WebSocket client with auto-reconnect
  Settings.h/.cpp                   Registry persistence and startup management
  NetworkInfo.h/.cpp                MAC/IP address enumeration (IP Helper API)
  ThemeHelper.h/.cpp                Dark/light mode detection and application
  resource.h                        Resource identifiers
  WolSkill.rc                       Dialog template, version info, icon resource
  WolSkill.ico                      Application icon
  app.manifest                      DPI awareness, common controls v6
  Package.appxmanifest              Package identity, startup task, capabilities
  Images/                           Store and tile logo assets
```
