#include "../idlib/precompiled.h"
#pragma hdrstop

static idAutoComplete * completionTarget = NULL;
static idStr completionPrefix;

static void CollectCompletion( const char * text ) {
	if ( completionTarget == NULL || text == NULL ) {
		return;
	}
	if ( completionPrefix.Length() != 0 && idStr::Icmpn( text, completionPrefix, completionPrefix.Length() ) != 0 ) {
		return;
	}
	completionTarget->Append( text );
}

idEditField::idEditField() : cursor( 0 ), scroll( 0 ), widthInChars( 0 ) {
	Clear();
}

/*
====================
idEditField::idEditField

Autocomplete owns parsed command arguments whose argv entries point back into
the owning object. History entries only need the editable text and cursor
state, so rebuild autocomplete on demand instead of copying those pointers.
====================
*/
idEditField::idEditField( const idEditField &other ) :
	cursor( other.cursor ),
	scroll( other.scroll ),
	widthInChars( other.widthInChars ),
	buffer( other.buffer ) {
	autoComplete.Clear();
}

idEditField &idEditField::operator=( const idEditField &other ) {
	if ( this != &other ) {
		cursor = other.cursor;
		scroll = other.scroll;
		widthInChars = other.widthInChars;
		buffer = other.buffer;
		autoComplete.Clear();
	}
	return *this;
}

idEditField::~idEditField() {
}

void idEditField::Clear() {
	buffer.Clear();
	cursor = 0;
	scroll = 0;
	autoComplete.Clear();
}

void idEditField::SetWidthInChars( int width ) {
	widthInChars = Max( 1, width );
	MakeCursorVisible();
}

void idEditField::SetCursor( int position ) {
	cursor = idMath::ClampInt( 0, buffer.Length(), position );
	MakeCursorVisible();
}

int idEditField::GetCursor() const {
	return cursor;
}

void idEditField::Erase( int start, int count ) {
	if ( count <= 0 || start < 0 || start >= buffer.Length() ) {
		return;
	}
	const int end = Min( buffer.Length(), start + count );
	idStr result( buffer, 0, start );
	result.Append( buffer.c_str() + end );
	buffer = result;
}

void idEditField::MakeCursorVisible() {
	if ( cursor < scroll ) {
		scroll = cursor;
	}
	if ( cursor >= scroll + widthInChars ) {
		scroll = cursor - widthInChars + 1;
	}
	if ( scroll < 0 ) {
		scroll = 0;
	}
}

void idEditField::ClearAutoComplete() {
	if ( autoComplete.matchLength > 0 && autoComplete.matchLength <= buffer.Length() ) {
		buffer.CapLength( autoComplete.matchLength );
		cursor = Min( cursor, buffer.Length() );
	}
	autoComplete.Clear();
	MakeCursorVisible();
}

bool idEditField::AcceptAutoComplete() {
	if ( autoComplete.NumSuggestions() == 0 || autoComplete.matchLength <= 0 ) {
		return false;
	}
	autoComplete.Clear();
	cursor = buffer.Length();
	MakeCursorVisible();
	return true;
}

int idEditField::GetAutoCompleteLength() const {
	return autoComplete.matchLength;
}

void idEditField::AutoComplete( bool reverseOrder ) {
	if ( autoComplete.NumSuggestions() == 0 ) {
		autoComplete.Clear();
		autoComplete.args.TokenizeString( buffer.c_str(), false );
		if ( autoComplete.args.Argc() == 0 ) {
			return;
		}

		autoComplete.matchLength = buffer.Length();
		completionTarget = &autoComplete;
		completionPrefix = buffer;

		const bool completingArgument = strchr( buffer.c_str(), ' ' ) != NULL;
		if ( completingArgument ) {
			cmdSystem->ArgCompletion( buffer.c_str(), CollectCompletion );
			cvarSystem->ArgCompletion( buffer.c_str(), CollectCompletion );
		} else {
			cmdSystem->CommandCompletion( CollectCompletion );
			cvarSystem->CommandCompletion( CollectCompletion );
		}

		completionTarget = NULL;
		completionPrefix.Clear();
		if ( autoComplete.NumSuggestions() == 0 ) {
			autoComplete.Clear();
			return;
		}

		common->Printf( "]%s\n", buffer.c_str() );
		for ( int i = 0; i < autoComplete.NumSuggestions(); ++i ) {
			common->Printf( "    %s\n", autoComplete.GetSuggestion( i ).c_str() );
		}
		autoComplete.currentIndex = reverseOrder ? autoComplete.NumSuggestions() - 1 : 0;
	} else {
		const int count = autoComplete.NumSuggestions();
		autoComplete.currentIndex = ( autoComplete.currentIndex + ( reverseOrder ? count - 1 : 1 ) ) % count;
	}

	buffer = autoComplete.GetSuggestion( autoComplete.currentIndex );
	if ( autoComplete.NumSuggestions() == 1 && strchr( buffer.c_str(), ' ' ) == NULL ) {
		buffer.Append( ' ' );
	}
	cursor = autoComplete.matchLength;
	MakeCursorVisible();
}

