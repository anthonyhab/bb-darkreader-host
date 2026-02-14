# bb-darkreader-host (C Version)

> Ultra-lightweight native messaging host for syncing pywal colors with Dark Reader

![Demo](https://user-images.githubusercontent.com/2229212/377722433-73b648ac-fc2d-476d-857f-8ed86da79c96.png)

Your wallpaper changes → pywal generates colors → Firefox updates automatically. All websites get your theme.

---

## Why C Version?

This is the **C version** - optimized for minimum resource usage:

| Metric | This C Version | Go Version |
|--------|---------------|------------|
| Binary size | ~15 KB | 2.1 MB |
| Memory usage | ~1 MB | ~10 MB |
| Dependencies | None | Go runtime |

The Go version is in the `../darkreader-host-go/` directory if you need it.

---

## Quick Start (2 Minutes)

### 1. Install the Modified Dark Reader

Download and install **[darkreader.xpi](https://github.com/alexhulbert/SeaGlass/raw/main/user/files/darkreader.xpi)** in Firefox:

1. Open Firefox → Menu → Add-ons and Themes
2. Click the gear icon → Debug Add-ons
3. Click "Load Temporary Add-on"
4. Select the downloaded `darkreader.xpi`

**Important:** After installing, open Dark Reader settings:
- Go to **Dev Tools** → Click **Preview new design**
- Go to **Settings** → **Advanced** → Enable **Synchronize site fixes**

### 2. Install bb-darkreader-host

**Arch Linux (AUR):**
```bash
yay -S bb-darkreader-host
```

### 3. Run Setup

```bash
bb-darkreader-host --setup
```

This creates the native messaging config at `~/.mozilla/native-messaging-hosts/darkreader.json`.

### 4. Restart Firefox

Close Firefox completely and open it again. That's it!

---

## How It Works

```
┌─────────────┐     ┌──────────┐     ┌──────────────────┐     ┌─────────┐
│   Wallpaper │ ──► │  pywal   │ ──► │ ~/.cache/wal/    │ ──► │ Firefox │
│   changes   │     │          │     │ colors           │     │ + Dark  │
└─────────────┘     └──────────┘     └──────────────────┘     └─────────┘
                                                  │
                                                  ▼
                                         ┌──────────────────┐
                                         │ bb-darkreader-   │
                                         │ host (C version) │
                                         │ - Uses inotify   │
                                         │ - ~1MB memory    │
                                         │ - Auto-creates   │
                                         │   config on run  │
                                         └──────────────────┘
```

1. **pywal** runs and writes colors to `~/.cache/wal/colors`
2. **bb-darkreader-host** watches that file using Linux inotify (very efficient!)
3. When colors change, it sends a message to **Dark Reader**
4. **Dark Reader** updates all websites to match your theme

---

## Requirements

- **pywal** - Generates colors from your wallpaper (`sudo pacman -S pywal`)
- **Firefox** with the [modified Dark Reader fork](https://github.com/alexhulbert/SeaGlass/raw/main/user/files/darkreader.xpi)

---

## Installation

### Arch Linux (AUR)

```bash
yay -S bb-darkreader-host
bb-darkreader-host --setup
# Restart Firefox
```

### Manual Build

```bash
git clone https://github.com/bb-auth/bb-darkreader-host
cd bb-darkreader-host/c
gcc -O2 -s -o bb-darkreader-host coloreader-native.c
sudo install -Dm755 bb-darkreader-host /usr/bin/bb-darkreader-host
bb-darkreader-host --setup
# Restart Firefox
```

---

## Commands

```bash
# One-time setup (creates native messaging config)
bb-darkreader-host --setup

# Run daemon manually (for testing)
bb-darkreader-host
```

---

## Fork Info

This is a **fork** of the native messaging host from [alexhulbert/seaglass](https://github.com/alexhulbert/SeaGlass).

The original was a Node.js script. This C version is:
- Written in C for minimum resource usage
- Uses Linux inotify for efficient file watching
- No runtime dependencies
- Simple setup with `--setup` command

**Original by:** [alexhulbert](https://github.com/alexhulbert)
**Fork by:** [bb-auth](https://github.com/bb-auth)

---

## Troubleshooting

### "Theme not updating"

1. Check if pywal is generating colors:
   ```bash
   cat ~/.cache/wal/colors | head -5
   ```

2. Run setup again:
   ```bash
   bb-darkreader-host --setup
   ```

3. Check if config exists:
   ```bash
   cat ~/.mozilla/native-messaging-hosts/darkreader.json
   ```

4. Run manually to see errors:
   ```bash
   bb-darkreader-host
   ```

### "Dark Reader says no theme"

Make sure you installed the **modified Dark Reader fork** from seaglass, not the regular one from the Firefox Add-ons store.

---

## License

MIT - See [LICENSE](LICENSE)
