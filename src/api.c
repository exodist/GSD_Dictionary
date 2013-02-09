#include "include/gsd_dict.h"
#include "include/gsd_dict_return.h"
#include "balance.h"
#include "dot.h"
#include "epoch.h"
#include "free.h"
#include "location.h"
#include "structures.h"
#include "util.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// -- Creation and meta data --

int dict_free( dict **dr ) {
    dict *d = *dr;
    *dr = NULL;

    // Wait on all epochs
    epoch *e = d->epochs;
    while ( e != NULL ) {
        while( e->active ) sleep( 0 );
        e = e->next;
    }

    if ( d->set != NULL ) dict_free_set( d, d->set );
    free( d );

    return DICT_NO_ERROR;
}

int dict_create( dict **d, uint8_t epoch_limit, dict_settings *settings, dict_methods *methods ) {
    if ( settings == NULL )     return DICT_API_ERROR;
    if ( methods == NULL )      return DICT_API_ERROR;
    if ( methods->cmp == NULL ) return DICT_API_ERROR;
    if ( methods->loc == NULL ) return DICT_API_ERROR;
    if ( epoch_limit && epoch_limit < 4 ) return DICT_API_ERROR;

    return dict_do_create( d, epoch_limit, settings, methods );
}

// Copying and cloning
int dict_merge( dict *from, dict *to );
int dict_merge_refs( dict *from, dict *to );

// -- Informational --

char *dict_dump_dot( dict *d, dict_dot *decode ) {
    epoch *e = dict_join_epoch( d );
    set *s = d->set;

    if( !decode ) decode = dict_dump_node_label;

    char *out = dict_do_dump_dot( d, s, decode );

    dict_leave_epoch( d, e );
    return out;
}

// -- Operation --

// Chance to handle pathological data gracefully
int dict_reconfigure( dict *d, dict_settings *settings ) {
    return DICT_UNIMP_ERROR;
}

int dict_get( dict *d, void *key, void **val ) {
    location *loc = NULL;
    int err = dict_locate( d, key, &loc );

    if ( !err ) {
        if ( loc->sref == NULL ) {
            *val = NULL;
        }
        else {
            *val = loc->sref->value;
        }
    }

    // Free our locator
    if ( loc != NULL ) dict_free_location( d, loc );

    return err;
}

int dict_set( dict *d, void *key, void *val ) {
    location *locator = NULL;
    int err = dict_do_set( d, key, NULL, val, 1, 1, &locator );
    if ( locator != NULL ) {
        dict_free_location( d, locator );
    }
    return err;
}

int dict_insert( dict *d, void *key, void *val ) {
    location *locator = NULL;
    int err = dict_do_set( d, key, NULL, val, 0, 1, &locator );
    if ( locator != NULL ) {
        dict_free_location( d, locator );
    }
    return err;
}

int dict_update( dict *d, void *key, void *val ) {
    location *locator = NULL;
    int err = dict_do_set( d, key, NULL, val, 1, 0, &locator );
    if ( locator != NULL ) dict_free_location( d, locator );
    return err;
}

int dict_delete( dict *d, void *key ) {
    location *locator = NULL;
    int err = dict_do_set( d, key, NULL, NULL, 1, 0, &locator );
    if ( locator != NULL ) dict_free_location( d, locator );
    return err;
}

int dict_cmp_update( dict *d, void *key, void *old_val, void *new_val ) {
    if ( old_val == NULL ) return DICT_API_ERROR;
    location *locator = NULL;
    int err = dict_do_set( d, key, old_val, new_val, 1, 0, &locator );
    if ( locator != NULL ) dict_free_location( d, locator );
    return err;
}

int dict_cmp_delete( dict *d, void *key, void *old_val ) {
    location *locator = NULL;
    int err = dict_do_set( d, key, old_val, NULL, 1, 0, &locator );
    if ( locator != NULL ) dict_free_location( d, locator );
    return err;
}

int dict_reference( dict *orig, void *okey, dict *dest, void *dkey ) {
    location *oloc = NULL;
    location *dloc = NULL;

    // Find item in orig, insert if necessary
    int err1 = dict_do_set( orig, okey, NULL, NULL, 0, 1, &oloc );
    // Find item in dest, insert if necessary
    int err2 = dict_do_set( dest, dkey, NULL, NULL, 0, 1, &dloc );

    // Ignore rebalance errors.. might want to readdress this.
    if ( err1 > 100 ) err1 = 0;
    if ( err2 > 100 ) err2 = 0;

    // Transaction error from above simply means it already exists
    if ( err1 == DICT_TRANS_FAIL ) err1 = 0;
    if ( err2 == DICT_TRANS_FAIL ) err2 = 0;

    if ( !err1 && !err2 ) {
        dict_do_deref( dest, dkey, dloc, oloc->usref->sref );
    }

    if ( oloc != NULL ) dict_free_location( orig, oloc );
    if ( dloc != NULL ) dict_free_location( dest, dloc );

    if ( err1 ) return err1;
    if ( err2 ) return err2;

    return DICT_NO_ERROR;
}

int dict_dereference( dict *d, void *key ) {
    location *loc = NULL;
    int err = dict_locate( d, key, &loc );

    if ( !err && loc->sref != NULL ) err = dict_do_deref( d, key, loc, NULL );

    if ( loc != NULL ) dict_free_location( d, loc );
    return err;
}

int dict_iterate( dict *d, dict_handler *h, void *args ) {
    epoch *e = dict_join_epoch( d );
    set *s = d->set;
    int stop = DICT_NO_ERROR;

    for ( int i = 0; i < s->settings->slot_count; i++ ) {
        slot *sl = s->slots[i];
        if ( sl == NULL ) continue;
        node *n = sl->root;
        if ( n == NULL ) continue;
        stop = dict_iterate_node( d, n, h, args );
        if ( stop ) break;
    }

    dict_leave_epoch( d, e );
    return stop;
}
