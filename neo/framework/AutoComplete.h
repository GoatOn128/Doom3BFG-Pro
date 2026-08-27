#ifndef __AUTOCOMPLETE_H__
#define __AUTOCOMPLETE_H__

class idAutoComplete {
public:
	idAutoComplete();

	void			Clear();
	void			Append( const idStr & suggestion );
	int				NumSuggestions() const { return suggestions.Num(); }
	const idStr &	GetSuggestion( int index ) const { return suggestions[index]; }

	int				matchLength;
	int				currentIndex;
	idCmdArgs		args;
	idList< idStr >	suggestions;
};

#endif
