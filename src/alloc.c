#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "devtools.h"
#include "structure.h"
#include "alloc.h"
#include "epoch.h"
#include "balance.h"
#include "error.h"
#include "location.h"
#include "node_list.h"

//struct allocations {
//    set   *sets;
//    slot  *slots;
//    xtrn  *xtrns;
//    node  *nodes;
//    usref *usrefs;
//    sref  *srefs;
//
//    location *locations;
//
//    nlist *nlists;
//    nlist_item *nlist_items;
//};

#define SET_BLOCK        4
#define SLOT_BLOCK       128
#define NODE_BLOCK       128
#define USREF_BLOCK      128
#define SREF_BLOCK       128
#define XTRN_BLOCK       256
#define LOCATION_BLOCK   8
#define NLIST_BLOCK      8
#define NLIST_ITEM_BLOCK 128

void *do_alloc( alloc_block **from, size_t size, type t, size_t block_size ) {
    if ( size < sizeof( trash )) size = sizeof(trash);

    while( 1 ) {
        alloc_block *b = *from;
        if( b ) {
            uint8_t *start = (uint8_t *)(b + 1);

            size_t idx = __sync_fetch_and_add( &(b->idx), 1 );
            if ( idx < block_size ) {
                return start + ( idx * size );
            }

            trash *top = b->top;
            while ( top ) {
                if (__sync_bool_compare_and_swap( &(top->type), FREE, t )) {
                    if( __sync_bool_compare_and_swap( &(b->top), top, top->next )) {
                        return top;
                    }
                    top->type = FREE;
                }
                top = b->top;
            }
        }

        // malloc some more.
        alloc_block *new = malloc( sizeof(alloc_block) + (size * block_size) );
        if ( new == NULL ) return NULL;
        memset( new, 0, size * block_size );
        new->size = size * block_size;
        new->next = *from;

        if(__sync_bool_compare_and_swap( from, new->next, new ))
            continue;

        free( new );
    }
}

void *galloc( dict *d, type t ) {
    switch( t ) {
        case SET:
            return do_alloc( &(d->alloc->sets), sizeof( set ), SET, SET_BLOCK);
        case SLOT:
            return do_alloc( &(d->alloc->slots), sizeof( slot ), SLOT, SLOT_BLOCK);
        case NODE:
            return do_alloc( &(d->alloc->nodes), sizeof( node ), NODE, NODE_BLOCK);
        case USREF:
            return do_alloc( &(d->alloc->usrefs), sizeof( usref ), USREF, USREF_BLOCK);
        case SREF:
            return do_alloc( &(d->alloc->srefs), sizeof( sref ), SREF, SREF_BLOCK);
        case XTRN:
            return do_alloc( &(d->alloc->xtrns), sizeof( xtrn ), XTRN, XTRN_BLOCK);
        case LOCATION:
            return do_alloc( &(d->alloc->locations), sizeof( location ), LOCATION, LOCATION_BLOCK);
        case NLIST:
            return do_alloc( &(d->alloc->nlists), sizeof( nlist ), NLIST, NLIST_BLOCK);
        case NLIST_ITEM:
            return do_alloc( &(d->alloc->nlist_items), sizeof( nlist_item ), NLIST_ITEM, NLIST_ITEM_BLOCK);

        default:
            return NULL;
    }
}

rstat do_free( dict **dr ) {
    dict *d = *dr;
    *dr = NULL;

    // Wait on all epochs
    for ( uint8_t i = 0; i < EPOCH_LIMIT; i++ ) {
        while( d->epochs[i].active ) sleep( 0 );
    }

    if ( d->alloc != NULL ) free_allocations( d->alloc );

    free( d );
    return rstat_ok;
}