void idEditField::CharEvent( int character ) {
	if ( character == 'v' - 'a' + 1 ) {
		ClearAutoComplete();
		Paste();
		return;
	}
	if ( character == 'c' - 'a' + 1 ) {
		Clear();
		return;
	}
	if ( character == 'a' - 'a' + 1 ) {
		SetCursor( 0 );
		return;
	}
	if ( character == 'e' - 'a' + 1 ) {
		SetCursor( buffer.Length() );
		return;
	}
	if ( character == 'k' - 'a' + 1 ) {
		ClearAutoComplete();
		buffer.CapLength( cursor );
		return;
	}
	if ( character == 'h' - 'a' + 1 || character == K_BACKSPACE ) {
		ClearAutoComplete();
		if ( cursor > 0 ) {
			Erase( cursor - 1, 1 );
			--cursor;
			MakeCursorVisible();
		}
		return;
	}
	if ( character < 32 ) {
		return;
	}

	ClearAutoComplete();
	if ( idKeyInput::GetOverstrikeMode() && cursor < buffer.Length() ) {
		buffer[cursor] = static_cast<char>( character );
	} else {
		buffer.Insert( static_cast<char>( character ), cursor );
	}
	++cursor;
	MakeCursorVisible();
}

void idEditField::KeyDownEvent( int key ) {
	const bool control = idKeyInput::IsDown( K_LCTRL ) || idKeyInput::IsDown( K_RCTRL );
	const bool shift = idKeyInput::IsDown( K_LSHIFT ) || idKeyInput::IsDown( K_RSHIFT );

	if ( ( key == K_INS || key == K_KP_0 ) && shift ) {
		ClearAutoComplete();
		Paste();
		return;
	}
	if ( key == K_DEL ) {
		if ( autoComplete.NumSuggestions() != 0 ) {
			ClearAutoComplete();
		} else {
			Erase( cursor, 1 );
		}
		return;
	}
	if ( key == K_RIGHTARROW ) {
		AcceptAutoComplete();
		if ( control ) {
			while ( cursor < buffer.Length() && buffer[cursor] != ' ' ) { ++cursor; }
			while ( cursor < buffer.Length() && buffer[cursor] == ' ' ) { ++cursor; }
		} else if ( cursor < buffer.Length() ) {
			++cursor;
		}
		MakeCursorVisible();
		return;
	}
	if ( key == K_LEFTARROW ) {
		ClearAutoComplete();
		if ( control ) {
			while ( cursor > 0 && buffer[cursor - 1] == ' ' ) { --cursor; }
			while ( cursor > 0 && buffer[cursor - 1] != ' ' ) { --cursor; }
		} else if ( cursor > 0 ) {
			--cursor;
		}
		MakeCursorVisible();
		return;
	}
	if ( key == K_HOME || ( key == K_A && control ) ) {
		ClearAutoComplete();
		SetCursor( 0 );
		return;
	}
	if ( key == K_END || ( key == K_E && control ) ) {
		AcceptAutoComplete();
		SetCursor( buffer.Length() );
		return;
	}
	if ( key == K_INS ) {
		idKeyInput::SetOverstrikeMode( !idKeyInput::GetOverstrikeMode() );
		return;
	}
	if ( key != K_CAPSLOCK && key != K_LALT && key != K_LCTRL && key != K_LSHIFT &&
		key != K_RALT && key != K_RCTRL && key != K_RSHIFT ) {
		ClearAutoComplete();
	}
}

void idEditField::Paste() {
	char * clipboard = Sys_GetClipboardData();
	if ( clipboard == NULL ) {
		return;
	}
	for ( const char * text = clipboard; *text != '\0'; ++text ) {
		CharEvent( static_cast<unsigned char>( *text ) );
	}
	Mem_Free( clipboard );
}

const char * idEditField::GetBuffer() const {
	return buffer.c_str();
}

void idEditField::SetBuffer( const char * text ) {
	buffer = text != NULL ? text : "";
	autoComplete.Clear();
	cursor = buffer.Length();
	MakeCursorVisible();
}

void idEditField::Draw( int x, int y, int width, bool showCursor ) {
	const int drawChars = Max( 1, widthInChars );
	MakeCursorVisible();
	const int end = Min( buffer.Length(), scroll + drawChars );
	idStr visible( buffer, scroll, end );
	Con_DrawConsoleString( static_cast<float>( x ), static_cast<float>( y ), visible.c_str(), colorWhite, false );

	if ( !showCursor || ( ( idLib::frameNumber >> 4 ) & 1 ) != 0 ) {
		return;
	}
	const int cursorColumn = idMath::ClampInt( 0, drawChars, cursor - scroll );
	const char cursorCharacter = idKeyInput::GetOverstrikeMode() ? '|' : '_';
	Con_DrawConsoleChar( static_cast<float>( x ) + cursorColumn * Con_ConsoleCharWidth(),
		static_cast<float>( y ), cursorCharacter, colorWhite );
}
