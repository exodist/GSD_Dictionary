#include "../include/gsd_dict.h"
#include "../include/gsd_dict_return.h"
#include "../epoch.h"
#include "../structure.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

int compare( dict_settings *settings, void *key1, void *key2 ) {
    int64_t k1 = *(int64_t*)key1;
    int64_t k2 = *(int64_t*)key2;
    if ( k1 > k2 ) return -1;
    if ( k2 > k1 ) return 1;
    return 0;
}

char *show( void *key, void *val ) {
    int64_t k = *(int64_t*)key;
    int64_t v = val ? *(int64_t*)val : -1;
    char *buffer = malloc( 20 );
    snprintf( buffer, 20, "%li:%li", k, v );
    return buffer;
}

size_t locate( dict_settings *settings, void *key ) {
    int64_t k = *(int64_t*)key;
    int64_t s = k % settings->slot_count;
    return s;
}

int main() {
    srand(time(NULL));

    dict *d;
    dict_methods *m = malloc( sizeof( dict_methods ));
    memset( m, 0, sizeof( dict_methods ));
    dict_settings *s = malloc( sizeof( dict_settings ));
    memset( s, 0, sizeof( dict_settings ));
    s->slot_count = 8;
    s->max_imbalance = 2;
    m->cmp = compare;
    m->loc = locate;

    dict_create( &d, 0, s, m );
    int64_t v = 1;
    int64_t v2 = 11;
    int64_t v3 = 55;
    int64_t k[4001];
    for (uint64_t i = 0; i < 4001; i++) {
        k[i] = i;
    }

    for ( int i = 0; i < 200; i++ ) {
        int64_t x = rand();
        x <<= 8;
        x += rand();
        x %= 4000;

        if ( x < 0 ) x *= -1;
        dict_set( d, &k[x], &v );
        int64_t *g = NULL;
        dict_get( d, &k[x], (void **)&g );
        assert( g == &v );
        if ( i % 8 == 0 ) {
            dict_delete( d, &k[x] );
            dict_get( d, &k[x], (void **)&g );
            assert( g == NULL );
        }
        if ( i % 5 == 0 ) {
            int64_t y = rand() % 8;
            if ( x == y ) { y += 1; }
            dict_reference( d, &k[x], d, &k[y] );
        }
        if ( i % 7 == 0 ) {
            dict_dereference( d, &k[x] );
        }
    }

    dict_set( d, &k[0], &v );
    dict_set( d, &k[8], &v );
    dict_dereference( d, &k[0] );
    dict_dereference( d, &k[8] );
    dict_reference( d, &k[0], d, &k[3905] );
    dict_set( d, &k[0], &v3 );
    dict_reference( d, &k[3905], d, &k[2401] );

    epoch *e = dict_join_epoch( d );
    sref *sr = malloc( sizeof( sref ));
    memset( sr, 0, sizeof( sref ));
    dict_dispose( d, e, NULL, sr, SREF );
    char *dot = dict_dump_dot( d, show );
    dict_leave_epoch( d, e );

    printf( "%s\n", dot );

    return 0;
}
