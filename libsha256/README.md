# libsha256

SHA-256 (`FIPS 180-4`) for callers that need a digest without pulling in a TLS
stack - verifying what was written back to flash, checking a firmware payload.
For the API and usage examples please visit
[phoenix-rtos-doc](https://github.com/phoenix-rtos/phoenix-rtos-doc/blob/master/corelibs/libsha256.md).

## Updating the extract

The implementation is a single-file extract from
[libtomcrypt](https://github.com/libtom/libtomcrypt), kept in upstream's
formatting - and with formatting disabled in `sha256.c` - so that it stays
diffable against the original. `sbom.json` records the snapshot it was taken
from; update it whenever the extract is refreshed.

Everything Phoenix-RTOS adds on top lives outside that region: the public header
`include/libsha256.h`, which owns the context type the extract aliases to
upstream's `hash_state`, and the tests in `tests/`.
