# bb-darkreader-host

> Lightweight C native messaging host for Dark Reader

A drop-in replacement for the seaglass Node.js host with **~1MB memory usage** instead of ~50MB.

---

## Why?

The original seaglass host uses Node.js and consumes significant memory. This C version uses **~1MB** with zero runtime dependencies.

Perfect if you run multiple color watchers (Dark Reader, Pywalfox, etc.) and want to minimize footprint.

---

## Requirements

- Linux with inotify support
- Modified [Dark Reader fork](https://github.com/alexhulbert/SeaGlass/raw/main/user/files/darkreader.xpi) for Firefox
- pywal or similar color generator

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
gcc -O2 -s -o bb-darkreader-host bb-darkreader-host.c
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
2. **inotify watches** `~/.cache/wal/colors` for file changes
3. **Automatic updates** sent to Dark Reader when colors change

The daemon automatically detects changes to the pywal colors file and sends updates to Dark Reader. No manual configuration needed after installation.

---

## Integration with Theme Hooks

The daemon automatically updates when `~/.cache/wal/colors` changes. No manual updates needed - just ensure your theme hooks generate the pywal colors file.

---

## Stats

| Metric | Value |
|--------|-------|
| Binary size | ~15KB |
| Memory usage | ~1MB |
| Dependencies | None (glibc only) |
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
cat ~/.cache/wal/colors | head -20
```

---

## Building from Source

```bash
# Clone and build
git clone https://github.com/anthonyhab/bb-darkreader-host-c.git
cd bb-darkreader-host-c
gcc -O2 -s -o bb-darkreader-host bb-darkreader-host.c

# Install
sudo install -Dm755 bb-darkreader-host /usr/bin/bb-darkreader-host
bb-darkreader-host install
```

---

## License

MIT - See [LICENSE](LICENSE)
