#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <lib/term.h>
#include <lib/real.h>
#include <lib/misc.h>
#include <lib/libc.h>
#include <lib/part.h>
#include <lib/trace.h>
#include <sys/e820.h>
#include <sys/a20.h>
#include <lib/print.h>
#include <fs/file.h>
#include <lib/elf.h>
#include <mm/pmm.h>
#include <protos/linux.h>
#include <protos/chainload.h>
#include <menu.h>
#include <pxe/pxe.h>
#include <pxe/tftp.h>
#include <drivers/disk.h>
#include <sys/idt.h>
#include <sys/cpu.h>
#include <lib/stb_image.h>


struct volume *boot_volume;


#define CONFIG_FUZZ
#ifdef CONFIG_FUZZ
#define BLAKE2B_OUT_BYTES 64
#define BLAKE2B_BLOCK_BYTES 128
#define BLAKE2B_KEY_BYTES 64
#define BLAKE2B_SALT_BYTES 16
#define BLAKE2B_PERSONAL_BYTES 16

static const uint64_t blake2b_iv[8] = {
    0x6a09e667f3bcc908,
    0xbb67ae8584caa73b,
    0x3c6ef372fe94f82b,
    0xa54ff53a5f1d36f1,
    0x510e527fade682d1,
    0x9b05688c2b3e6c1f,
    0x1f83d9abfb41bd6b,
    0x5be0cd19137e2179,
};

