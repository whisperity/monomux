/* SPDX-License-Identifier: LGPL-3.0-only */

#ifndef DISPATCH
/// Set for the given \p MessageKind the callback \p FUNCTION_NAME.
#define DISPATCH(KIND, FUNCTION_NAME)
#endif

DISPATCH(ClientIDResponse, responseClientID)
DISPATCH(DetachedNotification, receivedDetachNotification)

#undef DISPATCH
