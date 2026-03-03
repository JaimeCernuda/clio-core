#ifndef DT_PROVENANCE_TRACKER_AUTOGEN_METHODS_H_
#define DT_PROVENANCE_TRACKER_AUTOGEN_METHODS_H_

#include <chimaera/chimaera.h>

namespace dt_provenance::tracker {

namespace Method {
// Inherited methods
GLOBAL_CONST chi::u32 kCreate = 0;
GLOBAL_CONST chi::u32 kDestroy = 1;
GLOBAL_CONST chi::u32 kMonitor = 9;

// Custom methods
GLOBAL_CONST chi::u32 kStoreInteraction = 10;
GLOBAL_CONST chi::u32 kQuerySession = 11;
GLOBAL_CONST chi::u32 kListSessions = 12;
GLOBAL_CONST chi::u32 kGetInteraction = 13;
}  // namespace Method

}  // namespace dt_provenance::tracker

#endif  // DT_PROVENANCE_TRACKER_AUTOGEN_METHODS_H_
