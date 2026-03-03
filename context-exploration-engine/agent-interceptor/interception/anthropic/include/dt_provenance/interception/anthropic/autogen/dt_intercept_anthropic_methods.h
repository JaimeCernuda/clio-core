#ifndef DT_PROVENANCE_INTERCEPTION_ANTHROPIC_AUTOGEN_METHODS_H_
#define DT_PROVENANCE_INTERCEPTION_ANTHROPIC_AUTOGEN_METHODS_H_

#include <chimaera/chimaera.h>

namespace dt_provenance::interception::anthropic {

namespace Method {
// Inherited methods
GLOBAL_CONST chi::u32 kCreate = 0;
GLOBAL_CONST chi::u32 kDestroy = 1;
GLOBAL_CONST chi::u32 kMonitor = 9;

// Custom methods
GLOBAL_CONST chi::u32 kInterceptAndForward = 10;
}  // namespace Method

}  // namespace dt_provenance::interception::anthropic

#endif  // DT_PROVENANCE_INTERCEPTION_ANTHROPIC_AUTOGEN_METHODS_H_
