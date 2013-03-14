/* NOT PUBLIC API, SUBJECT TO CHANGE WITHOUT WARNING
 *
 * This header is to be used internally in GSD Dictionary code. It is not
 * intended to be used by programs that use GSD Dictionary.
 *
 * The code in this header can change at any time with no warning. You should
 * only use the include/gsd_dict.h header file in external programs.
\*/

#ifndef STRUCTURE_H
#define STRUCTURE_H

#include "epoch.h"
#include "include/gsd_dict.h"
#include "error.h"

typedef struct location location;

typedef struct nlist nlist;
typedef struct nlist_item nlist_item;

typedef struct trash trash;
typedef struct dict  dict;
typedef struct set   set;
typedef struct slot  slot;
typedef struct xtrn  xtrn;
typedef struct node  node;
typedef struct usref usref;
typedef struct sref  sref;

typedef struct alloc_block alloc_block;
typedef struct allocations allocations;

typedef enum {
    FREE = 0,

    ALLOCATIONS = 1,

    SET   = 2,
    SLOT  = 3,
    NODE  = 4,
    USREF = 5,
    SREF  = 6,
    XTRN  = 7,

    LOCATION = 8,

    NLIST      =  9,
    NLIST_ITEM = 10
} type;

struct alloc_block {
    alloc_block *next;
    trash * volatile top;
    size_t idx;
    size_t size;
};

struct trash {
    trash *next;
    type   type;

#ifdef TRASH_CHECK
    char *fn;
    size_t ln;
#endif
};

struct allocations {
    trash trash;

    alloc_block *sets;
    alloc_block *slots;
    alloc_block *xtrns;
    alloc_block *nodes;
    alloc_block *usrefs;
    alloc_block *srefs;

    alloc_block *locations;
    
    alloc_block *nlists;
    alloc_block *nlist_items;
};

struct dict {
    dict_methods methods;

    set *set;

    size_t item_count;

#ifdef GSD_METRICS
    size_t rebalanced;
    size_t epoch_changed;
    size_t epoch_failed;
#endif

    epoch epochs[EPOCH_LIMIT];
    uint8_t epoch;

    allocations *alloc;
};

struct set {
    trash  trash;
    slot **slots;
    dict_settings settings;

    uint8_t immutable;
    uint8_t rebuild;
};

struct slot {
    trash   trash;
    node   *root;

    size_t  item_count;

    uint8_t ideal_height;
    uint8_t rebuild;
    uint8_t patho;
};

struct xtrn {
    trash trash;
    void *value;
};

struct node {
    trash trash;
    node  *left;
    node  *right;
    xtrn  *key;
    usref *usref;
};

struct usref {
    trash trash;
    size_t  refcount;
    sref   *sref;
};

struct sref {
    trash   trash;
    size_t  refcount;
    xtrn   *xtrn;
    dict_trigger *trigger;
};

int iterate( dict *d, dict_handler *h, void *args );

int iterate_node( dict *d, set *s, node *n, dict_handler *h, void *args );

#endif

