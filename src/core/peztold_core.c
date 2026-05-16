#include "peztold_core.h"
#define CORE_MAX_OBSERVERS 16
static void (*s_coreObservers[CORE_MAX_OBSERVERS])(CoreEvent ev);
static int s_coreObserverCount;
void Core_RegisterObserver(void (*fn)(CoreEvent ev)) {
	if (!fn || s_coreObserverCount >= CORE_MAX_OBSERVERS)
		return;
	s_coreObservers[s_coreObserverCount++] = fn;
}
void Core_Notify(CoreEvent ev) {
	for (int i = 0; i < s_coreObserverCount; i++) {
		if (s_coreObservers[i])
			s_coreObservers[i](ev);
	}
}
