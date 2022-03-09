#ifndef DISPATCH
/// Set for the given \p MessageKind the callback \p FUNCTION_NAME.
#define DISPATCH(KIND, FUNCTION_NAME)
#endif

DISPATCH(RSP_ClientID, responseClientID)
DISPATCH(RSP_DataSocket, responseDataSocket)

#undef DISPATCH
