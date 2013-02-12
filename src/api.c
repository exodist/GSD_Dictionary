#include "include/gsd_dict.h"

#include "alloc.h"
#include "dot.h"
#include "merge.h"
#include "operations.h"
#include "settings.h"
#include "structure.h"

rstat dict_create( dict **d, uint8_t epoch_limit, dict_settings *settings, dict_methods *methods ) {
    return do_create( d, epoch_limit, settings, methods );
}

rstat merge( dict *from, dict *to ) {
    return rstat_unimp;
}

rstat merge_refs( dict *from, dict *to ) {
    return rstat_unimp;
}

rstat dict_free( dict **d ) {
    return do_free( d );
}

dict_settings *dict_get_settings( dict *d ) {
    return get_settings( d );
}

dict_methods *dict_get_methods( dict *d ) {
    return get_methods( d );
}

char *dict_dump_dot( dict *d, dict_dot *decode ) {
    return dump_dot( d, decode );
}

rstat dict_reconfigure( dict *d, dict_settings *settings ) {
    return reconfigure( d, settings );
}

rstat dict_get( dict *d, void *key, void **val ) {
    return op_get( d, key, val );
}
rstat dict_set( dict *d, void *key, void *val ) {
    return op_set( d, key, val );
}
rstat dict_insert( dict *d, void *key, void *val ) {
    return op_insert( d, key, val );
}
rstat dict_update( dict *d, void *key, void *val ) {
    return op_update( d, key, val );
}
rstat dict_delete( dict *d, void *key ) {
    return op_delete( d, key );
}

rstat dict_cmp_update( dict *d, void *key, void *old_val, void *new_val ) {
    return op_cmp_update( d, key, old_val, new_val );
}
rstat dict_cmp_delete( dict *d, void *key, void *old_val ) {
    return op_cmp_delete( d, key, old_val );
}

rstat dict_reference( dict *orig, void *okey, dict *dest, void *dkey ) {
    return op_reference( orig, okey, dest, dkey );
}
rstat dict_dereference( dict *d, void *key ) {
    return op_dereference( d, key );
}

rstat dict_iterate( dict *d, dict_handler *h, void *args ) {
    return dict_iterate( d, h, args );
}

