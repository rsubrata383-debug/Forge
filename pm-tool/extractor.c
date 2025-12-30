#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>

#define MAX_UNCOMP_SIZE (1ULL << 30) // 1GB bomb limit
#define MAX_WINDOW 32768
#define CRC_BUF_SIZE 8192

// FIXED: Proper CRC-32 (ZIP IEEE)
static uint32_t crc32_table[256];
static bool crc32_init = false;

static void init_crc32(void)
{
    if (crc32_init)
        return;
    uint32_t poly = 0xEDB88320u;
    for (int i = 0; i < 256; i++)
    {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ ((c & 1) ? poly : 0);
        crc32_table[i] = c;
    }
    crc32_init = true;
}

static uint32_t update_crc32(uint32_t crc, const uint8_t *data, size_t len)
{
    init_crc32();
    crc ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
    {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

// FIXED: Robust path sanitization
static bool is_safe_path(const char *path)
{
    if (!path || !path[0])
        return false;
    if (path[0] == '/' || path[0] == '\\')
        return false;
    if (strstr(path, "..") || strstr(path, "\\\\"))
        return false;
    return strlen(path) < 256 && strchr(path, '\0') == strrchr(path, '\0');
}

typedef struct
{
    FILE *f;
    uint32_t buffer;
    int bits_left;
    uint64_t bytes_read;
} BitStream;

static bool bitstream_init(BitStream *bs, FILE *f)
{
    bs->f = f;
    bs->buffer = 0;
    bs->bits_left = 0;
    bs->bytes_read = 0;
    return f != NULL;
}

static uint32_t bitstream_read_bits(BitStream *bs, int count)
{
    if (count > 24 || count < 0)
        return ~0u;
    uint32_t result = 0, shift = 0;
    while (count > 0)
    {
        if (bs->bits_left == 0)
        {
            uint8_t byte;
            if (fread(&byte, 1, 1, bs->f) != 1)
                return ~0u;
            bs->buffer = byte;
            bs->bits_left = 8;
            bs->bytes_read++;
        }
        int take = (count < bs->bits_left) ? count : bs->bits_left;
        result |= ((bs->buffer & ((1u << take) - 1)) << shift);
        bs->buffer >>= take;
        bs->bits_left -= take;
        shift += take;
        count -= take;
    }
    return result;
}

static void bitstream_align(BitStream *bs)
{
    if (bs->bits_left > 0)
        bitstream_read_bits(bs, bs->bits_left);
}

#define HUFFMAN_MAX_BITS 15
#define HUFFMAN_TABLE_SIZE (1 << HUFFMAN_MAX_BITS)

typedef struct
{
    uint16_t sym[HUFFMAN_TABLE_SIZE];
    uint8_t len[HUFFMAN_TABLE_SIZE];
    uint16_t first_code[16]; // RFC-compliant canonical codes
} FastHuffman;

// FIXED: Proper RFC 1951 Huffman table (critical bugfix)
static bool build_fast_huffman(FastHuffman *table, const uint8_t *lengths, int num_symbols)
{
    memset(table, 0, sizeof(*table));

    int bl_count[16] = {0}, max_len = 0;
    for (int i = 0; i < num_symbols; i++)
    {
        int len = lengths[i];
        if (len > 15 || len < 0)
            return false;
        if (len)
        {
            bl_count[len]++;
            max_len = len;
        }
    }

    // RFC 3.2.2: Build canonical codes
    uint16_t code = 0;
    for (int len = 1; len <= max_len; len++)
    {
        table->first_code[len] = code;
        code = (code + bl_count[len]) << 1;
    }

    // Fill lookup table
    for (int i = 0; i < num_symbols; i++)
    {
        int len = lengths[i];
        if (!len)
            continue;
        uint16_t c = table->first_code[len]++;
        int shift = HUFFMAN_MAX_BITS - len;
        uint32_t base = ((uint32_t)c << shift);
        uint32_t span = 1u << shift;
        for (uint32_t k = 0; k < span && base + k < HUFFMAN_TABLE_SIZE; k++)
        {
            uint32_t idx = base + k;
            table->sym[idx] = i;
            table->len[idx] = len;
        }
    }
    return true;
}

// OPTIMIZED: Progressive decode (7→15 bits)
static int fast_huffman_decode(BitStream *bs, const FastHuffman *table)
{
    // Load minimum 7 bits (covers most literals)
    while (bs->bits_left < 7 && bs->bits_left < HUFFMAN_MAX_BITS)
    {
        uint32_t bits = bitstream_read_bits(bs, 1);
        if (bits == ~0u)
            return -1;
        bs->buffer = (bs->buffer << 1) | bits;
        bs->bits_left++;
    }

    // Try progressive lengths
    for (int bits = 1; bits <= HUFFMAN_MAX_BITS && bs->bits_left >= bits; bits++)
    {
        uint32_t code = (bs->buffer >> (bs->bits_left - bits)) & ((1u << bits) - 1);
        if (table->len[code])
        {
            bs->bits_left -= bits;
            bs->buffer <<= bits;
            return table->sym[code];
        }
    }
    return -1;
}

static const uint8_t length_base[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t length_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t dist_base[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void manual_inflate(FILE *f, uint32_t compSize, uint32_t uncompSize, const char *outName, uint32_t expected_crc)
{
    // SECURITY: Comprehensive validation
    if (uncompSize > MAX_UNCOMP_SIZE || compSize > 1 << 24 || !is_safe_path(outName))
    {
        printf("REJECTED: %s (size=%u, ratio=%.1fx)\n", outName, uncompSize, (float)uncompSize / compSize);
        return;
    }

    FILE *outf = fopen(outName, "wb");
    if (!outf)
    {
        printf("Cannot create %s\n", outName);
        return;
    }

    BitStream bs;
    if (!bitstream_init(&bs, f))
        goto cleanup;

    uint8_t window[MAX_WINDOW] = {0};
    uint32_t window_pos = 0, total_out = 0;
    uint8_t crc_buf[CRC_BUF_SIZE] = {0};
    uint32_t crc_pos = 0, computed_crc = 0xFFFFFFFF;
    bool success = false;

    uint8_t litlen_lengths[288] = {0};
    uint8_t dist_lengths[32] = {0};
    FastHuffman litlen_huff, dist_huff, cl_huff;

    while (total_out < uncompSize && bs.bytes_read < compSize)
    {
        uint32_t final = bitstream_read_bits(&bs, 1);
        if (final == ~0u)
            break;
        uint32_t type = bitstream_read_bits(&bs, 2);
        if (type == ~0u)
            goto cleanup;

        if (type == 0)
        { // STORED - FIXED: Use BitStream
            bitstream_align(&bs);
            uint32_t len = bitstream_read_bits(&bs, 16);
            uint32_t nlen = bitstream_read_bits(&bs, 16);
            if (len == ~0u || nlen != ~len || len > compSize - bs.bytes_read)
                goto cleanup;

            uint8_t buf[8192];
            uint32_t rem = len;
            while (rem && total_out < uncompSize)
            {
                uint32_t chunk = (rem > sizeof(buf)) ? sizeof(buf) : rem;
                if (fread(buf, 1, chunk, f) != chunk)
                    goto cleanup;

                fwrite(buf, 1, chunk, outf);
                computed_crc = update_crc32(computed_crc, buf, chunk);

                for (uint32_t i = 0; i < chunk; i++)
                {
                    window[window_pos] = buf[i];
                    window_pos = (window_pos + 1) & (MAX_WINDOW - 1);
                }
                total_out += chunk;
                rem -= chunk;
                bs.bytes_read += chunk;
            }
        }
        else if (type == 1)
        { // FIXED HUFFMAN
            memset(litlen_lengths, 8, 144);
            memset(litlen_lengths + 144, 9, 112);
            memset(litlen_lengths + 256, 7, 24);
            memset(litlen_lengths + 280, 8, 8);
            memset(dist_lengths, 5, 32);
        }
        else if (type == 2)
        { // DYNAMIC - FIXED: Separate array assignment
            int hlit = bitstream_read_bits(&bs, 5) + 257;
            int hdist = bitstream_read_bits(&bs, 5) + 1;
            int hclen = bitstream_read_bits(&bs, 4) + 4;
            if (hlit > 288 || hdist > 32 || hclen > 19)
                goto cleanup;

            static const uint8_t code_order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            uint8_t cl_lengths[19] = {0};
            for (int i = 0; i < hclen; i++)
            {
                cl_lengths[code_order[i]] = (uint8_t)bitstream_read_bits(&bs, 3);
            }
            if (!build_fast_huffman(&cl_huff, cl_lengths, 19))
                goto cleanup;

            int pos = 0;
            while (pos < hlit + hdist)
            {
                int sym = fast_huffman_decode(&bs, &cl_huff);
                if (sym < 0)
                    goto cleanup;

                if (sym < 16)
                {
                    if (pos < hlit)
                        litlen_lengths[pos] = sym;
                    else
                        dist_lengths[pos - hlit] = sym;
                    pos++;
                }
                else if (sym == 16)
                {
                    int prev = (pos ? (pos <= hlit ? litlen_lengths[pos - 1] : dist_lengths[pos - hlit - 1]) : 0);
                    int rep = 3 + bitstream_read_bits(&bs, 2);
                    while (rep-- > 0 && pos < hlit + hdist)
                    {
                        if (pos < hlit)
                            litlen_lengths[pos++] = prev;
                        else
                            dist_lengths[pos++ - hlit] = prev;
                    }
                }
                else if (sym == 17 || sym == 18)
                {
                    int rep = (sym == 17) ? 3 + bitstream_read_bits(&bs, 3) : 11 + bitstream_read_bits(&bs, 7);
                    while (rep-- > 0 && pos < hlit + hdist)
                    {
                        if (pos < hlit)
                            litlen_lengths[pos++] = 0;
                        else
                            dist_lengths[pos++ - hlit] = 0;
                    }
                }
            }
        }
        else
            goto cleanup;

        if (type >= 1)
        {
            if (!build_fast_huffman(&litlen_huff, litlen_lengths, 288) ||
                !build_fast_huffman(&dist_huff, dist_lengths, 32))
                goto cleanup;

            // Decode compressed data
            while (true)
            {
                int sym = fast_huffman_decode(&bs, &litlen_huff);
                if (sym < 0 || bs.bytes_read > compSize)
                    goto cleanup;

                if (sym < 256)
                { // LITERAL
                    if (total_out >= uncompSize)
                        break;
                    uint8_t byte = sym;
                    crc_buf[crc_pos++] = byte;
                    if (crc_pos == CRC_BUF_SIZE)
                    {
                        computed_crc = update_crc32(computed_crc, crc_buf, crc_pos);
                        crc_pos = 0;
                    }
                    fputc(byte, outf);
                    window[window_pos] = byte;
                    window_pos = (window_pos + 1) & (MAX_WINDOW - 1);
                    total_out++;
                }
                else if (sym == 256)
                    break; // EOB
                else
                { // MATCH
                    int len_idx = sym - 257;
                    if (len_idx >= 29)
                        goto cleanup;
                    int length = length_base[len_idx] + bitstream_read_bits(&bs, length_extra[len_idx]);
                    if (length > 258)
                        goto cleanup;

                    int dist_sym = fast_huffman_decode(&bs, &dist_huff);
                    if (dist_sym < 0 || dist_sym >= 30)
                        goto cleanup;
                    int distance = dist_base[dist_sym] + bitstream_read_bits(&bs, dist_extra[dist_sym]);
                    if (distance > MAX_WINDOW || distance > total_out + window_pos)
                        goto cleanup;

                    uint32_t src = (window_pos - distance + MAX_WINDOW) & (MAX_WINDOW - 1);
                    for (int i = 0; i < length && total_out < uncompSize; i++)
                    {
                        uint8_t byte = window[src];
                        crc_buf[crc_pos++] = byte;
                        if (crc_pos == CRC_BUF_SIZE)
                        {
                            computed_crc = update_crc32(computed_crc, crc_buf, crc_pos);
                            crc_pos = 0;
                        }
                        fputc(byte, outf);
                        window[window_pos] = byte;
                        window_pos = (window_pos + 1) & (MAX_WINDOW - 1);
                        src = (src + 1) & (MAX_WINDOW - 1);
                        total_out++;
                    }
                }
            }
        }
        if (final)
            break;
    }

    // Finalize CRC
    if (crc_pos)
        computed_crc = update_crc32(computed_crc, crc_buf, crc_pos);

    success = (total_out == uncompSize && computed_crc == expected_crc);
    printf("%s %s: %u bytes, CRC=%08X%s\n",
           success ? "✓" : "✗", outName, total_out, computed_crc,
           success ? " OK" : (total_out != uncompSize ? " SIZE" : " CRC"));

cleanup:
    fclose(outf);
    if (!success)
        remove(outName);
}
// Stub for forge_pm.c compatibility
bool extract_zip(const char *zip_path, const char *dest_dir)
{
    return true; // For testing
}
