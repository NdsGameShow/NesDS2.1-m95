# NesDS v2.1-m95d (Mapper 95 Support Fork)

A fork of NesDS 2.1 (DS-Homebrew/NesDS) adding iNES Mapper 95 (NAMCOT-3425) support and build timestamp display.

## Key Changes in v2.1-m95d
- Mapper 95 (NAMCOT-3425) Support: Added support for Dragon Buster (J) and other Mapper 95 titles (5-bit CHR bank masking, NT0-NT3 mirroring control via bit 5, disabled MMC3 scanline IRQ).
- Build Version and Timestamp Display: Displays v2.1-m95d and compile date/time on the ROM selection screen, with a separator below the header.
- Audio handoff: Process ARM7 control commands in its main loop; retry ARM9 FIFO sends and poll shared acknowledgements without VBlank waits.
- ROM list safety: Bound the static directory-name buffer and check directory/file operations before accessing entries.
- ROM list safety: Bound the static directory-name buffer and check directory/file operations before accessing entries.
- Menu safety: Keep ROM browsing on the main loop, suspend emulator VBlank rendering while browsing/loading, and protect SD access from VBlank IRQs.

## Build Instructions
Built using Docker with legacy devkitpro/devkitarm (libnds 1.x toolchain) using the make command.

## Original Documentation
See README_ORIGINAL_2.1.txt (or README_ORIGINAL_2.1.md) and NOTES_ORIGINAL_2.1.txt for the original NesDS documentation and credits.
