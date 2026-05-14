#include "text_session.h"
#include "../tools.h"

void TextSession_Begin(TextSession *s, HWND hWnd, int x, int y) {
  ToolSession_Begin(s, hWnd, x, y, MK_LBUTTON, TOOL_TEXT);
}

void TextSession_MarkModified(TextSession *s) { ToolSession_MarkModified(s); }
void TextSession_CommitIfNeeded(TextSession *s, const char *actionName) { ToolSession_CommitIfNeeded(s, actionName); }
void TextSession_End(TextSession *s) { ToolSession_End(s); }
void TextSession_Cancel(TextSession *s) { ToolSession_Cancel(s); }