void free_trash( dict *d, trash *t ) {
    while ( t != NULL ) {
        trash *goner = t;
        t = t->next;

        dev_assert( goner->type );
        dev_assert( goner->type < LOCATION );
        switch ( goner->type ) {
            case SET:
                free_set( d, (set *)goner );
            break;
            case SLOT:
                free_slot( d, (slot *)goner );
            break;
            case NODE:
                free_node( d, (node *)goner );
            break;
            case SREF:
                free_sref( d, (sref *)goner );
            break;
            case XTRN:
                free_xtrn( d, (xtrn *)goner );
            break;

            case ALLOCATIONS:
                free_allocations( (allocations *)goner );
            break;

            default:
                fprintf( stderr, "Attempt to free unexpected trash type\n" );
                abort();
        }
    }
}

int in_alloc( alloc_block *b, trash *t ) {
    while ( b ) {
        if ( (void *)t > (void *)b ) {
            if ( (void *)t < (void *)b + b->size) {
                return 1;
            }
        }
        b = b->next;
    }
    return 0;
}

void gfree( dict *d, void *item ) {
    trash *t = item;
    alloc_block *b = NULL;

    switch( t->type ) {
        case SET:
            b = d->alloc->sets;
        break;
        case SLOT:
            b = d->alloc->slots;
        break;
        case NODE:
            b = d->alloc->nodes;
        break;
        case USREF:
            b = d->alloc->usrefs;
        break;
        case SREF:
            b = d->alloc->srefs;
        break;
        case XTRN:
            b = d->alloc->xtrns;
        break;
        case LOCATION:
            b = d->alloc->locations;
        break;
        case NLIST:
            b = d->alloc->nlists;
        break;
        case NLIST_ITEM:
            b = d->alloc->nlist_items;
        break;

        default:
            fprintf( stderr, "unexpected allocation\n" );
            abort();
    }

    if ( !in_alloc( b, item )) return;

    t->type = FREE;
    while( 1 ) {
        t->next = b->top;
        if( __sync_bool_compare_and_swap( &(b->top), t->next, t ))
            return;
    }
}

void free_set( dict *d, set *s ) {
    for ( int i = 0; i < s->settings.slot_count; i++ ) {
        if ( s->slots[i] != NULL ) free_slot( d, s->slots[i] );
    }
    gfree( d, s->slots );
    gfree( d, s );
}

void free_slot( dict *d, slot *s ) {
    if ( s->root != NULL ) free_node( d, s->root );
    gfree( d, s );
}

void free_node( dict *d, node *n ) {
    if ( n->left && !blocked_null( n->left ))
        free_node( d, n->left );
    if ( n->right && !blocked_null( n->right ))
        free_node( d, n->right );

    size_t count = __sync_sub_and_fetch( &(n->usref->refcount), 1 );
    if( count == 0 ) {
        sref *r = n->usref->sref;
        if ( r && !blocked_null( r )) {
            count = __sync_sub_and_fetch( &(r->refcount), 1 );
            // If refcount is SIZE_MAX we almost certainly have an underflow. 
            dev_assert( count != SIZE_MAX );
            if( count == 0 ) free_sref( d, r );
        }

        gfree( d, n->usref );
    }

    free_xtrn( d, n->key );

    gfree( d, n );
}

void free_sref( dict *d, sref *r ) {
    if ( r->xtrn && !blocked_null( r->xtrn ))
        free_xtrn( d, r->xtrn );

    gfree( d, r );
}

void free_xtrn( dict *d, xtrn *x ) {
    if ( d->methods.ref && x->value )
        d->methods.ref( d, x->value, -1 );

    gfree( d, x );
}

void free_allocations( allocations *a ) {
    free_alloc_block( a->sets );
    free_alloc_block( a->slots );
    free_alloc_block( a->xtrns );
    free_alloc_block( a->nodes );
    free_alloc_block( a->usrefs );
    free_alloc_block( a->srefs );
    free_alloc_block( a->locations );
    free_alloc_block( a->nlists );
    free_alloc_block( a->nlist_items );
}

void free_alloc_block( alloc_block *b ) {
    while ( b ) {
        alloc_block *goner = b;
        b = b->next;
        free( goner );
    }
}

