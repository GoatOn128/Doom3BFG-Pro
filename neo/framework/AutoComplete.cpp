#include "../idlib/precompiled.h"
#pragma hdrstop

#include "AutoComplete.h"

idAutoComplete::idAutoComplete() {
	Clear();
}

void idAutoComplete::Clear() {
	matchLength = 0;
	currentIndex = -1;
	args.Clear();
	suggestions.Clear();
}

void idAutoComplete::Append( const idStr & suggestion ) {
	for ( int i = 0; i < suggestions.Num(); ++i ) {
		if ( suggestions[i].Icmp( suggestion ) == 0 ) {
			return;
		}
	}
	suggestions.Append( suggestion );
}
