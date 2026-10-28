<p align="center"><img src=".github/assets/banner.png" alt="ClickThroughGuard" width="100%"></p>

<p align="center">
  <img src="https://img.shields.io/badge/Rainmeter-4.5%2B-1E90FF?style=flat-square" alt="Rainmeter 4.5+">
  <img src="https://img.shields.io/badge/License-GPL--3.0-blue?style=flat-square" alt="License">
  <a href="https://github.com/SteelArchadian/ClickThroughGuard/releases/latest"><img src="https://img.shields.io/github/v/release/SteelArchadian/ClickThroughGuard?style=flat-square&label=Download" alt="Download"></a>
</p>

**ClickThroughGuard** makes all your Rainmeter skins fully click-through while a game or other fullscreen window is in front, so a click meant for the game never lands on a skin.

## What it does

- **Watches the window in front.** When a fullscreen or borderless window (a game, a fullscreen video) is on top, every skin becomes click-through. When you leave it, skins work normally again.
- **Holds on.** If Rainmeter tries to undo click-through (for example when a skin refreshes), it's put back right away, so there's never a moment when a click can land on a skin.
- **Covers all skins at once.** You don't have to change any of your other skins.
- **Easy to turn off:** unload the ClickThroughGuard skin and everything is back to normal.

## Settings

Open `ClickThroughGuard.ini`, change the values under `[Variables]`, then refresh the skin:

| Setting | Default | What it does |
|---|---|---|
| `Mode` | `Fullscreen` | `Fullscreen`: only while something fullscreen is in front. `Always`: all the time |
| `Exclude` | *(empty)* | Skins to leave alone, separated by `\|`, e.g. `NetDot X\|Mond\Settings` |

## The plugin

ClickThroughGuard uses a small plugin, `ClickThroughGuard.dll`. The `.rmskin` installs it for you. Its source is in [`plugin/`](plugin). To build it yourself, run `plugin/build.bat` with the Visual Studio 2022 Build Tools installed.

## Install

Download the `.rmskin` file from **[Releases](https://github.com/SteelArchadian/ClickThroughGuard/releases/latest)** and double-click it. Rainmeter installs and loads it.

## Credits and license

Made by SteelArchadian, under the [GNU GPL v3.0](LICENSE).

Made with AI assistance.

## Bugs and ideas

Post them on [GitHub Issues](https://github.com/SteelArchadian/ClickThroughGuard/issues).
