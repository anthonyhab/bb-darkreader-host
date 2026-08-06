# bb-darkreader-host

> Lightweight C native messaging host for Dark Reader

A drop-in replacement for the seaglass Node.js host with low memory usage and strict native-message/palette validation.

---

## Why?

The original seaglass host uses Node.js and consumes significant memory. This C version avoids a JavaScript runtime and uses the small `json-c` parser for strict palette validation.

Perfect if you run multiple color watchers (Dark Reader, Pywalfox, etc.) and want to minimize footprint.

---

## Requirements

- Linux with inotify support
- Modified [Dark Reader fork](https://github.com/alexhulbert/SeaGlass/raw/main/user/files/darkreader.xpi) for Firefox
- pywal or similar color generator
- json-c

---

## Quick Start

### Arch Linux (AUR)
```bash
yay -S bb-darkreader-host
bb-darkreader-host install
# Restart Firefox
```

### Manual Build
```bash
make
sudo install -Dm755 bb-darkreader-host /usr/bin/bb-darkreader-host
bb-darkreader-host install
```

---

## Usage

### Commands

| Command | Description |
|---------|-------------|
| `bb-darkreader-host` | Run daemon (auto-started by Firefox) |
| `bb-darkreader-host install` | Install native messaging manifest |

---

## How It Works

1. **Firefox starts the daemon** via native messaging when Dark Reader loads
2. **inotify watches** the directory containing `~/.cache/wal/colors.json`, including atomic renames
3. **Validated, deduplicated updates** are sent to Dark Reader when effective colors change

The daemon automatically detects changes to the pywal colors file and sends updates to Dark Reader. No manual configuration needed after installation.

---

## Integration with Theme Hooks

The daemon automatically updates when `~/.cache/wal/colors.json` changes. No manual update command is needed.

---

## Stats

| Metric | Value |
|--------|-------|
| Binary size | ~15KB |
| Memory usage | ~1MB |
| Dependencies | glibc, json-c |
| Build tools | gcc |

---

## Fork Info

Based on [alexhulbert/seaglass](https://github.com/alexhulbert/SeaGlass).

---

## Troubleshooting

### Colors not updating
```bash
# Check daemon is running
ps aux | grep bb-darkreader-host

# Reinstall manifest
bb-darkreader-host install
# Restart Firefox
```

### Verify colors file
```bash
# Check pywal colors exist
python3 -m json.tool ~/.cache/wal/colors.json | head -20
```

---

## Building from Source

```bash
# Clone and build
git clone https://github.com/anthonyhab/bb-darkreader-host-c.git
cd bb-darkreader-host-c
make

# Install
sudo install -Dm755 bb-darkreader-host /usr/bin/bb-darkreader-host
bb-darkreader-host install
```

---

## License

MIT - See [LICENSE](LICENSE)
