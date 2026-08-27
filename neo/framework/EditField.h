/*
===========================================================================

	Tech 5 style dynamic console edit field.

===========================================================================
*/
#ifndef __EDITFIELD_H__
#define __EDITFIELD_H__

#include "AutoComplete.h"

// The native Win32 console uses this only as the size of its temporary input
// buffer.  The in-game edit field itself is dynamically allocated.
const int MAX_EDIT_LINE = MAX_STRING_CHARS;

class idEditField {
public:
	idEditField();
	idEditField( const idEditField &other );
	~idEditField();
	idEditField &	operator=( const idEditField &other );

	void			Clear();
	void			SetWidthInChars( int w );
	void			SetCursor( int c );
	int				GetCursor() const;
	void			ClearAutoComplete();
	bool			AcceptAutoComplete();
	int				GetAutoCompleteLength() const;
	void			AutoComplete( bool reverseOrder = false );
	void			CharEvent( int c );
	void			KeyDownEvent( int key );
	void			Paste();
	const char *	GetBuffer() const;
	void			Draw( int x, int y, int width, bool showCursor );
	void			SetBuffer( const char * text );

private:
	void			Erase( int start, int count );
	void			MakeCursorVisible();

	int				cursor;
	int				scroll;
	int				widthInChars;
	idStr			buffer;
	idAutoComplete	autoComplete;
};

#endif
