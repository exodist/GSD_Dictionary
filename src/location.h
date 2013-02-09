/* NOT PUBLIC API, SUBJECT TO CHANGE WITHOUT WARNING
 *
 * This header is to be used internally in GSD Dictionary code. It is not
 * intended to be used by programs that use GSD Dictionary.
 *
 * The code in this header can change at any time with no warning. You should
 * only use the include/gsd_dict.h header file in external programs.
\*/

#ifndef GSD_DICT_LOCATION_H
#define GSD_DICT_LOCATION_H

#include "include/gsd_dict.h"
#include "include/gsd_dict_return.h"
#include "structures.h"

location *dict_create_location( dict *d );
int dict_locate( dict *d, void *key, location **locate );
void dict_free_location( dict *d, location *locate );

#endif