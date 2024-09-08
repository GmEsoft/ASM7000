#pragma once

#include <string>
#include <vector>
#include <time.h>

using namespace std;

/////// STRINGS ///////////////////////////////////////////////////////////////


class Strings
{
public:
	static char* timeStr()
	{
		struct tm *ptime;
		time_t clock;
		char *s, *p;

		time( &clock );
		ptime = localtime( &clock );
		s = asctime( ptime );
		p = s + strlen( s ) - 1;
		if ( *p == 0x0A )
			*p = 0;
		return s;
	}


	// Split string to tokens using provided separator
	static vector<string> split( string str, const string &sep )
	{
		vector<string> tokens;
		tokens.reserve( 5 );
		const char *p0 = str.data();
		const char *p = p0;
		bool cmt = false;
		char quot = 0;

		while ( *p )
		{
			cmt = cmt || ( !quot && *p == ';' );
			while ( *p && ( cmt || sep.find( *p ) == string::npos ) )
			{
				bool esc = false;
				do
				{
					if ( esc )
					{
						esc = false;
					}
					else if ( *p == '\\' )
					{
						esc = true;
					}
					else if ( quot )
					{
						if ( !*p || quot == *p )
							quot = 0;
					}
					else if ( *p == '\'' || *p == '"' )
					{
						quot = *p;
					}


					if ( quot )
						++p;
				}
				while ( *p && quot );

				++p;
			}
			tokens.push_back( string( p0, p ) );
			while ( *p && !cmt && sep.find( *p ) != string::npos )
			{
				++p;
			}
			p0 = p;
		}
		return tokens;
	}

	// Tabulate string to given position
	void tab( string &str, int tab )
	{
		int len = 0;
		for ( int pos = 0; pos < str.size(); ++pos )
		{
			if ( str[pos] == '\t' )
				len += 8 - ( len % 8 );
			else
				++len;
		}
		while ( len < tab )
		{
			str += "\t";
			len += 8 - ( len % 8 );
		}

	}

	static string touppernotquoted( const string& str )
	{
		string ret = str;
		bool esc = false;
		char quot = 0;
		for ( int i=0; i<ret.size(); ++i )
		{
			char c = ret[i];
			if ( esc )
				esc = false;
			else if ( quot )
			{
				if ( c == quot )
					quot = 0;
			}
			else if ( c == '\\' )
			{
				esc = true;
			}
			else
			{
				if ( c == '"' || c == '\'' )
					quot = c;
				else if ( !quot )
					ret[i] = toupper( c );
			}
		}
		return ret;
	}

};