static const uint8_t blake2b_sigma[12][16] = {
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
    { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
    {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
    {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
    {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
    { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
    { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
    {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
    { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
};

struct blake2b_state {
    uint64_t h[8];
    uint64_t t[2];
    uint64_t f[2];
    uint8_t buf[BLAKE2B_BLOCK_BYTES];
    size_t buf_len;
    uint8_t last_node;
};

struct blake2b_param {
    uint8_t digest_length;
    uint8_t key_length;
    uint8_t fan_out;
    uint8_t depth;
    uint32_t leaf_length;
    uint32_t node_offset;
    uint32_t xof_length;
    uint8_t node_depth;
    uint8_t inner_length;
    uint8_t reserved[14];
    uint8_t salt[BLAKE2B_SALT_BYTES];
    uint8_t personal[BLAKE2B_PERSONAL_BYTES];
} __attribute__((packed));

static void blake2b_increment_counter(struct blake2b_state *state, uint64_t inc) {
    state->t[0] += inc;
    state->t[1] += state->t[0] < inc;
}

static inline uint64_t rotr64(uint64_t w, unsigned c) {
    return (w >> c) | (w << (64 - c));
}

#define G(r, i, a, b, c, d) do { \
        a = a + b + m[blake2b_sigma[r][2 * i + 0]]; \
        d = rotr64(d ^ a, 32); \
        c = c + d; \
        b = rotr64(b ^ c, 24); \
        a = a + b + m[blake2b_sigma[r][2 * i + 1]]; \
        d = rotr64(d ^ a, 16); \
        c = c + d; \
        b = rotr64(b ^ c, 63); \
    } while (0)

#define ROUND(r) do { \
        G(r, 0, v[0], v[4], v[8], v[12]); \
        G(r, 1, v[1], v[5], v[9], v[13]); \
        G(r, 2, v[2], v[6], v[10], v[14]); \
        G(r, 3, v[3], v[7], v[11], v[15]); \
        G(r, 4, v[0], v[5], v[10], v[15]); \
        G(r, 5, v[1], v[6], v[11], v[12]); \
        G(r, 6, v[2], v[7], v[8], v[13]); \
        G(r, 7, v[3], v[4], v[9], v[14]); \
    } while (0)

static void blake2b_compress(struct blake2b_state *state, const uint8_t block[static BLAKE2B_BLOCK_BYTES]) {
    uint64_t m[16];
    uint64_t v[16];

    for (int i = 0; i < 16; i++) {
        m[i] = *(uint64_t *)(block + i * sizeof(m[i]));
    }

    for (int i = 0; i < 8; i++) {
        v[i] = state->h[i];
    }

    v[8] = blake2b_iv[0];
    v[9] = blake2b_iv[1];
    v[10] = blake2b_iv[2];
    v[11] = blake2b_iv[3];
    v[12] = blake2b_iv[4] ^ state->t[0];
    v[13] = blake2b_iv[5] ^ state->t[1];
    v[14] = blake2b_iv[6] ^ state->f[0];
    v[15] = blake2b_iv[7] ^ state->f[1];

    ROUND(0);
    ROUND(1);
    ROUND(2);
    ROUND(3);
    ROUND(4);
    ROUND(5);
    ROUND(6);
    ROUND(7);
    ROUND(8);
    ROUND(9);
    ROUND(10);
    ROUND(11);

    for (int i = 0; i < 8; i++) {
        state->h[i] = state->h[i] ^ v[i] ^ v[i + 8];
    }
}

#undef G
#undef ROUND

static void blake2b_init(struct blake2b_state *state) {
    struct blake2b_param param = {0};

    param.digest_length = BLAKE2B_OUT_BYTES;
    param.fan_out = 1;
    param.depth = 1;

    memset(state, 0, sizeof(struct blake2b_state));

    for (int i = 0; i < 8; i++) {
        state->h[i] = blake2b_iv[i];
    }

    for (int i = 0; i < 8; i++) {
        state->h[i] ^= *(uint64_t *)((void *)&param + sizeof(state->h[i]) * i);
    }
}

static void blake2b_update(struct blake2b_state *state, const void *in, size_t in_len) {
    if (in_len == 0) {
        return;
    }

    size_t left = state->buf_len;
    size_t fill = BLAKE2B_BLOCK_BYTES - left;

    if (in_len > fill) {
        state->buf_len = 0;

        memcpy(state->buf + left, in, fill);
        blake2b_increment_counter(state, BLAKE2B_BLOCK_BYTES);
        blake2b_compress(state, state->buf);

        in += fill;
        in_len -= fill;

        while (in_len > BLAKE2B_BLOCK_BYTES) {
            blake2b_increment_counter(state, BLAKE2B_BLOCK_BYTES);
            blake2b_compress(state, in);

            in += fill;
            in_len -= fill;
        }
    }

    memcpy(state->buf + state->buf_len, in, in_len);
    state->buf_len += in_len;
}

static void blake2b_final(struct blake2b_state *state, void *out) {
    uint8_t buffer[BLAKE2B_OUT_BYTES] = {0};

    blake2b_increment_counter(state, state->buf_len);
    state->f[0] = (uint64_t)-1;
    memset(state->buf + state->buf_len, 0, BLAKE2B_BLOCK_BYTES - state->buf_len);
    blake2b_compress(state, state->buf);

    for (int i = 0; i < 8; i++) {
        *(uint64_t *)(buffer + sizeof(state->h[i]) * i) = state->h[i];
    }

    memcpy(out, buffer, BLAKE2B_OUT_BYTES);
    memset(buffer, 0, sizeof(buffer));
}

static void blake2b(void *out, const void *in, size_t in_len) {
    struct blake2b_state state = {0};

    blake2b_init(&state);
    blake2b_update(&state, in, in_len);
    blake2b_final(&state, out);
}

static bool config_ready;

struct menu_entry {
    char name[64];
    char *comment;
    struct menu_entry *parent;
    struct menu_entry *sub;
    bool expanded;
    char *body;
    struct menu_entry *next;
};

struct conf_tuple {
    char *value1;
    char *value2;
};
static struct menu_entry *menu_tree = NULL;

struct macro {
    char name[1024];
    char value[2048];
    struct macro *next;
};



static char *config_get_value(const char *config, size_t index, const char *key);
static struct conf_tuple config_get_tuple(const char *config, size_t index,
                                   const char *key1, const char *key2);

#define BLAKE2B_OUT_BYTES 64
#define CONFIG_B2SUM_SIGNATURE "++CONFIG_B2SUM_SIGNATURE++"
#define CONFIG_B2SUM_EMPTY "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"

static const char *config_b2sum = CONFIG_B2SUM_SIGNATURE CONFIG_B2SUM_EMPTY;

static bool config_get_entry_name(char *ret, size_t index, size_t limit);
static char *config_get_entry(size_t *size, size_t index);

#define SEPARATOR '\n'


static no_unwind bool bad_config = false;

static char *config_addr;

#define NOT_CHILD      (-1)
#define DIRECT_CHILD   0
#define INDIRECT_CHILD 1

static int is_child(char *buf, size_t limit,
                    size_t current_depth, size_t index) {
    if (!config_get_entry_name(buf, index, limit))
        return NOT_CHILD;
    if (strlen(buf) < current_depth + 1)
        return NOT_CHILD;
    for (size_t j = 0; j < current_depth; j++)
        if (buf[j] != ':')
            return NOT_CHILD;
    if (buf[current_depth] == ':')
        return INDIRECT_CHILD;
    return DIRECT_CHILD;
}

static bool is_directory(char *buf, size_t limit,
                         size_t current_depth, size_t index) {
    switch (is_child(buf, limit, current_depth + 1, index + 1)) {
        default:
        case NOT_CHILD:
            return false;
        case INDIRECT_CHILD:
            bad_config = true;
            panic(true, "config: Malformed config file. Parentless child.");
        case DIRECT_CHILD:
            return true;
    }
}

static struct menu_entry *create_menu_tree(struct menu_entry *parent,
                                           size_t current_depth, size_t index) {
    struct menu_entry *root = NULL, *prev = NULL;

    for (size_t i = index; ; i++) {
        static char name[64];

        switch (is_child(name, 64, current_depth, i)) {
            case NOT_CHILD:
                return root;
            case INDIRECT_CHILD:
                continue;
            case DIRECT_CHILD:
                break;
        }

        struct menu_entry *entry = ext_mem_alloc(sizeof(struct menu_entry));

        if (root == NULL)
            root = entry;

        config_get_entry_name(name, i, 64);

        bool default_expanded = name[current_depth] == '+';

        strcpy(entry->name, name + current_depth + default_expanded);
        entry->parent = parent;

        size_t entry_size;
        char *config_entry = config_get_entry(&entry_size, i);
        entry->body = ext_mem_alloc(entry_size + 1);
        memcpy(entry->body, config_entry, entry_size);
        entry->body[entry_size] = 0;

        if (is_directory(name, 64, current_depth, i)) {
            entry->sub = create_menu_tree(entry, current_depth + 1, i + 1);
            entry->expanded = default_expanded;
        }

        char *comment = config_get_value(entry->body, 0, "COMMENT");
        if (comment != NULL) {
            entry->comment = comment;
        }

        if (prev != NULL)
            prev->next = entry;
        prev = entry;
    }
}



static struct macro *macros = NULL;
bool editor_enabled;
static int init_config(size_t config_size) {
    
    config_b2sum += sizeof(CONFIG_B2SUM_SIGNATURE) - 1;

    if (memcmp((void *)config_b2sum, CONFIG_B2SUM_EMPTY, 128) != 0) {
        editor_enabled = false;
        
        uint8_t out_buf[BLAKE2B_OUT_BYTES];
        blake2b(out_buf, config_addr, config_size - 2);
        uint8_t hash_buf[BLAKE2B_OUT_BYTES];

        for (size_t i = 0; i < BLAKE2B_OUT_BYTES; i++) {
            hash_buf[i] = digit_to_int(config_b2sum[i * 2]) << 4 | digit_to_int(config_b2sum[i * 2 + 1]);
        }

        if (memcmp(hash_buf, out_buf, BLAKE2B_OUT_BYTES) != 0) {
            
            panic(false, "!!! CHECKSUM MISMATCH FOR CONFIG FILE !!!");
        }
    }
    // kAFL_hypercall(HYPERCALL_KAFL_PRINTF, "!!! CHECKSUM MISMATCH FOR CONFIG FILE !!!");
    // add trailing newline if not present
    config_addr[config_size - 2] = '\n';

    // remove windows carriage returns and spaces at the start and end of lines, if any
    for (size_t i = 0; i < config_size; i++) {
        size_t skip = 0;
        if (config_addr[i] == ' ' || config_addr[i] == '\t') {
            while (config_addr[i + skip] == ' ' || config_addr[i + skip] == '\t') {
                skip++;
            }
            if (config_addr[i + skip] == '\n') {
                goto skip_loop;
            }
            skip = 0;
        }
        while ((config_addr[i + skip] == '\r')
            || ((!i || config_addr[i - 1] == '\n') && (config_addr[i + skip] == ' ' || config_addr[i + skip] == '\t'))
        ) {
            skip++;
        }
skip_loop:
        if (skip) {
            for (size_t j = i; j < config_size - skip; j++)
                config_addr[j] = config_addr[j + skip];
            config_size -= skip;
        }
    }

    // Load macros
    struct macro *arch_macro = ext_mem_alloc(sizeof(struct macro));
    strcpy(arch_macro->name, "ARCH");
#if defined (__x86_64__)
    strcpy(arch_macro->value, "x86-64");
#elif defined (__i386__)
    {
    uint32_t eax, ebx, ecx, edx;
    if (!cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx) || !(edx & (1 << 29))) {
        strcpy(arch_macro->value, "ia-32");
    } else {
        strcpy(arch_macro->value, "x86-64");
    }
    }
#elif defined (__aarch64__)
    strcpy(arch_macro->value, "aarch64");
#elif defined (__riscv64)
    strcpy(arch_macro->value, "riscv64");
#else
#error "Unspecified architecture"
#endif
    arch_macro->next = macros;
    macros = arch_macro;

    for (size_t i = 0; i < config_size;) {
        if ((config_size - i >= 3 && memcmp(config_addr + i, "\n${", 3) == 0)
         || (config_size - i >= 2 && i == 0 && memcmp(config_addr, "${", 2) == 0)) {
            struct macro *macro = ext_mem_alloc(sizeof(struct macro));

            i += i ? 3 : 2;
            size_t j;
            for (j = 0; config_addr[i] != '}' && config_addr[i] != '\n' && config_addr[i] != 0; j++, i++) {
                macro->name[j] = config_addr[i];
            }

            if (config_addr[i] == '\n' || config_addr[i] == 0 || config_addr[i+1] != '=') {
                continue;
            }
            i += 2;

            macro->name[j] = 0;

            for (j = 0; config_addr[i] != '\n' && config_addr[i] != 0; j++, i++) {
                macro->value[j] = config_addr[i];
            }
            macro->value[j] = 0;

            macro->next = macros;
            macros = macro;

            continue;
        }

        i++;
    }

    // Expand macros
    if (macros != NULL) {
        size_t new_config_size = config_size * 4;
        char *new_config = ext_mem_alloc(new_config_size);

        size_t i, in;
        for (i = 0, in = 0; i < config_size;) {
            if ((config_size - i >= 3 && memcmp(config_addr + i, "\n${", 3) == 0)
             || (config_size - i >= 2 && i == 0 && memcmp(config_addr, "${", 2) == 0)) {
                size_t orig_i = i;
                i += i ? 3 : 2;
                while (config_addr[i++] != '}') {
                    if (i >= config_size) {
                        bad_config = true;
                        panic(true, "config: Malformed macro usage");
                    }
                }
                if (config_addr[i++] != '=') {
                    i = orig_i;
                    goto next;
                }
                while (config_addr[i] != '\n' && config_addr[i] != 0) {
                    i++;
                    if (i >= config_size) {
                        bad_config = true;
                        panic(true, "config: Malformed macro usage");
                    }
                }
                continue;
            }

next:
            if (config_size - i >= 2 && memcmp(config_addr + i, "${", 2) == 0) {
                char *macro_name = ext_mem_alloc(1024);
                i += 2;
                size_t j;
                for (j = 0; config_addr[i] != '}' && config_addr[i] != '\n' && config_addr[i] != 0; j++, i++) {
                    macro_name[j] = config_addr[i];
                }
                if (config_addr[i] != '}') {
                    bad_config = true;
                    panic(true, "config: Malformed macro usage");
                }
                i++;
                macro_name[j] = 0;
                char *macro_value = "";
                struct macro *macro = macros;
                for (;;) {
                    if (macro == NULL) {
                        break;
                    }
                    if (strcmp(macro->name, macro_name) == 0) {
                        macro_value = macro->value;
                        break;
                    }
                    macro = macro->next;
                }
                pmm_free(macro_name, 1024);
                for (j = 0; macro_value[j] != 0; j++, in++) {
                    if (in >= new_config_size) {
                        goto overflow;
                    }
                    new_config[in] = macro_value[j];
                }
                continue;
            }

            if (in >= new_config_size) {
overflow:
                bad_config = true;
                panic(true, "config: Macro-induced buffer overflow");
            }
            new_config[in++] = config_addr[i++];
        }

        pmm_free(config_addr, config_size);

        config_addr = new_config;
        config_size = in;

        // Free macros
        struct macro *macro = macros;
        for (;;) {
            if (macro == NULL) {
                break;
            }
            struct macro *next = macro->next;
            pmm_free(macro, sizeof(struct macro));
            macro = next;
        }
    }

    config_ready = true;

    menu_tree = create_menu_tree(NULL, 1, 0);

    size_t s;
    char *c = config_get_entry(&s, 0);
    while (*c != ':') {
        c--;
    }
    if (c > config_addr) {
        c[-1] = 0;
    }

    return 0;
}

static bool config_get_entry_name(char *ret, size_t index, size_t limit) {
    if (!config_ready)
        return false;

    char *p = config_addr;

    for (size_t i = 0; i <= index; i++) {
        while (*p != ':') {
            if (!*p)
                return false;
            p++;
        }
        p++;
        if ((p - 1) != config_addr && *(p - 2) != '\n')
            i--;
    }

    p--;

    size_t i;
    for (i = 0; i < (limit - 1); i++) {
        if (p[i] == SEPARATOR)
            break;
        ret[i] = p[i];
    }

    ret[i] = 0;
    return true;
}

static char *config_get_entry(size_t *size, size_t index) {
    if (!config_ready)
        return NULL;

    char *ret;
    char *p = config_addr;

    for (size_t i = 0; i <= index; i++) {
        while (*p != ':') {
            if (!*p)
                return NULL;
            p++;
        }
        p++;
        if ((p - 1) != config_addr && *(p - 2) != '\n')
            i--;
    }

    do {
        p++;
    } while (*p != '\n');

    ret = p;

cont:
    while (*p != ':' && *p)
        p++;

    if (*p && *(p - 1) != '\n') {
        p++;
        goto cont;
    }

    *size = p - ret;

    return ret;
}

static const char *lastkey;

struct conf_tuple config_get_tuple(const char *config, size_t index,
                                   const char *key1, const char *key2) {
    struct conf_tuple conf_tuple;

    conf_tuple.value1 = config_get_value(config, index, key1);
    if (conf_tuple.value1 == NULL) {
        return (struct conf_tuple){0};
    }

    conf_tuple.value2 = config_get_value(lastkey, 0, key2);

    const char *lk1 = lastkey;

    const char *next_value1 = config_get_value(config, index + 1, key1);

    const char *lk2 = lastkey;

    if (conf_tuple.value2 != NULL && next_value1 != NULL) {
        if ((uintptr_t)lk1 > (uintptr_t)lk2) {
            conf_tuple.value2 = NULL;
        }
    }

    return conf_tuple;
}

static char *config_get_value(const char *config, size_t index, const char *key) {
    if (!key || !config_ready)
        return NULL;

    if (config == NULL)
        config = config_addr;

    size_t key_len = strlen(key);

    for (size_t i = 0; config[i]; i++) {
        if (!strncmp(&config[i], key, key_len) && config[i + key_len] == '=') {
            if (i && config[i - 1] != SEPARATOR)
                continue;
            if (index--)
                continue;
            i += key_len + 1;
            size_t value_len;
            for (value_len = 0;
                 config[i + value_len] != SEPARATOR && config[i + value_len];
                 value_len++);
            char *buf = ext_mem_alloc(value_len + 1);
            memcpy(buf, config + i, value_len);
            lastkey = config + i;
            return buf;
        }
    }

    return NULL;
}
#endif


#if defined (BIOS)

bool stage3_loaded = false;
static bool stage3_found = false;

extern symbol stage3_addr;
extern symbol limine_bios_sys_size;
extern symbol build_id_s2;
extern symbol build_id_s3;

static bool stage3_init(struct volume *part) {
    struct file_handle *stage3;

    bool old_cif = case_insensitive_fopen;
    case_insensitive_fopen = true;
    if ((stage3 = fopen(part, "/limine-bios.sys")) == NULL
     && (stage3 = fopen(part, "/limine/limine-bios.sys")) == NULL
     && (stage3 = fopen(part, "/boot/limine-bios.sys")) == NULL
     && (stage3 = fopen(part, "/boot/limine/limine-bios.sys")) == NULL) {
        case_insensitive_fopen = old_cif;
        return false;
    }
    case_insensitive_fopen = old_cif;

    stage3_found = true;

    if (stage3->size != (size_t)limine_bios_sys_size) {
        print("limine-bios.sys size incorrect.\n");
        return false;
    }

    fread(stage3, stage3_addr,
          (uintptr_t)stage3_addr - 0xf000,
          stage3->size - ((uintptr_t)stage3_addr - 0xf000));

    fclose(stage3);

    if (memcmp(build_id_s2 + 16, build_id_s3 + 16, 20) != 0) {
        print("limine-bios.sys build ID mismatch.\n");
        return false;
    }

    stage3_loaded = true;

    return true;
}

enum {
    BOOTED_FROM_HDD = 0,
    BOOTED_FROM_PXE = 1,
    BOOTED_FROM_CD = 2
};

#include "nyx_api.h"
#include "paging.h"
kAFL_payload *payload_buffer;
int fuzz_start = 0;
noreturn void entry(uint8_t boot_drive, int boot_from) {
    // XXX DO NOT MOVE A20 ENABLE CALL
    
    if (!a20_enable()) {
        panic(false, "Could not enable A20 line");
    }

    init_e820();
    init_memmap();

    


    //-----------------
    struct page_dir *page_dir_ptr =  (struct page_dir *)ext_mem_alloc(sizeof(struct page_dir));
    struct page_table_list *page_table_list_ptr =  (struct page_table_list *)ext_mem_alloc(sizeof(struct page_table_list));
    struct idt* idt_ptr = (struct idt *)ext_mem_alloc(sizeof(struct idt));

    init_paging(page_dir_ptr,page_table_list_ptr);
    for(unsigned long i = 0 ; i < 0x20000000 ; i+= 4096) 
    {
        map_unmap_page(i, 1);
    }
    enable_paging();
    
    init_fuzz_intc(idt_ptr);
    volatile host_config_t host_config;
    volatile agent_config_t agent_config = {0};

    kAFL_hypercall(HYPERCALL_KAFL_ACQUIRE, 0);
    kAFL_hypercall(HYPERCALL_KAFL_RELEASE, 0);

    kAFL_hypercall(HYPERCALL_KAFL_USER_SUBMIT_MODE, KAFL_MODE_32);


    kAFL_hypercall(HYPERCALL_KAFL_GET_HOST_CONFIG, (uint32_t)&host_config);
    if (host_config.host_magic != NYX_HOST_MAGIC || host_config.host_version != NYX_HOST_VERSION) 
    {
        habort("magic error\n");
    }
    agent_config.agent_magic = NYX_AGENT_MAGIC;
    agent_config.agent_version = NYX_AGENT_VERSION;

    agent_config.agent_tracing = 0;
    agent_config.agent_ijon_tracing = 0;
    agent_config.agent_non_reload_mode = 0; // persistent fuzz!
    agent_config.coverage_bitmap_size = host_config.bitmap_size;

    payload_buffer = ext_mem_alloc(host_config.payload_buffer_size);
    kAFL_hypercall (HYPERCALL_KAFL_SET_AGENT_CONFIG, (uint32_t)&agent_config);

    uint64_t buffer_kernel[3] = {0};


    buffer_kernel[0] = 0x7000;
    buffer_kernel[1] = 0x20000000;
    buffer_kernel[2] = 0;
    kAFL_hypercall(HYPERCALL_KAFL_RANGE_SUBMIT, (uint32_t)buffer_kernel);
    kAFL_hypercall (HYPERCALL_KAFL_GET_PAYLOAD, (uint32_t)payload_buffer);
    
    kAFL_hypercall (HYPERCALL_KAFL_NEXT_PAYLOAD, 0);
    kAFL_hypercall (HYPERCALL_KAFL_ACQUIRE, 0);

    //file system harness
    
    fuzz_start = 1;

    disk_create_index();
    struct volume *drive = volume_get_by_coord(false, 1, 1);
    if (drive)
    {
        
        struct file_handle *f = fopen(drive,"/a.txt");
        if(f)
        {
            kAFL_hypercall(HYPERCALL_KAFL_PRINTF, "kAFL fuzzer initialized.");
            uint8_t buf[6];
            fread(f, buf,
                0,
                6);
            fclose(f);
            print("data %x %x\n",buf[0],buf[1]);
        }
        f = fopen(drive,"/a.txt.ln");
        if(f)
        {
            uint8_t buf[6];
            fread(f, buf,
                0,
                6);
            fclose(f);
            print("data %x %x\n",buf[0],buf[1]);
        }
        
    }
    
    
//    config file harness
//    config_addr = payload_buffer->data;
//    init_config(payload_buffer->size);
//    config_get_value(NULL, 0, "EDITOR_HIGHLIGHTING");
//    config_get_value(NULL, 0, "EDITOR_VALIDATION");
//    config_get_tuple(0, 0,"MODULE_PATH", "MODULE_CMDLINE");
//    config_get_tuple(0, 0, "MODULE_PATH", "MODULE_STRING");
    
   // pic harness
    // int x, y, bpp;

    // stbi_load_from_memory(payload_buffer->data, payload_buffer->size, &x, &y, &bpp, 4);
    

    kAFL_hypercall (HYPERCALL_KAFL_RELEASE, 0);

    
    

    while(1)
    {
        ;
    }
    disk_create_index();

    if (boot_from == BOOTED_FROM_HDD || boot_from == BOOTED_FROM_CD) {
        boot_volume = volume_get_by_bios_drive(boot_drive);
    } else if (boot_from == BOOTED_FROM_PXE) {
        pxe_init();
        boot_volume = pxe_bind_volume();
    }

    if (boot_volume == NULL) {
        panic(false, "Could not determine boot drive");
    }

    volume_iterate_parts(boot_volume,
        if (stage3_init(_PART)) {
            break;
        }
    );

    if (!stage3_found) {
        print("\n"
              "!! Stage 3 file not found!\n"
              "!! Have you copied limine-bios.sys to the root, /boot, /limine, or /boot/limine\n"
              "!! directories of one of the partitions on the boot device?\n\n");
    }

    if (!stage3_loaded) {
        panic(false, "Failed to load stage 3.");
    }

    term_fallback();

    stage3_common();
    
}
void rm_int(uint8_t int_no, struct rm_regs *out_regs, struct rm_regs *in_regs)
{
    if (!fuzz_start)
    {
        disable_paging();
        rm_int2(int_no,out_regs,in_regs);
        enable_paging();
        return;
    }
    uint32_t drive = in_regs->edx;
    void *data = (void *)((in_regs->ds << 4) + in_regs->esi);

    if(int_no == 0x13)
    {
        // if(drive == 0x80)
        // {
        //     disable_paging();
        //     rm_int2(int_no,out_regs,in_regs);
        //     enable_paging();
        //     return;
        // }
        
        if(drive == 0x80)
        {
            if((in_regs->eax & 0xff00) == 0x4200)
            {
                struct dap_ *dap = (struct dap_*)data;
                if((dap->lba + dap->count) * 512 > payload_buffer->size)
                {
                    out_regs->eflags |= EFLAGS_CF;
                    return;
                }
                void *buf = (void *)((dap->segment << 4) + dap->offset);
                memcpy(buf,payload_buffer->data + dap->lba * 512,dap->count * 512);
                out_regs->eflags &= ~EFLAGS_CF;
                return;
                
            }
            else if((in_regs->eax & 0xff00) == 0x4300)
            {
                struct dap_ *dap = (struct dap_*)data;
                if((dap->lba + dap->count) * 512 > payload_buffer->size)
                {
                    out_regs->eflags |= EFLAGS_CF;
                    return;
                }
                void *buf = (void *)((dap->segment << 4) + dap->offset);
                memcpy(payload_buffer->data + dap->lba * 512,buf,dap->count * 512);
                out_regs->eflags &= ~EFLAGS_CF;
                return;
            }
            else if((in_regs->eax & 0xff00) == 0x4800)
            {
                struct bios_drive_params_ *params = (struct bios_drive_params_ *)data;
                params->lba_count = 128;
                out_regs->eflags &= ~EFLAGS_CF;
                return;
            }
            
            else
            {
                out_regs->eflags |= EFLAGS_CF;
                return;
            }

        }
        
        else
        {
            out_regs->eflags |= EFLAGS_CF;
            return;
        }
        
    }
    else
    {
        // disable_paging();
        // rm_int2(int_no,out_regs,in_regs);
        // enable_paging();
        out_regs->eflags |= EFLAGS_CF;
        return;
    }

}

#endif
