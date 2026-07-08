# VanGram

A customized Telegram Desktop client, based on [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop).

[ English | [Русский](README-RU.md) ]

VanGram is a personal fork focused on multi-account management and tdata
synchronization across machines. It inherits all of AyuGram's features and is
built continuously for Windows x64 via GitHub Actions.

## Features

Inherited from AyuGram Desktop:

- Full ghost mode (flexible)
- Messages history & anti-recall
- Font customization
- Streamer mode
- Local Telegram Premium
- Translator
- Enhanced appearance

See the [AyuGram documentation](https://docs.ayugram.one/desktop/) for details on
the inherited feature set.

## Downloads

### Windows x64

Prebuilt Windows x64 binaries are produced by CI on every push to `main`. Grab
the latest artifact from the
[Actions tab](https://github.com/FindLyTen/VanGram/actions), or from the
[Releases](https://github.com/FindLyTen/VanGram/releases) page once published.

> The binary is still named `AyuGram.exe` for compatibility with existing tdata
> and the auto-updater. This is intentional.

### Self-built

Follow the [Windows x64 build guide](docs/building-win-x64.md) if you want to
compile VanGram yourself. macOS and Linux build guides are in
[docs/](docs/).

## Remarks for Windows

Make sure you have these components installed with VS Build Tools:

- C++ MFC latest (x86 & x64)
- C++ ATL latest (x86 & x64)
- latest Windows 11 SDK

## Credits

VanGram would not exist without these projects:

- [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop) — the direct upstream
- [Telegram Desktop](https://github.com/telegramdesktop/tdesktop)
- [64Gram](https://github.com/TDesktop-x64/tdesktop)
- [Kotatogram](https://github.com/kotatogram/kotatogram-desktop)

VanGram is based on AyuGram, developed and maintained by
[Radolyn Labs](https://github.com/AyuGram).
