/* What the C heap holds: the console's memory goes to the heap and to the
 * GPU's textures, and a run's caches (parsed scenes, skeletons) only grow. */
#ifndef SPIRE_HEAP_H
#define SPIRE_HEAP_H

#include <stddef.h>

namespace spire {

/* Bytes malloc has handed out and not had back (newlib's and glibc's
 * mallinfo); 0 where the runtime has no such count. */
size_t heap_in_use();
/* Bytes the heap has taken from the system (its arena): what is free
 * inside it (fragments between live blocks) counts against the console's
 * memory all the same. */
size_t heap_held();

} /* namespace spire */

#endif
