#pragma once

#include "ArgType.h"
#include "Log.h"

#include <string>
#include <map>
#include <deque>

using namespace std;

/////// SYMBOLS ///////////////////////////////////////////////////////////////

typedef map< string, Arg > 			symbols_t;
typedef symbols_t::iterator 		symbolptr_t;
typedef deque< symbols_t >			symstack_t;
typedef symstack_t::iterator 		symstackptr_t;

class Symbols
{
public:

	symstack_t symstack;

	void beginSymbols()
	{
		symstack.push_front( symbols_t() );
	}

	void endSymbols()
	{
		symstack.pop_front();
	}

	Arg &getSymbol( const string &name )
	{
		static Arg nosym = { ARG_UNDEF, 0xFFFF, "", "" };
		for ( symstackptr_t it = symstack.begin(); it != symstack.end(); ++it )
		{
			symbolptr_t itsym = it->find( name );
			if ( itsym != it->end() )
				return itsym->second;
		}
		return nosym;
	}

	void addSymbol( const string &name, ArgType type, word data, const string &str, const string &text )
	{
		if ( symstack.empty() )
		{
			log.error( "BAD: Symbols stack empty !!!" );
		}
		else
		{
			Arg sym = { type, data, str, text };
			if ( symstack.front().find( name ) != symstack.front().end() ) // is local ?
				symstack.front()[name] = sym;
			else
				symstack.back()[name] = sym;
		}
	}

	void addLocalSymbol( const string &name, ArgType type, word data, const string &str, const string &text )
	{
		if ( symstack.empty() )
		{
			log.error( "BAD: Symbols stack empty !!!" );
		}
	//	else if ( symstack.size() == 1 )
	//	{
	//		log.error( "BAD: No local symbols map !!!" );
	//	}
		else
		{
			Arg sym = { type, data, str, text };
			symstack.front()[name] = sym;
		}
	}

	void addSymbol( const string &name, const Arg &arg )
	{
		if ( symstack.empty() )
		{
			log.error( "BAD: Symbols stack empty !!!" );
		}
		else
		{
			if ( symstack.front().find( name ) != symstack.front().end() ) // is local ?
				symstack.front()[name] = arg;
			else
				symstack.back()[name] = arg;
		}
	}

	void addLocalSymbol( const string &name, const Arg &arg )
	{
		if ( symstack.empty() )
		{
			log.error( "BAD: Symbols stack empty !!!" );
		}
	//	else if ( symstack.size() == 1 )
	//	{
	//		log.error( "BAD: No local symbols map !!!" );
	//	}
		else
		{
			symstack.front()[name] = arg;
		}
	}
};

extern Symbols symbols;
