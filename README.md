# DualTimeClockTray

A lightweight Windows system tray application that displays a second clock in any timezone, designed for Windows 7, 8, 10, and 11.
<div style="">
  <img alt="world dual clock in windows system tray" src="https://raw.githubusercontent.com/Eb43/DualTimeClockTray/refs/heads/main/images/Clipboard01.jpg" style="width:300px; display: inline-block; margin-left:30px;"/>
  <img alt="world dual clock in windows system tray" src="https://raw.githubusercontent.com/Eb43/DualTimeClockTray/refs/heads/main/images/Clipboard02.jpg" style="width:300px; display: inline-block; margin-left:30px;"/>
  <img alt="world dual clock in windows system tray" src="https://raw.githubusercontent.com/Eb43/DualTimeClockTray/refs/heads/main/images/Clipboard03.jpg" style="width:300px; display: inline-block; margin-left:30px;"/>
  </div>

  <div style="">
  <img alt="world dual clock in windows system tray" src="https://raw.githubusercontent.com/Eb43/DualTimeClockTray/refs/heads/main/images/win7.gif" style="width:300px; display: inline-block; margin-left:30px;"/>
  <img alt="world dual clock in windows system tray" src="https://raw.githubusercontent.com/Eb43/DualTimeClockTray/refs/heads/main/images/win10.gif" style="width:300px; display: inline-block; margin-left:30px;"/>
    <img alt="world dual clock in windows system tray" src="https://raw.githubusercontent.com/Eb43/DualTimeClockTray/refs/heads/main/images/VirtualBox_win11_13_07_2025_20_21_55.png" style="width:300px; display: inline-block; margin-left:30px;"/>
      </div>


## How Windows Dual Clock Works by Default

Windows provides a built-in dual clock feature that doez not allow to see dual time in a glance. OS allows you to display a second timezone when hovering over the system clock or clicking on it. This feature is accessible through:

1. **Windows 7-10**: Right-click the taskbar clock → "Adjust date/time" → "Add clocks for different time zones"
2. **Windows 11**: Click the clock → "Change date and time settings" → "Add clocks for different time zones"

This default implementation only shows the second clock when interacting with the system clock, making it inconvenient for users who need constant visibility of multiple timezones.

## Alternative Dual Clock Solutions

Several third-party applications exist to address this limitation:

- **TClockEx** - Customizes the system clock with additional timezone information
- **WorldClock Pro** - Desktop widget displaying multiple timezone clocks
- **Qlock** - Replaces the system clock with enhanced functionality
- **ClockGen** - Creates floating desktop clocks for different timezones

Most dual clock applications suffer from common drawbacks:

1. **Desktop widgets** consume screen real estate and can be hidden by windows
2. **System clock replacements** modify core Windows functionality, potentially causing compatibility issues on Windows Update
3. **Floating clocks** can interfere with other applications and user workflow

## Benefits of DualTimeClockTray

DualTimeClockTray takes a different approach by utilizing the system tray for second clock display:

- **Always visible** - Clock remains visible as long as system tray is visible
- **Minimal footprint** - Uses existing system tray space efficiently
- **Non-intrusive** - Doesn't modify Windows components or create floating windows
- **Lightweight** - Minimal resource usage with efficient icon management
- **Native integration** - Seamlessly integrates with Windows notification area

# Technical Challenge

Windows system tray icons have a fixed size (typically 16x16 pixels), making it impossible to display readable time text in a single icon. To solve this limitation, DualTimeClockTray employs multiple icons to display the time clearly.

### Windows Icon Hiding Problem

Windows automatically hides inactive system tray icons to reduce clutter. DualTimeClockTray implements two methods to overcome this behavior:

#### Method 1: Three Icons Strategy
- Uses three icons: one clock symbol + two time digits (hours and minutes)
- When Windows hides the first icon, the remaining two time icons stay visible
- Provides redundancy against Windows' automatic icon hiding

#### Method 2: Redraw Method
- Continuously destroys and recreates icons faster than Windows can hide them
- Prevents Windows from marking icons as "inactive"
- Updates every 30 seconds or when minutes change to maintain visibility

Users can switch between methods via the right-click context menu to find the best approach for their system.

## Compilation

The application is compiled using MSYS2 MINGW64 environment with the following command:

```bash
g++ main.cpp -static -static-libgcc -static-libstdc++ -mwindows -o DualTimeClockTray.exe
```

### Compilation Flags Explained:
- `-static` - Statically links all libraries
- `-static-libgcc` - Statically links GCC runtime library
- `-static-libstdc++` - Statically links C++ standard library
- `-mwindows` - Creates Windows GUI application (no console window)

## Windows Compatibility Measures

DualTimeClockTray is designed to work across Windows 7, 8, 10, and 11 through several compatibility measures:

### Windows Version Targeting
```cpp
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
```
- Targets Windows 7 (0x0601) as the minimum supported version
- Ensures compatibility with older Windows APIs

### Dynamic Function Loading
The application uses runtime dynamic linking for timezone functions:
- `EnumDynamicTimeZoneInformation` - Modern timezone enumeration
- `SystemTimeToTzSpecificLocalTimeEx` - Enhanced timezone conversion

If these functions aren't available (Windows 7), the app falls back to legacy APIs:
- `SystemTimeToTzSpecificLocalTime` - Legacy timezone conversion
- Hardcoded UTC timezone as ultimate fallback

### Registry Integration
- Uses `HKEY_CURRENT_USER` for settings storage (no admin privileges required)
- Stores timezone selection and icon method preferences
- Compatible with all Windows versions' registry structure

### Icon Management
- Uses `GetSystemMetrics(SM_CXICON)` for dynamic icon sizing
- Handles different DPI settings across Windows versions
- Implements proper icon cleanup to prevent resource leaks

## Features

- **Timezone Selection**: Right-click menu with comprehensive timezone list
- **Automatic Updates**: Clock updates every second (Three Icons) or every 30 seconds (Redraw)
- **Tooltip Information**: Detailed time, date, and timezone information on hover
- **Persistent Settings**: Remembers selected timezone and icon method between sessions
- **Minimal Resource Usage**: Efficient memory and CPU usage
- **No Dependencies**: Standalone executable with no external requirements

## Usage

1. Download and run [DualTimeClockTray.exe](https://github.com/Eb43/DualTimeClockTray/blob/main/DualTimeClockTray.exe)
2. Right-click any tray icon to access the context menu
3. Select your desired timezone from "Select Timezone"
4. Choose icon display method from "Icon Fix Method"
5. The application will remember your settings on relaunch
6. Drag and drop to move icons in system tray

## System Requirements

- Windows 7, 10, 11 
- Minimal RAM and CPU requirements
- No additional software dependencies

Special thanks to AI and companies who offer free subcription plans.
