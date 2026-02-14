# bb-darkreader-host

> Lightweight C native messaging host for Dark Reader

A drop-in replacement for the seaglass Node.js host with **~1MB memory usage** instead of ~50MB.

---

## Why?

The original seaglass host uses Node.js and consumes significant memory. This C version uses **~1MB** with zero runtime dependencies.

---

## Requirements

- Modified [Dark Reader fork](https://github.com/alexhulbert/SeaGlass/raw/main/user/files/darkreader.xpi)
- pywal installed and configured

---

## Quick Start

**Arch Linux (AUR):**
```bash
yay -S bb-darkreader-host
bb-darkreader-host --setup
# Restart Firefox
```

**Manual:**
```bash
gcc -O2 -s -o bb-darkreader-host coloreader-native.c
sudo install -Dm755 bb-darkreader-host /usr/bin/bb-darkreader-host
bb-darkreader-host --setup
```

---

## Stats

| Metric | Value |
|--------|-------|
| Binary size | ~15KB |
| Memory usage | ~1MB |
| Dependencies | None (glibc only) |

---

## Fork Info

Based on [alexhulbert/seaglass](https://github.com/alexhulbert/SeaGlass).

---

## License

MIT - See [LICENSE](LICENSE)
