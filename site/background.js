// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Beach 909 backdrop: turn a picture into the 120x56 RGB565 block the firmware
// keeps, and send it with the FM1 transfer protocol (see app/transfer.c).
(function (root) {
  'use strict';
  const WIDTH = 120, HEIGHT = 56, CHUNK = 64;

  const table = new Uint32Array(256).map((_, n) => {
    let v = n;
    for (let k = 0; k < 8; k++) v = v & 1 ? 0xedb88320 ^ (v >>> 1) : v >>> 1;
    return v >>> 0;
  });
  function crc32(bytes) {
    let v = ~0;
    for (const b of bytes) v = table[(v ^ b) & 255] ^ (v >>> 8);
    return ~v >>> 0;
  }
  // rgba: WIDTH * HEIGHT * 4 bytes. Returns header + little-endian RGB565 pixels.
  function buildBlob(rgba) {
    if (rgba.length !== WIDTH * HEIGHT * 4) throw Error('Picture must be 120 x 56 pixels');
    const blob = new Uint8Array(16 + WIDTH * HEIGHT * 2), view = new DataView(blob.buffer);
    for (let i = 0; i < WIDTH * HEIGHT; i++) {
      const color = (rgba[i * 4] >> 3) << 11 | (rgba[i * 4 + 1] >> 2) << 5 | rgba[i * 4 + 2] >> 3;
      view.setUint16(16 + i * 2, color, true);
    }
    blob.set([0x42, 0x47, 0x30, 0x39]); // "BG09"
    view.setUint16(4, WIDTH, true);
    view.setUint16(6, HEIGHT, true);
    view.setUint32(8, crc32(blob.subarray(16)), true);
    view.setUint32(12, 0xffffffff, true); // the device commits after its own check
    return blob;
  }
  // F0 7D 'F' 'M' 02 cmd token-lo token-hi packed checksum F7
  function frame(cmd, token, data) {
    const body = [0x7d, 0x46, 0x4d, 2, cmd, token & 127, (token >> 7) & 127];
    let value = 0, bits = 0;
    for (const b of data) {
      value |= b << bits; bits += 8;
      while (bits >= 7) { body.push(value & 127); value >>= 7; bits -= 7; }
    }
    if (bits) body.push(value & 127);
    body.push(-body.reduce((a, b) => a + b, 0) & 127);
    return Uint8Array.from([0xf0, ...body, 0xf7]);
  }
  // Returns {status, data} for the reply to (cmd, token), or null for anything else.
  function parse(message, cmd, token) {
    const m = message;
    if (m.length < 11 || m[0] !== 0xf0 || m[m.length - 1] !== 0xf7 || m[1] !== 0x7d || m[2] !== 0x46 ||
        m[3] !== 0x4d || m[4] !== 2 || m[5] !== (cmd | 64) || m[6] !== (token & 127) || m[7] !== ((token >> 7) & 127))
      return null;
    let sum = 0;
    for (let i = 1; i < m.length - 1; i++) sum += m[i];
    if (sum & 127) return null;
    const bytes = [];
    let value = 0, bits = 0;
    for (let i = 8; i < m.length - 2; i++) {
      value |= m[i] << bits; bits += 7;
      if (bits >= 8) { bytes.push(value & 255); value >>= 8; bits -= 8; }
    }
    if (!bytes.length) return null;
    return { status: bytes[0], data: Uint8Array.from(bytes.slice(1)) };
  }
  // `request(frameBytes, cmd, token)` resolves with the parsed reply or rejects.
  function client(request) {
    let token = Math.floor(Math.random() * 16384);
    async function call(cmd, data) {
      token = (token + 1) & 16383;
      const reply = await request(frame(cmd, token, data), cmd, token);
      if (reply.status) throw Error(reply.status === 67 ? 'FM-1 is busy; try again'
        : 'FM-1 refused the request (status ' + reply.status + ')');
      return reply.data;
    }
    return {
      async state() { const d = await call(30, []); return { picture: !!d[0], width: d[1], height: d[2] }; },
      async send(blob, progress) {
        for (let at = 0; at < blob.length; at += CHUNK) {
          await call(31, [at & 255, at >> 8, ...blob.subarray(at, at + CHUNK)]);
          if (progress) progress(Math.min(1, (at + CHUNK) / blob.length));
        }
        await call(32, Array.from(blob.subarray(8, 12)));
      },
      async clear() { await call(33, []); }
    };
  }
  root.FM1Background = { WIDTH, HEIGHT, crc32, buildBlob, frame, parse, client };
})(typeof globalThis !== 'undefined' ? globalThis : this);
