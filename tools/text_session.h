#ifndef TEXT_SESSION_H
#define TEXT_SESSION_H

#include "tool_session.h"

typedef ToolSession TextSession;

void TextSession_Begin(TextSession *s, HWND hWnd, int x, int y);
void TextSession_MarkModified(TextSession *s);
void TextSession_CommitIfNeeded(TextSession *s, const char *actionName);
void TextSession_End(TextSession *s);
void TextSession_Cancel(TextSession *s);

#endif