rstat do_create( dict **d, dict_settings settings, dict_methods methods ) {
    if ( methods.cmp == NULL ) return error( 1, 0, DICT_API_MISUSE, "The 'cmp' method may not be NULL.", 0 );
    if ( methods.loc == NULL ) return error( 1, 0, DICT_API_MISUSE, "The 'loc' method may not be NULL.", 0 );

    if( !settings.slot_count )    settings.slot_count    = 128;

    dict *out = malloc( sizeof( dict ));
    if ( out == NULL ) return rstat_mem;
    memset( out, 0, sizeof( dict ));

    out->alloc = malloc( sizeof( allocations ));
    if ( out->alloc == NULL ) {
        free( out );
        return rstat_mem;
    }
    memset( out->alloc, 0, sizeof( allocations ));

    out->set = create_set( out, settings, settings.slot_count );
    if ( out->set == NULL ) {
        free( out );
        return rstat_mem;
    }

    out->methods = methods;

    *d = out;

    return rstat_ok;
}

set *create_set( dict *d, dict_settings settings, size_t slot_count ) {
    set *out = galloc( d, SET );
    if ( out == NULL ) return NULL;

    memset( out, 0, sizeof( set ));
    out->trash.type = SET;
    out->settings   = settings;

    out->slots = malloc( slot_count * sizeof( slot * ));
    if ( out->slots == NULL ) {
        gfree( d, out );
        return NULL;
    }

    memset( out->slots, 0, slot_count * sizeof( slot * ));
    out->settings.slot_count = slot_count;

    return out;
}

slot *create_slot( dict *d, node *root ) {
    dev_assert( root );
    slot *new_slot = galloc( d, SLOT );
    if ( !new_slot ) return NULL;
    memset( new_slot, 0, sizeof( slot ));
    new_slot->root = root;
    new_slot->trash.type = SLOT;

    return new_slot;
}

node *create_node( dict *d, xtrn *key, usref *ref, size_t min_start_refcount ) {
    dev_assert( ref );
    dev_assert( key );

    node *new_node = galloc( d, NODE );
    if ( !new_node ) return NULL;
    memset( new_node, 0, sizeof( node ));
    new_node->trash.type = NODE;
    new_node->key = key;
    size_t rc;
    while ( 1 ) {
        rc = ref->refcount;
        if ( rc < min_start_refcount ) {
            gfree( d, new_node );
            return NULL;
        }

        if ( __sync_bool_compare_and_swap( &(ref->refcount), rc, rc + 1 ))
            break;
    }
    dev_assert( ref->refcount );
    new_node->usref = ref;

    return new_node;
}

usref *create_usref( dict *d, sref *ref ) {
    usref *new_usref = galloc( d, USREF );
    if ( !new_usref ) return NULL;
    memset( new_usref, 0, sizeof( usref ));
    new_usref->trash.type = USREF;
    if ( ref ) {
        __sync_add_and_fetch( &(ref->refcount), 1 );
    }
    new_usref->sref = ref;
    return new_usref;
}

sref *create_sref( dict *d, xtrn *x, dict_trigger *t ) {
    sref *new_sref = galloc( d, SREF );
    if ( !new_sref ) return NULL;
    memset( new_sref, 0, sizeof( sref ));
    new_sref->xtrn = x;
    new_sref->trash.type = SREF;
    new_sref->trigger = t;

    return new_sref;
}

xtrn *create_xtrn( dict *d, void *value ) {
    dev_assert( value );
    dev_assert( d );
    xtrn *new_xtrn = galloc( d, XTRN );
    if ( !new_xtrn ) return NULL;
    memset( new_xtrn, 0, sizeof( xtrn ));

    if ( d->methods.ref )
        d->methods.ref( d, value, 1 );

    new_xtrn->value = value;
    new_xtrn->trash.type = XTRN;

    return new_xtrn;
}

