#include "Strings.h"

#include <iostream>

int touppernotquotedTest( const string &arg, const string &expected )
{
	string ret = Strings::touppernotquoted( arg );
	if ( ret != expected )
	{
		cerr << "Test failed: expected [" << expected << "] but got [" << ret << "]" << endl;
		return 1;
	}
	return 0;
}

int main()
{
	return 	touppernotquotedTest( "my name is 'FooBar'. 'FooBar' is my name.", "MY NAME IS 'FooBar'. 'FooBar' IS MY NAME." )
		+	touppernotquotedTest( "'Hank''s friends'", "'Hank''s friends'" )
		//+	touppernotquotedTest( "'Hank\\'s friends'", "'Hank\\'s friends'" ) TODO: FIX
		+	touppernotquotedTest( "\"Hank's friends\"", "\"Hank's friends\"" );
}
