#pragma once

#include "Log.h"
#include "Strings.h"

#include <string>
#include <vector>
#include <map>

using namespace std;

/////// FUNCTIONS /////////////////////////////////////////////////////////////

class Function
{
public:
	Function()
	{}

	Function( const string &name, const vector< string > &def )
	{
		if ( name.empty() )
		{
			log.error( "Missing function name" );
		}
		else if ( !def.size() )
		{
			log.error( "Missing function definition for %s", name.data() );
		}
		else
		{
			name_ = name;
			for ( int i=0; i<def.size()-1; ++i )
			{
				params_.push_back( Strings::touppernotquoted( def[i] ) );
			}
			expr_ = Strings::touppernotquoted( def.back() );
			log.debug( "Function %s = %s", name_.data(), expr_.data() );
		}
	}

	string name_;
	vector< string > params_;
	string expr_;
};

typedef map< string, Function > FunctionSeq_t;
typedef FunctionSeq_t::const_iterator FunctionPtr_t;

extern FunctionSeq_t functions;


