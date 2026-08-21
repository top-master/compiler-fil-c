/* Covers zsys_dl_iterate_phdr: the runtime enumerates the loaded objects and rebuilds each
   struct dl_phdr_info in freshly-allocated Fil-C objects, so the dlpi_name string and the
   dlpi_phdr array handed to the callback carry valid, dereferenceable capabilities. The old
   static_dl_iterate_phdr stub used to panic, so merely reaching the callback already exercises
   the new path; we additionally prove the pointers are real and that the callback's return value
   stops and propagates. */

#include <link.h>
#include <elf.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdfil.h>

struct ctx {
    int count;
    int found_load;
    int data_ok;
};

static int count_cb(struct dl_phdr_info* info, size_t size, void* data)
{
    struct ctx* c = (struct ctx*)data;
    c->data_ok = 1; /* the runtime forwarded our data pointer unchanged */

    /* The runtime hands back a full struct dl_phdr_info, so size must at least describe the
       original four ABI fields. */
    ZASSERT(size >= offsetof(struct dl_phdr_info, dlpi_phnum) + sizeof(info->dlpi_phnum));

    c->count++;

    /* dlpi_name must be a real Fil-C capability we can dereference (it may be the empty string for
       the main program). If the capability were bogus, this strlen would trap. */
    ZASSERT(info->dlpi_name);
    volatile size_t namelen = strlen(info->dlpi_name);
    (void)namelen;

    /* dlpi_phdr must point at a valid array of dlpi_phnum entries we can read end to end. Every
       loaded object has at least one PT_LOAD segment. */
    unsigned i;
    for (i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)* ph = info->dlpi_phdr + i;
        volatile unsigned type = ph->p_type;
        volatile unsigned long memsz = ph->p_memsz;
        (void)memsz;
        if (type == PT_LOAD)
            c->found_load = 1;
    }
    return 0; /* keep enumerating */
}

static int stop_cb(struct dl_phdr_info* info, size_t size, void* data)
{
    (void)info;
    (void)size;
    int* n = (int*)data;
    (*n)++;
    return 123; /* nonzero: enumeration must stop here and hand this value back */
}

int main(void)
{
    struct ctx c;
    c.count = 0;
    c.found_load = 0;
    c.data_ok = 0;

    int r = dl_iterate_phdr(count_cb, &c);
    ZASSERT(r == 0);
    ZASSERT(c.data_ok);
    ZASSERT(c.count >= 1);   /* at minimum the main program is enumerated */
    ZASSERT(c.found_load);   /* and its phdrs were readable */

    /* A nonzero callback return stops enumeration immediately and is propagated to the caller. */
    int n = 0;
    r = dl_iterate_phdr(stop_cb, &n);
    ZASSERT(r == 123);
    ZASSERT(n == 1);

    printf("dl_iterate_phdr saw %d objects.\n", c.count);
    return 0;
}
