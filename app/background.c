/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Player-supplied backdrop for Bitch909: one 120x56 RGB565 picture, sent from
 * the browser and kept in the last four sectors of the sample area. A sample
 * bank large enough to reach those sectors takes them back: the picture then
 * fails its checksum and the built-in scene returns. Control task only. */
#include "fm1_device.h"
#include "instrument.h"
#include <string.h>

enum { MAGIC = 0x39304742 /* "BG09" */, PIXEL_BYTES = GROOVE_BACKGROUND_BYTES - 16 };
struct header { uint32_t magic; uint16_t width, height; uint32_t crc, commit; };
_Static_assert(sizeof(struct header) == 16, "background header");
static const uint8_t *pixels;
static unsigned checked, position;

static int usable(void)
{
    return fm1_flash_size() == GROOVE_FLASH_BYTES && groove_samples_status() != 3 &&
           groove_samples_size() <= GROOVE_BACKGROUND_OFFSET - GROOVE_SAMPLE_OFFSET;
}
const uint8_t *groove_background(void)
{
    return pixels;
}
void groove_background_poll(void)
{
    if (checked) return;
    struct header h;
    int r = fm1_flash_read(GROOVE_BACKGROUND_OFFSET, &h, sizeof h);
    if (r == FM1_FLASH_BUSY) return;
    checked = 1;
    pixels = NULL;
    if (r || !usable() || h.magic != MAGIC || h.width != GROOVE_BACKGROUND_W ||
        h.height != GROOVE_BACKGROUND_H || h.commit)
        return;
    const uint8_t *mapped = fm1_flash_map(GROOVE_BACKGROUND_OFFSET + sizeof h, PIXEL_BYTES);
    if (mapped && ~groove_crc32(~0U, mapped, PIXEL_BYTES) == h.crc) pixels = mapped;
}
/* Sequential 64-byte chunks from offset zero. The commit word stays erased
 * until the whole picture has been read back and checked. */
int groove_background_write(unsigned offset, const uint8_t *data, unsigned bytes)
{
    if (!usable() || !data || offset % 64U || offset >= GROOVE_BACKGROUND_BYTES ||
        bytes > GROOVE_BACKGROUND_BYTES - offset ||
        bytes != (GROOVE_BACKGROUND_BYTES - offset < 64U ? GROOVE_BACKGROUND_BYTES - offset : 64U) ||
        (offset && offset != position))
        return GROOVE_PROJECT_INVALID;
    pixels = NULL;
    checked = 1;
    position = 0;
    uint8_t copy[64];
    memcpy(copy, data, bytes);
    if (!offset) memset(copy + 12, 0xff, 4);
    int r = offset % FM1_FLASH_SECTOR_BYTES ? 0 : fm1_flash_erase(GROOVE_BACKGROUND_OFFSET + offset);
    if (!r) r = fm1_flash_write(GROOVE_BACKGROUND_OFFSET + offset, copy, bytes);
    if (!r) position = offset + bytes;
    return r;
}
int groove_background_finish(uint32_t crc)
{
    if (position != GROOVE_BACKGROUND_BYTES || !usable()) return GROOVE_PROJECT_INVALID;
    struct header h;
    int r = fm1_flash_invalidate(GROOVE_BACKGROUND_OFFSET, GROOVE_BACKGROUND_BYTES);
    if (!r) r = fm1_flash_read(GROOVE_BACKGROUND_OFFSET, &h, sizeof h);
    if (r) return r;
    const uint8_t *mapped = fm1_flash_map(GROOVE_BACKGROUND_OFFSET + sizeof h, PIXEL_BYTES);
    if (!mapped || h.magic != MAGIC || h.crc != crc || ~groove_crc32(~0U, mapped, PIXEL_BYTES) != crc)
        return GROOVE_PROJECT_INVALID;
    uint32_t commit = 0;
    r = fm1_flash_write(GROOVE_BACKGROUND_OFFSET + 12, &commit, sizeof commit);
    if (!r) r = fm1_flash_invalidate(GROOVE_BACKGROUND_OFFSET, sizeof h);
    position = 0;
    checked = 0; /* Read it back as a power-on would. */
    return r;
}
int groove_background_clear(void)
{
    if (!usable()) return GROOVE_PROJECT_INVALID;
    pixels = NULL;
    checked = 1;
    position = 0;
    return fm1_flash_erase(GROOVE_BACKGROUND_OFFSET);
}
