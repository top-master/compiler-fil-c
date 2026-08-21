/* Covers zsys_dl_find_object: it walks the loaded-object program headers (via the runtime's
   dl_iterate_phdr) to find the object whose PT_LOAD segments cover an address, and reports that
   object's mapping range and PT_GNU_EH_FRAME in a Fil-C struct dl_find_object. The opaque address
   fields are forged the same way as zsys_dladdr's dli_fbase, so we read their integer values but
   never dereference them. */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdfil.h>

int main(void)
{
    /* Resolve the object that contains main() itself. main's code lives in a PT_LOAD segment of
       the main program, so this must succeed. */
    struct dl_find_object fo;
    int r = _dl_find_object((void*)&main, &fo);
    ZASSERT(r == 0);

    uintptr_t addr = (uintptr_t)(void*)&main;
    uintptr_t lo = (uintptr_t)fo.dlfo_map_start;
    uintptr_t hi = (uintptr_t)fo.dlfo_map_end;

    /* The reported mapping must be a non-empty range that actually covers the queried address. */
    ZASSERT(lo != 0);
    ZASSERT(hi > lo);
    ZASSERT(addr >= lo);
    ZASSERT(addr < hi);

    /* The object's opaque handle (its load base) is reported. */
    ZASSERT(fo.dlfo_link_map != 0);

    /* Fields the runtime writes explicitly: flags cleared, no sframe. */
    ZASSERT(fo.dlfo_flags == 0);
    ZASSERT(fo.dlfo_sframe == 0);

    /* An address that belongs to no loaded object (a heap/stack address is not inside any object's
       PT_LOAD range) must report "not found". */
    struct dl_find_object miss;
    int r2 = _dl_find_object((void*)&fo, &miss);
    ZASSERT(r2 == -1);

    printf("dl_find_object resolved main in [%p, %p).\n", fo.dlfo_map_start, fo.dlfo_map_end);
    return 0;
}
