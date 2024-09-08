#pragma once

#include "TypeDefs.h"

#include <string>

using namespace std;

/////// ARGS //////////////////////////////////////////////////////////////////

enum ArgType
{
	ARG_NONE = 0,
	ARG_UNDEF,
	ARG_IMM,
	ARG_EFFEC,
	ARG_DIR,
	ARG_INDEX,
	ARG_INDIR,
	ARG_REG,
	ARG_PORT,
	ARG_A,
	ARG_B,
	ARG_ST,
	ARG_TEXT,
	ARG_DUP
};

struct ArgTypes
{
	static const char *get( size_t n )
	{
		const char *argtypes[] =
		{
			"NONE",
			"UNDEF",
			"IMM",
			"EFFEC",
			"DIR",
			"INDEX",
			"INDIR",
			"REG",
			"PORT",
			"A",
			"B",
			"ST",
			"TEXT",
			"DUP"
		};
		return argtypes[n];
	}
};

struct Arg
{
	ArgType type;
	word	data;
	string	str;
	string	text;
};


