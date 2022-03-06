#ifndef DISPATCH
/// Set for the given \p MessageKind the callback \p FUNCTION_NAME.
#define DISPATCH(KIND, FUNCTION_NAME)
#endif

DISPATCH(REQ_ClientID, clientID)

DISPATCH(REQ_SpawnProcess, spawnProcess)

#undef DISPATCH
