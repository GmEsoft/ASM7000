#include "Parser.h"

#include <iostream>

Log log;
Symbols symbols;
word pc;
FunctionSeq_t functions;

int parsenumTest( const string &arg, int radix, int expected )
{
	Parser parser( arg );
	int ret = parser.parsenum( radix );
	log.writeTo( cerr );
	log.clear();

	if ( ret != expected )
	{
		cerr << "Test failed: expected [" << expected << "] but got [" << ret << "]" << endl;
		return 1;
	}

	return 0;
}

int parseTest( const string &arg, int expected )
{
	Parser parser( arg );
	Arg ret = parser.parse();
	log.writeTo( cerr );
	log.clear();

	if ( ret.type != ARG_IMM )
	{
		cerr << "parseTest failed: expected type ARG_IMM but got [" << ArgTypes::get(ret.type)<< "]" << endl;
		return 1;
	}

	if ( ret.data != expected )
	{
		cerr << "parseTest failed [" << arg << "]: expected [" << expected << "] but got [" << ret.data << "]" << endl;
		return 1;
	}

	return 0;
}

int main()
{
	log.setEnabled( true );
	//log.setDebug( true );
	symbols.beginSymbols();

	int ret = 0;
	ret += parsenumTest( "123", 10, 123 );
	ret += parsenumTest( "123", 16, 0x123 );
	ret += parsenumTest( "123", 0, 123 );
	ret += parsenumTest( "123H", 0, 0x123 );
	ret += parsenumTest( "10100101B", 0, 0xA5 );
	ret += parsenumTest( "10100101", 2, 0xA5 );

	ret += parseTest( "123", 123 );
	ret += parseTest( ">123", 0x123 );
	ret += parseTest( "3-2-1", 0 );
	ret += parseTest( "3-2+1", 2 );
	ret += parseTest( "3-(2+1)", 0 );
	ret += parseTest( "3 - ( 2 + 1 )", 0 );

	Arg one   = { ARG_IMM, 1, "", "" };
	Arg two   = { ARG_IMM, 2, "", "" };
	Arg three = { ARG_IMM, 3, "", "" };
	symbols.addSymbol( "ONE",   one );
	symbols.addSymbol( "TWO",   two );
	symbols.addSymbol( "THREE", three );
	ret += parseTest( "THREE", 3 );
	ret += parseTest( "THREE-(TWO+ONE)", 0 );

	vector< string > sumDef;
	sumDef.push_back( "X" );
	sumDef.push_back( "Y" );
	sumDef.push_back( "X+Y" );
	functions["SUM"] = Function( "SUM", sumDef );
	ret += parseTest( "SUM(ONE,TWO)", 3 );
	ret += parseTest( "SUM(ONE,TWO)-THREE", 0 );
	ret += parseTest( "SUM(SUM(ONE,TWO),THREE)", 6 );

	//TODO: ret += parseTest( "-1", 0xFFFF );
	//TODO: ret += parseTest( "~0A55AH", 0x5AA5 );
	ret += parseTest( "0F5H&0FAH", 0x00F0 );
	ret += parseTest( "0F5H|0FAH", 0x00FF );
	ret += parseTest( "0F5H^0FAH", 0x000F );
	ret += parseTest( "0F5H<<4", 0x0F50 );
	ret += parseTest( "0F50H>>4", 0x00F5 );
	ret += parseTest( "3*5", 15 );
	ret += parseTest( "255/15", 17 );
	ret += parseTest( "256%15", 1 );

	symbols.endSymbols();

	return ret;
}
