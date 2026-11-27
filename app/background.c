/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Player-supplied pictures for Bitch909, sent from the browser as RGB565:
 * slot 0 is the 120x56 backdrop, slot 1 the 40x40 knife-board target, slot 2
 * the 16x16 thing thrown at it. They live in the last six sectors of the sample area. A sample bank large enough
 * to reach those sectors takes them back: the pictures then fail their
 * checksums and the built-in art returns. Control task only. */
#include "fm1_device.h"
#include "instrument.h"
#include <string.h>

enum { MAGIC = 0x39304742 /* "BG09" */ };
struct header { uint32_t magic; uint16_t width, height; uint32_t crc, commit; };
_Static_assert(sizeof(struct header) == 16, "picture header");
static const struct { uint32_t offset; uint16_t width, height; } slots[GROOVE_PICTURES] = {
    {GROOVE_BACKGROUND_OFFSET, GROOVE_BACKGROUND_W, GROOVE_BACKGROUND_H},
    {GROOVE_TARGET_OFFSET, GROOVE_TARGET_SIZE, GROOVE_TARGET_SIZE},
    {GROOVE_THROW_OFFSET, GROOVE_THROW_SIZE, GROOVE_THROW_SIZE}};
static const uint8_t *pixels[GROOVE_PICTURES];
static unsigned checked[GROOVE_PICTURES], position[GROOVE_PICTURES];

static unsigned total(unsigned slot) { return 16U + slots[slot].width * slots[slot].height * 2U; }
static int usable(void)
{
    return fm1_flash_size() == GROOVE_FLASH_BYTES && groove_samples_status() != 3 &&
           groove_samples_size() <= GROOVE_THROW_OFFSET - GROOVE_SAMPLE_OFFSET;
}
const uint8_t *groove_picture(unsigned slot)
{
    return slot < GROOVE_PICTURES ? pixels[slot] : NULL;
}
void groove_picture_poll(void)
{
    for (unsigned slot = 0; slot < GROOVE_PICTURES; ++slot) {
        if (checked[slot]) continue;
        struct header h;
        int r = fm1_flash_read(slots[slot].offset, &h, sizeof h);
        if (r == FM1_FLASH_BUSY) return;
        checked[slot] = 1;
        pixels[slot] = NULL;
        if (r || !usable() || h.magic != MAGIC || h.width != slots[slot].width ||
            h.height != slots[slot].height || h.commit)
            continue;
        const uint8_t *mapped = fm1_flash_map(slots[slot].offset + sizeof h, total(slot) - sizeof h);
        if (mapped && ~groove_crc32(~0U, mapped, total(slot) - sizeof h) == h.crc) pixels[slot] = mapped;
    }
}
/* Sequential 64-byte chunks from offset zero. The commit word stays erased
 * until the whole picture has been read back and checked. */
int groove_picture_write(unsigned slot, unsigned offset, const uint8_t *data, unsigned bytes)
{
    if (slot >= GROOVE_PICTURES || !usable() || !data || offset % 64U || offset >= total(slot) ||
        bytes != (total(slot) - offset < 64U ? total(slot) - offset : 64U) ||
        (offset && offset != position[slot]))
        return GROOVE_PROJECT_INVALID;
    pixels[slot] = NULL;
    checked[slot] = 1;
    position[slot] = 0;
    uint8_t copy[64];
    memcpy(copy, data, bytes);
    if (!offset) memset(copy + 12, 0xff, 4);
    int r = offset % FM1_FLASH_SECTOR_BYTES ? 0 : fm1_flash_erase(slots[slot].offset + offset);
    if (!r) r = fm1_flash_write(slots[slot].offset + offset, copy, bytes);
    if (!r) position[slot] = offset + bytes;
    return r;
}
int groove_picture_finish(unsigned slot, uint32_t crc)
{
    if (slot >= GROOVE_PICTURES || position[slot] != total(slot) || !usable()) return GROOVE_PROJECT_INVALID;
    struct header h;
    int r = fm1_flash_invalidate(slots[slot].offset, total(slot));
    if (!r) r = fm1_flash_read(slots[slot].offset, &h, sizeof h);
    if (r) return r;
    const uint8_t *mapped = fm1_flash_map(slots[slot].offset + sizeof h, total(slot) - sizeof h);
    if (!mapped || h.magic != MAGIC || h.crc != crc || ~groove_crc32(~0U, mapped, total(slot) - sizeof h) != crc)
        return GROOVE_PROJECT_INVALID;
    uint32_t commit = 0;
    r = fm1_flash_write(slots[slot].offset + 12, &commit, sizeof commit);
    if (!r) r = fm1_flash_invalidate(slots[slot].offset, sizeof h);
    position[slot] = 0;
    checked[slot] = 0; /* Read it back as a power-on would. */
    return r;
}
int groove_picture_clear(unsigned slot)
{
    if (slot >= GROOVE_PICTURES || !usable()) return GROOVE_PROJECT_INVALID;
    pixels[slot] = NULL;
    checked[slot] = 1;
    position[slot] = 0;
    return fm1_flash_erase(slots[slot].offset);
}
