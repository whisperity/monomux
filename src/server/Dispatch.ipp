#ifndef DISPATCH
/// Set for the given \p MessageKind the callback \p FUNCTION_NAME.
#define DISPATCH(KIND, FUNCTION_NAME)
#endif

DISPATCH(REQ_ClientID, requestClientID)
DISPATCH(REQ_DataSocket, requestDataSocket)

DISPATCH(REQ_SpawnProcess, requestSpawnProcess)

#undef DISPATCH
