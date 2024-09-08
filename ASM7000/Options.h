#pragma once

#include "Debug.h"

#include <stack>

/////// OPTIONS ///////////////////////////////////////////////////////////////

struct Options
{
	bool nocompatwarning;
	bool nowarning;
	bool nodebug;
	bool list;
	bool clist;
	bool listall;
	bool nolist;
	bool noheader;
	bool nolinenum;
	bool nocerr;
	bool page;


	Options()
	: nocompatwarning( false )
	, nowarning( false )
	, nodebug( !DEBUG_MSGS )
	, list( true )
	, clist( false )
	, listall( false )
	, nolist( false )
	, noheader( false )
	, nolinenum( false )
	, nocerr( false )
	, page( true )
	{}
};

extern Options options;

extern stack< Options > optstack;

