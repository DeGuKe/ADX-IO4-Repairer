# Security and safe operation

ADX IO4 Repairer opens the selected HID interface with `GENERIC_READ`. Its runtime scope is report reading, button-state decoding, state merging and forwarding existing touch/LED APIs.

For a clean deployment:

1. Download binaries from this repository's Releases page.
2. Compare SHA-256 with the release notes and `checksums/SHA256SUMS.txt`.
3. Keep a copy of the current `segatools.ini` before changing `[mai2io] path`.
4. Use `tests/test_original.c` only inside an isolated test directory.
5. Share diagnostic logs after removing account, card, Keychip, PCBID and server identity fields.

Security reports can be submitted through the repository owner's GitHub contact channel with a minimal reproduction and the affected version hash.
