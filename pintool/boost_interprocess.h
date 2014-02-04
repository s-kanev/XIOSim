/* Headers needed for multiprocess communication through boost interprocess.
 * This needs to be included BEFORE anything referencing machine.h,
 * or macro definitions from machine.h would mess template args.
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/unordered_map.hpp>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>

#pragma GCC diagnostic pop

#include "shared_map.h"
#include "shared_unordered_map.h"
