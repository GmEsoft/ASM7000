#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <cstdarg>
#include <time.h>

using namespace std;

typedef unsigned short word;
typedef unsigned char  byte;

typedef map< string, word > symbols_t;
typedef symbols_t::const_iterator symbolptr_t;

const char title[] = "** TMS-7000 tiny assembler - v0.1.0-alpha - (C) 2024 GmEsoft, All rights reserved. **";

const char help[] = "Usage: ASM7000 -i:InputFile[.asm] -o:OutputFile[.cim] [-l:Listing[.lst]]\n";

word pc = 0;
int pass;
symbols_t symbols;
vector<string> errors;

enum ArgType
{
	ARG_NONE = 0,
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
	ARG_TEXT
};

struct Arg
{
	ArgType type;
	word	data;
	string	str;
};

#define enum_pair( A, B ) ( ( (A) << 4 ) | (B) )


char* timeStr()
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
vector<string> split( string str, char sep )
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
		do
		{
			if ( quot )
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
		while ( quot );

		while ( *p && ( cmt || *p != sep ) )
		{
			++p;
		}
		tokens.push_back( string( p0, p ) );
		while ( *p && !cmt && *p == sep )
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

void error( const char* format, ... )
{
	static char buf[80];
	if ( pass == 2 )
	{
		va_list args;
		va_start( args, format );
		vsprintf_s( buf, sizeof buf, format, args );
		va_end( args );
		errors.push_back( buf );
	}
}

bool chkargs( const string& op, const vector< Arg > &args, size_t num )
{
	if ( args.size() < num )
		error( "%s: Too few args %d, expecting %d", op.data(), args.size(), num );
	else if ( args.size() > num )
		error( "%s: Too many args %d, expecting %d", op.data(), args.size(), num );
	return args.size() == num;
}

word parselit( const string &arg )
{
	word ret = 0xFFFF;
	size_t size = arg.size();
	if ( size > 0 )
	{
		if ( arg[0] == '$' )
		{
			ret = pc;
			if ( size > 1 )
			{
				if ( arg[1] == '+' )
					ret += parselit( arg.substr( 2 ) );
				else if ( arg[1] == '-' )
					ret -= parselit( arg.substr( 2 ) );
				else
					error( "Error in expression: %s", arg.data() );
			}
		}
		else if ( arg[0] == '>' )
		{
			stringstream sstr( arg.substr( 1 ) );
			sstr >> hex >> ret;
		}
		else if ( arg[0] == '?' )
		{
			error( "Binary format not supported: %s", arg.data() );
		}
		else if ( isdigit( arg[0] ) )
		{
			stringstream sstr( arg );
			sstr >> ret;
		}
		else
		{
			symbolptr_t sym = symbols.find( arg );
			if ( sym != symbols.end() )
			{
				ret = sym->second;
			}
			else
			{
				error( "Symbol not found: %s", arg.data() );
			}
		}
	}
	else
	{
		error( "Missing literal", arg.data() );
	}
	return ret;
}

Arg getarg( const string &arg )
{
	Arg ret;
	ret.type = ARG_NONE;
	ret.data = 0;
	ret.str  = arg;

	size_t size = arg.size();

	if ( arg == "A" )
	{
		ret.type = ARG_A;
	}
	else if ( arg == "B" )
	{
		ret.type = ARG_B;
	}
	else if ( arg == "ST" )
	{
		ret.type = ARG_ST;
	}
	else if ( arg[0] == '%' )
	{
		if ( arg.find( "(B)" ) == arg.size() - 3 )
		{
			ret.type = ARG_EFFEC;
			ret.data = parselit( arg.substr( 1, arg.size() - 4 ) );
		}
		else
		{
			ret.type = ARG_IMM;
			ret.data = parselit( arg.substr( 1 ) );
		}
	}
	else if ( size > 2 && arg[0] == '*' && arg[1] == 'R' && isdigit( arg[2] ) )
	{
		ret.type = ARG_INDIR;
		ret.data = parselit( arg.substr( 2 ) );
	}
	else if ( arg[0] == '@' )
	{
		if ( arg.find( "(B)" ) == arg.size() - 3 )
		{
			ret.type = ARG_INDEX;
			ret.data = parselit( arg.substr( 1, arg.size() - 4 ) );
		}
		else
		{
			ret.type = ARG_DIR;
			ret.data = parselit( arg.substr( 1 ) );
		}
	}
	else if ( size > 1 && arg[0] == 'R' && isdigit( arg[1] ) )
	{
		ret.type = ARG_REG;
		ret.data = parselit( arg.substr( 1 ) );
	}
	else if ( size > 1 && arg[0] == 'P' && isdigit( arg[1] ) )
	{
		ret.type = ARG_PORT;
		ret.data = parselit( arg.substr( 1 ) );
	}
	else if ( arg[0] == '\'' || ( size > 1 && arg[0] == '-' && arg[1] == '\'' ) )
	{
		ret.type = ARG_TEXT;
	}
	else
	{
		ret.type = ARG_IMM;
		ret.data = parselit( arg );
	}
	return ret;
}


word getimmediate( const Arg &arg )
{
	if ( arg.type != ARG_IMM )
		error( "Expecting immediate: %s", arg.str.data() );
	return arg.data;
}

word getbyte( const Arg &arg )
{
	if ( arg.type != ARG_IMM )
		error( "Bad byte type: %s", arg.str.data() );
	else if ( short( arg.data ) < -128 || arg.data > 255 )
		error( "Byte range error: %s", arg.str.data() );
	return arg.data & 0xFF;
}

word gethigh( const Arg &arg )
{
	return arg.data >> 8;
}

word getlow( const Arg &arg )
{
	return arg.data & 0xFF;
}

word getnum( const Arg &arg )
{
	if ( arg.data > 255 )
		error( "Number range error: %s", arg.str.data() );
	return arg.data & 0xFF;
}

word getoffset( word addr, const Arg &arg )
{
	short offset = arg.data - addr;
	if ( offset < -128 || offset > 127 )
	{
		error( "Offset range error: %s", arg.str.data() );
//		return 0xFE;
	}
	return offset & 0xFF;
}

void ass_mov( const string op, vector< Arg > &args, vector< byte > &instr )
{
	//			Rn,A	%n,A	Rn,B	Rn,Rn	%n,B	B,A		%n,R	A,B		A,Rn	B,Rn
	// 	AND		12		22		32		42		52		62		72		C0		D0		D1
	if ( chkargs( op, args, 2 ) )
	{
		switch( enum_pair( args[0].type, args[1].type ) )
		{
		case enum_pair( ARG_REG, ARG_A ):
			instr.push_back( 0x12 );
			instr.push_back( getnum( args[0] ) );
			break;
		case enum_pair( ARG_IMM, ARG_A ):
			instr.push_back( 0x22 );
			instr.push_back( getbyte( args[0] ) );
			break;
		case enum_pair( ARG_REG, ARG_B ):
			instr.push_back( 0x32 );
			instr.push_back( getnum( args[0] ) );
			break;
		case enum_pair( ARG_REG, ARG_REG ):
			instr.push_back( 0x42 );
			instr.push_back( getnum( args[0] ) );
			instr.push_back( getnum( args[1] ) );
			break;
		case enum_pair( ARG_IMM, ARG_B ):
			instr.push_back( 0x52 );
			instr.push_back( getbyte( args[0] ) );
			break;
		case enum_pair( ARG_B, ARG_A ):
			instr.push_back( 0x62 );
			break;
		case enum_pair( ARG_IMM, ARG_REG ):
			instr.push_back( 0x72 );
			instr.push_back( getbyte( args[0] ) );
			instr.push_back( getnum( args[1] ) );
			break;
		case enum_pair( ARG_A, ARG_B ):
			instr.push_back( 0xC0 );
			break;
		case enum_pair( ARG_A, ARG_REG ):
			instr.push_back( 0xD0 );
			instr.push_back( getnum( args[1] ) );
			break;
		case enum_pair( ARG_B, ARG_REG ):
			instr.push_back( 0xD1 );
			instr.push_back( getnum( args[1] ) );
			break;
		default:
			error( "Bad arg(s): %s %s,%s", op.data(), args[0].str.data(), args[1].str.data() );
		}
	}
}

void ass_movd( const string op, vector< Arg > &args, vector< byte > &instr )
{
	//			%n,Rn	Rn,Rn	%n(B),Rn
	//	MOVD	88		A8		98
	if ( chkargs( op, args, 2 ) )
	{
		switch( enum_pair( args[0].type, args[1].type ) )
		{
		case enum_pair( ARG_IMM, ARG_REG ):
			instr.push_back( 0x88 );
			instr.push_back( gethigh( args[0] ) );
			instr.push_back( getlow( args[0] ) );
			instr.push_back( getnum( args[1] ) );
			break;
		case enum_pair( ARG_REG, ARG_REG ):
			instr.push_back( 0x98 );
			instr.push_back( getnum( args[0] ) );
			instr.push_back( getnum( args[1] ) );
			break;
		case enum_pair( ARG_EFFEC, ARG_REG ):
			instr.push_back( 0xA8 );
			instr.push_back( gethigh( args[0] ) );
			instr.push_back( getlow( args[0] ) );
			instr.push_back( getnum( args[1] ) );
			break;
		default:
			error( "Bad arg(s): %s %s,%s", op.data(), args[0].str.data(), args[1].str.data() );
		}
	}
}

void ass_movp( const string op, vector< Arg > &args, vector< byte > &instr )
{
	//			A,Pn	B,Pn	%n,Pn	Pn,A	Pn,B
	//	MOVD	82		A2		92		80		91
	if ( chkargs( op, args, 2 ) )
	{
		switch( enum_pair( args[0].type, args[1].type ) )
		{
		case enum_pair( ARG_A, ARG_PORT ):
			instr.push_back( 0x82 );
			instr.push_back( getnum( args[1] ) );
			break;
		case enum_pair( ARG_B, ARG_PORT ):
			instr.push_back( 0x92 );
			instr.push_back( getnum( args[1] ) );
			break;
		case enum_pair( ARG_IMM, ARG_PORT ):
			instr.push_back( 0xA2 );
			instr.push_back( getbyte( args[0] ) );
			instr.push_back( getnum( args[1] ) );
			break;
		case enum_pair( ARG_PORT, ARG_A ):
			instr.push_back( 0x80 );
			instr.push_back( getnum( args[0] ) );
			break;
		case enum_pair( ARG_PORT, ARG_B ):
			instr.push_back( 0x91 );
			instr.push_back( getnum( args[0] ) );
			break;
		default:
			error( "Bad arg(s): %s %s,%s", op.data(), args[0].str.data(), args[1].str.data() );
		}
	}
}

void ass_xaddr( const string op, vector< Arg > &args, int bits, vector< byte > &instr )
{
	//			@n		*Rn		@n(B)
	//	LDA		8A		9A		AA
	//	STA		8B		9B		AB
	//	BR		8C		9C		AC
	//	CMPA	8D		9D		AD
	//	CALL	8E		9E		AE
	if ( chkargs( op, args, 1 ) )
	{
		switch( args[0].type )
		{
		case ARG_DIR:
			instr.push_back( 0x80 | bits );
			instr.push_back( gethigh( args[0] ) );
			instr.push_back( getlow( args[0] ) );
			break;
		case ARG_INDIR:
			instr.push_back( 0x90 | bits );
			instr.push_back( getnum( args[0] ) );
			break;
		case ARG_INDEX:
			instr.push_back( 0xA0 | bits );
			instr.push_back( gethigh( args[0] ) );
			instr.push_back( getlow( args[0] ) );
			break;
		default:
			error( "Bad arg: %s %s", op.data(), args[0].str.data() );
		}
	}
}

bool ass_unop( const string op, vector< Arg > &args, int num, int bits, vector< byte > &instr )
{
	//			A		B		Rn
	// 	DEC		B2		C2		D2
	//	INC		B3		C3		D3
	//	INV		B4		C4		D4
	//	CLR		B5		C5		D5
	//	XCHB	B6		C6		D6
	//	SWAP	B7		C7		D7
	//	PUSH	B8		C8		D8
	//	POP		B9		C9		D9
	//	DJNZ	BA		CA		DA
	//	DECD	BB		CB		DB
	//	RR		BC		CC		DC
	//	RRC		BD		CD		DD
	//	RL		BE		CE		DE
	//	RLC		BF		CF		DF
	int ok = chkargs( op, args, num );
	if ( ok )
	{
		switch( args[0].type )
		{
		case ARG_A:
			instr.push_back( 0xB0 | bits );
			break;
		case ARG_B:
			instr.push_back( 0xC0 | bits );
			break;
		case ARG_REG:
			instr.push_back( 0xD0 | bits );
			instr.push_back( getnum( args[0] ) );
			break;
		default:
			error( "Bad arg: %s %s", op.data(), args[0].str.data() );
		}
	}
	return ok;
}

bool ass_binop( const string op, vector< Arg > &args, int num, int bits, vector< byte > &instr )
{
	//			Rn,A	%n,A	Rn,B	Rn,Rn	%n,B	B,A		%n,R
	// 	AND		13		23		33		43		53		63		73
	// 	OR		14		24		34		44		54		64		74
	// 	XOR		15		25		35		45		55		65		75
	//	BTJO	16		26		36		46		56		66		76
	//	BTJZ	17		27		37		47		57		67		77
	//	ADD		18		28		38		48		58		68		78
	//	ADC		19		29		39		49		59		69		79
	//	SUB		1A		2A		3A		4A		5A		6A		7A
	//	SBB		1B		2B		3B		4B		5B		6B		7B
	//	MPY		1C		2C		3C		4C		5C		6C		7C
	//	CMP		1D		2D		3D		4D		5D		6D		7D
	//	DAC		1E		2E		3E		4E		5E		6E		7E
	//	DSB		1F		2F		3F		4F		5F		6F		7F
	bool ok = chkargs( op, args, num );
	if ( ok )
	{
		switch( enum_pair( args[0].type, args[1].type ) )
		{
		case enum_pair( ARG_REG, ARG_A ):
			instr.push_back( 0x10 | bits );
			instr.push_back( getnum( args[0] ) );
			break;
		case enum_pair( ARG_IMM, ARG_A ):
			instr.push_back( 0x20 | bits );
			instr.push_back( getnum( args[0] ) );
			break;
		case enum_pair( ARG_REG, ARG_B ):
			instr.push_back( 0x30 | bits );
			instr.push_back( getnum( args[0] ) );
			break;
		case enum_pair( ARG_REG, ARG_REG ):
			instr.push_back( 0x40 | bits );
			instr.push_back( getnum( args[0] ) );
			instr.push_back( getnum( args[1] ) );
			break;
		case enum_pair( ARG_IMM, ARG_B ):
			instr.push_back( 0x50 | bits );
			instr.push_back( getnum( args[0] ) );
			break;
		case enum_pair( ARG_B, ARG_A ):
			instr.push_back( 0x60 | bits );
			break;
		case enum_pair( ARG_IMM, ARG_REG ):
			instr.push_back( 0x70 | bits );
			instr.push_back( getnum( args[0] ) );
			instr.push_back( getnum( args[1] ) );
			break;
		default:
			error( "Bad arg(s): %s %s,%s", op.data(), args[0].str.data(), args[1].str.data() );
		}
	}
	return ok;
}

bool ass_binop_p( const string op, vector< Arg > &args, int num, int bits, vector< byte > &instr )
{
	//			A,Pn	B,Pn	%n,Pn
	// 	ANDP	83		93		A3
	// 	ORP		84		94		A4
	// 	XORP	85		95		A5
	//	BTJOP	86		96		A6
	//	BTJZP	87		97		A7
	bool ok = chkargs( op, args, num );
	if ( ok )
	{
		switch( enum_pair( args[0].type, args[1].type ) )
		{
		case enum_pair( ARG_A, ARG_PORT ):
			instr.push_back( 0x80 | bits );
			instr.push_back( getnum( args[1] ) );
			break;
		case enum_pair( ARG_B, ARG_PORT ):
			instr.push_back( 0x90 | bits );
			instr.push_back( getnum( args[1] ) );
			break;
		case enum_pair( ARG_IMM, ARG_PORT ):
			instr.push_back( 0xA0 | bits );
			instr.push_back( getbyte( args[0] ) );
			instr.push_back( getnum( args[1] ) );
			break;
		default:
			error( "Bad arg(s): %s %s,%s", op.data(), args[0].str.data(), args[1].str.data() );
		}
	}
	return ok;
}

void ass_jump( const string op, vector< Arg > &args, int bits, vector< byte > &instr )
{
	//		JMP		JN/JLT	JZ/JEQ	JC/JHS	JP/JGT	JPZ/JGE	JNZ/JNE	JNC/JL
	//		E0		E1		E2		E3		E4		E5		E6		E7
	if ( chkargs( op, args, 1 ) )
	{
		switch( args[0].type )
		{
		case ARG_IMM:
			instr.push_back( 0xE0 | bits );
			instr.push_back( getoffset( pc+2, args[0] ) );
			break;
		default:
			error( "Bad arg: %s %s", op.data(), args[0].str.data() );
		}
	}
}

void ass_trap( const string op, vector< Arg > &args, vector< byte > &instr )
{
	//			n
	//	TRAP	E8+n
	if ( chkargs( op, args, 1 ) )
	{
		switch( args[0].type )
		{
		case ARG_IMM:
			instr.push_back( 0xE8 + args[0].data );
			break;
		default:
			error( "Bad arg: %s %s", op.data(), args[0].str.data() );
		}
	}
}

void ass_pushpop( const string op, vector< Arg > &args, int bits, vector< byte > &instr )
{
	//			A		B		REG		ST
	//	PUSH	B8		C8		D8		0E
	//	POP		B9		C9		D9		08
	if ( chkargs( op, args, 1 ) )
	{
		if ( chkargs( op, args, 1 ) )
		{
			switch( args[0].type )
			{
			case ARG_A:
				instr.push_back( 0xB0 | bits );
				break;
			case ARG_B:
				instr.push_back( 0xC0 | bits );
				break;
			case ARG_REG:
				instr.push_back( 0xD0 | bits );
				instr.push_back( getnum( args[0] ) );
				break;
			case ARG_ST:
				instr.push_back( bits == 0x08 ? 0x0E : 0x08 );
				break;
			default:
				error( "Bad arg: %s %s", op.data(), args[0].str.data() );
			}
		}
	}
}

void ass_implicit( const string op, vector< Arg > &args, int bits, vector< byte > &instr )
{
	if ( chkargs( op, args, 0 ) )
	{
		instr.push_back( bits );
	}
}

void main( int argc, const char* argp[] )
{
	cerr << title << endl << endl;

	char buf[256];
	const string stab = "\t";

	string infile, outfile, lstfile;

	bool noheader = false;
	bool nolinenum = false;

	for ( int i=1; i<argc; ++i )
	{
		const char *p = argp[i];
		char c = *argp[i];
		if ( c == '-' || c == '/' )
		{
			++p;
			c = *p++;
			switch ( toupper( c ) )
			{
			case 'I':
				if ( *p == ':' )
					++p;
				infile = p;
				break;
			case 'O':
				if ( *p == ':' )
					++p;
				outfile = p;
				break;
			case 'L':
				if ( *p == ':' )
					++p;
				lstfile = p;
				break;
			case 'N':
				switch ( toupper( *p ) )
				{
				case 'H':
					noheader = true;
					break;
				case 'N':
					nolinenum = true;
					break;
				}
				break;
			case '?':
				cerr << help << endl;
				exit( 0 );
				break;
			}
		}
	}

	if 	( !infile.empty() && infile.find( "." ) == string::npos )
	{
		infile += ".asm";
	}

	if 	( !outfile.empty() && outfile.find( "." ) == string::npos )
	{
		outfile += ".cim";
	}

	if 	( !lstfile.empty() && lstfile.find( "." ) == string::npos )
	{
		lstfile += ".lst";
	}

	ifstream in( infile.data() );

	if ( !in )
	{
		cerr << "Failed to open input file [" << infile << "]" << endl;
		exit( 1 );
	}

	ofstream out;

	if ( !outfile.empty() )
	{
			out.open( outfile.data(), ios::binary );

		if ( !out )
		{
			cerr << "Failed to open output file [" << outfile << "]" << endl;
			exit( 1 );
		}
	}

	ofstream lst;

	if ( !lstfile.empty() )
	{
		lst.open( lstfile.data(), ios::binary );

		if ( !lst )
		{
			cerr << "Failed to open listing file [" << lstfile << "]" << endl;
			exit( 1 );
		}
	}

	ostream &ostr = !lstfile.empty() ? lst : cout;

	if ( !noheader )
	{
		ostr << title << endl << endl;

		ostr  << "\t" << timeStr() << endl << endl;
		if ( !outfile.empty() )
			ostr  << "\tAssembly of  : " << infile << endl;
		ostr  << "\tOutput file  : " << outfile << endl;
		if ( !lstfile.empty() )
			ostr  << "\tListing file : " << lstfile << endl;

		ostr  << endl;
	}

	for ( pass=1; pass<=2; ++pass )
	{
		cerr << "Pass: " << pass << endl;
		pc = 0;
		bool end = false;
		int num = 1;

		in.clear();
		in.seekg(0, ios::beg);

		int errcount = 0;

		while( in.getline( buf, sizeof buf ) )
		{
			string 	line = buf;

			errors.clear();

			vector< string > tokens = split( line, '\t' );

			// get tokens
			string label, op, comment;
			vector< string > argstrs;

			for ( int i=0; i<tokens.size(); ++i )
			{
				const string &token = tokens[i];
				if ( token[0] == ';' )
				{
					comment = token;
					break;
				}
				else
				{
					switch ( i )
					{
					case 0:
						label = token;
						break;
					case 1:
						op = token;
						break;
					case 2:
						argstrs = split( token, ',' );
						break;
					default:
						error( "Extra token %d: %s", i, token.data() );
						break;
					}
				}
			}

			size_t nargs = argstrs.size();

			vector< Arg > args( nargs );
			for ( int i=0; i<nargs; ++i )
			{
				args[i] = getarg( argstrs[i] );
			}

			word addr = pc;

			if ( !label.empty() )
			{
				if ( op == "AORG" || op == "EQU" )
				{
					if ( chkargs( op, args, 1 ) )
					{
						addr = getimmediate( args[0] );
						if ( op == "AORG" )
							pc = addr;
					}
				}

				symbolptr_t sym = symbols.find( label );
				if ( pass == 2 )
				{
					if ( sym != symbols.end() && sym->second != addr )
						error ( "Multiple definition: %s", label.data() );
				}
				symbols[label] = addr;
			}

			vector< byte > instr;

			bool first = true;

			// ============= AORG
			if ( op == "AORG" )	// AORG aaaa
			{
				if ( label.empty() )
				{
					if ( chkargs( op, args, 1 ) )
					{
						addr = pc = getimmediate( args[0] );
					}
				}
			}
			// ============= MOVE
			else if ( op == "MOV" ) // MOV ss,dd
			{
				ass_mov( op, args, instr );
			}
			else if ( op == "MOVD" ) // MOVD ssss,dddd
			{
				ass_movd( op, args, instr );
			}
			else if ( op == "MOVP" ) // MOVP ss,dd
			{
				ass_movp( op, args, instr );
			}
			// ============= MEMORY ACCESS
			else if ( op == "LDA" ) // LDA @aaaa[(B)],dd
			{
				ass_xaddr( op, args, 0x0A, instr );
			}
			else if ( op == "STA" ) // STA dd,@aaaa[(B)]
			{
				ass_xaddr( op, args, 0x0B, instr );
			}
			else if ( op == "BR" ) // BR @aaaa[(B)] (long jump)
			{
				ass_xaddr( op, args, 0x0C, instr );
			}
			else if ( op == "CMPA" ) // CMPA @aaaa[(B)]
			{
				ass_xaddr( op, args, 0x0D, instr );
			}
			else if ( op == "CALL" ) // CALL aaaa
			{
				ass_xaddr( op, args, 0x0E, instr );
			}
			// ============= IMPLICIT (no operands)
			else if ( op == "NOP"  )  // NOP
			{
				ass_implicit( op, args, 0x01, instr );
			}
			else if ( op == "IDLE"  ) // IDLE
			{
				ass_implicit( op, args, 0x01, instr );
			}
			else if ( op == "EINT" ) // EINT
			{
				ass_implicit( op, args, 0x05, instr );
			}
			else if ( op == "DINT" ) // DINT
			{
				ass_implicit( op, args, 0x06, instr );
			}
			else if ( op == "SETC" ) // SETC
			{
				ass_implicit( op, args, 0x07, instr );
			}
			else if ( op == "STSP" ) // STSP
			{
				ass_implicit( op, args, 0x09, instr );
			}
			else if ( op == "RETS" ) // RETS (from subroutine)
			{
				ass_implicit( op, args, 0x0A, instr );
			}
			else if ( op == "RETI" ) // RETI (from interrupt)
			{
				ass_implicit( op, args, 0x0B, instr );
			}
			else if ( op == "LDSP" ) // LDSP
			{
				ass_implicit( op, args, 0x0D, instr );
			}
			else if ( op == "TSTA" || op == "CLRC" ) // TSTA/CLRC
			{
				ass_implicit( op, args, 0xB0, instr );
			}
			else if ( op == "TSTB" ) // TSTB
			{
				ass_implicit( op, args, 0xC1, instr );
			}
			// ============= UNARY OPS
			else if ( op == "DEC"  ) // DEC xx
			{
				ass_unop( op, args, 1, 0x02, instr );
			}
			else if ( op == "INC"  ) // INC xx
			{
				ass_unop( op, args, 1, 0x03, instr );
			}
			else if ( op == "INV"  ) // INV xx
			{
				ass_unop( op, args, 1, 0x04, instr );
			}
			else if ( op == "CLR"  ) // CLR xx
			{
				ass_unop( op, args, 1, 0x05, instr );
			}
			else if ( op == "XCHB" ) // XCHB xx
			{
				ass_unop( op, args, 1, 0x06, instr );
			}
			else if ( op == "SWAP" ) // SWAP xx
			{
				ass_unop( op, args, 1, 0x07, instr );
			}
			else if ( op == "DECD" ) // DECD Rnn
			{
				ass_unop( op, args, 1, 0x0B, instr );
			}
			else if ( op == "RR"   ) // RR xx
			{
				ass_unop( op, args, 1, 0x0C, instr );
			}
			else if ( op == "RRC"  ) // RRC xx
			{
				ass_unop( op, args, 1, 0x0D, instr );
			}
			else if ( op == "RL"   ) // RL xx
			{
				ass_unop( op, args, 1, 0x0E, instr );
			}
			else if ( op == "RLC"  ) // RLC xx
			{
				ass_unop( op, args, 1, 0x0F, instr );
			}
			// ============= BINARY OPERATIONS
			else if ( op == "AND" ) // AND yy,xx
			{
				ass_binop( op, args, 2, 0x03, instr );
			}
			else if ( op == "OR"  ) // OR yy,xx
			{
				ass_binop( op, args, 2, 0x04, instr );
			}
			else if ( op == "XOR" ) // XOR yy,xx
			{
				ass_binop( op, args, 2, 0x05, instr );
			}
			else if ( op == "ADD" ) // ADD yy,xx
			{
				ass_binop( op, args, 2, 0x08, instr );
			}
			else if ( op == "ADC" ) // ADC yy,xx
			{
				ass_binop( op, args, 2, 0x09, instr );
			}
			else if ( op == "SUB" ) // SUB yy,xx
			{
				ass_binop( op, args, 2, 0x0A, instr );
			}
			else if ( op == "SBB" ) // SBB yy,xx
			{
				ass_binop( op, args, 2, 0x0B, instr );
			}
			else if ( op == "MPY" ) // MPY yy,xx
			{
				ass_binop( op, args, 2, 0x0C, instr );
			}
			else if ( op == "CMP" ) // CMP yy,xx
			{
				ass_binop( op, args, 2, 0x0D, instr );
			}
			else if ( op == "DAC" ) // DAC yy,xx
			{
				ass_binop( op, args, 2, 0x0E, instr );
			}
			else if ( op == "DSB" ) // DSB yy,xx
			{
				ass_binop( op, args, 2, 0x0F, instr );
			}
			// ============= BINARY OPERATIONS ON PORTS
			else if ( op == "ANDP" ) // ANDP yy,Pnn
			{
				ass_binop_p( op, args, 2, 0x03, instr );
			}
			else if ( op == "ORP" ) // ORP yy,Pnn
			{
				ass_binop_p( op, args, 2, 0x04, instr );
			}
			else if ( op == "XORP" ) // ORP yy,Pnn
			{
				ass_binop_p( op, args, 2, 0x05, instr );
			}
			// ============= SHORT JUMPS
			else if ( op == "JMP" )                // JMP aaaa
			{
				ass_jump( op, args, 0x00, instr );
			}
			else if ( op == "JN" || op == "JLT"  ) // JN/LT aaaa (jump if negative/less than)
			{
				ass_jump( op, args, 0x01, instr );
			}
			else if ( op == "JZ" || op == "JEQ"  ) // JZ/JEQ aaaa (jump if zero/equal)
			{
				ass_jump( op, args, 0x02, instr );
			}
			else if ( op == "JC" || op == "JHS"  ) // JC/?JHS aaaa (jump if carry/?higher or same)
			{
				ass_jump( op, args, 0x03, instr );
			}
			else if ( op == "JP" || op == "JGT"  ) // JP/JGT aaaa (jump if positive/greater than)
			{
				ass_jump( op, args, 0x04, instr );
			}
			else if ( op == "JPZ" || op == "JGE" ) // JPZ/JGE aaaa (jump if positive or zero/greater or equal)
			{
				ass_jump( op, args, 0x05, instr );
			}
			else if ( op == "JNZ" || op == "JNE" ) // JNZ/JNZ aaaa (jump if !zero/!equal)
			{
				ass_jump( op, args, 0x06, instr );
			}
			else if ( op == "JNC" || op == "JL"  ) // JNC/JL aaaa (jump if !carry/?lower)
			{
				ass_jump( op, args, 0x07, instr );
			}
			else if ( op == "DJNZ" ) 			   // DJNZ R,aaaa (jump if !carry/?lower)
			{
				if ( ass_unop( op, args, 2, 0x0A, instr ) )
				{
					instr.push_back( getoffset( pc+3, args[0] ) );
				}
			}
			// ============= BIT TESTS & SHORT JUMPS
			else if ( op == "BTJO" ) // BTJO %yy,xx,aaaa (jump if any bit of xx masked by %yy is one)
			{
				if ( ass_binop( op, args, 3, 0x06, instr ) )
				{
					instr.push_back( getoffset( pc+instr.size()+1, args[2] ) );
				}
			}
			else if ( op == "BTJOP" ) // BTJOP %yy,Pnn,aaaa (jump if any bit of Pnn masked by %yy is one)
			{
				if ( ass_binop_p( op, args, 3, 0x06, instr ) )
				{
					instr.push_back( getoffset( pc+instr.size()+1, args[2] ) );
				}
			}
			else if ( op == "BTJZ" ) // BTJZ %yy,xx,aaaa (jump if any bit of xx masked by %yy is zero)
			{
				if ( ass_binop( op, args, 3, 0x07, instr ) )
				{
					instr.push_back( getoffset( pc+instr.size()+1, args[2] ) );
				}
			}
			else if ( op == "BTJZP" ) // BTJZP %yy,Pnn,aaaa (jump if any bit of Pnn masked by %yy is zero)
			{
				if ( ass_binop_p( op, args, 3, 0x07, instr ) )
				{
					instr.push_back( getoffset( pc+instr.size()+1, args[2] ) );
				}
			}
			// ============= PUSH/POP
			else if ( op == "PUSH" ) // PUSH xx
			{
				ass_pushpop( op, args, 0x08, instr );
			}
			else if ( op == "POP" ) // POP xx
			{
				ass_pushpop( op, args, 0x09, instr );
			}
			// ============= SPECIAL
			else if ( op == "TRAP" ) // POP xx
			{
				ass_trap( op, args, instr );
			}
			// ============= DATA
			else if ( op == "BYTE" ) // BYTE x,... (8-bit data)
			{
				if ( !args.size() )
					error( "Missing byte value(s)" );
				for ( int i=0; i<args.size(); ++i )
				{
					instr.push_back( getbyte( args[i] ) );
				}
			}
			else if ( op == "TEXT" ) // TEXT "..."
			{
				if ( chkargs( op, args, 1 ) )
				{
					bool quot = false, neg = false, esc = false;
					const string &text = args[0].str;
					for ( int i=0; i<text.size(); ++i )
					{
						char c = text[i];
						if ( !quot )
						{
							if ( !i && c == '-' )
								neg = true;
							else if ( c == '\'' )
								quot = true;
						}
						else
						{
							if ( !esc && c == '\\' )
							{
								esc = true;
							}
							else if ( !esc && c == '\'' )
							{
								quot = false;
								break;
							}
							else
							{
								esc = false;
								instr.push_back( byte( c ) );
							}
						}
					}
				}
			}
			else if ( op == "DATA" ) // DATA xxxx,... (16-bit data)
			{
				if ( !args.size() )
					error( "Missing byte value(s)" );
				for ( int i=0; i<args.size(); ++i )
				{
					instr.push_back( gethigh( args[i] ) );
					instr.push_back( getlow( args[i] ) );
				}
			}
			else if ( op == "EQU" ) // llll EQU xxxx
			{
				// no-op
			}
			else if ( op == "END" ) // END [aaaa]
			{
				end = true;
			}
			else if ( !op.empty() ) // Unrecognized op-code
			{
				error( "Unrecognized op-code: %s", op.data() );
			}

			if ( pass == 2 )
			{
				stringstream sstr;
				if ( !nolinenum )
					sstr << setw( 5 ) << num << ":  ";
				sstr << hex << uppercase << setfill( '0' );
				if ( !op.empty() )
					sstr << setw( 4 ) << addr << "  ";
				else
					sstr << "      ";
				int i;
				//sstr << instr.size() << ":";
				for ( i=0; i<instr.size() && i<4; ++i )
					sstr << setw(2) << int(word(instr[i]));
				for ( int j=i; j<5; ++j )
					sstr << "  ";
				sstr << line << endl;
				while ( i<instr.size() )
				{
					int imax = i+4;
					addr += 4;
					if ( !nolinenum )
						sstr << "        ";
					sstr << "      ";
					for ( ; i<instr.size() && i<imax; ++i )
						sstr << setw(2) << int(word(instr[i]));
					sstr << endl;
				}

				ostr << sstr.str();
				if ( !errors.empty() )
				{
					for ( i=0; i<errors.size(); ++i )
						ostr << "*** " << errors[i] << endl;
					if ( !lstfile.empty() && cout != cerr )
					{
						cerr << sstr.str();
						for ( i=0; i<errors.size(); ++i )
							cerr << "*** " << errors[i] << endl;
					}
					errcount += errors.size();
				}

				if ( out )
					for ( i=0; i<instr.size(); ++i )
						out.put( instr[i] );
			}

			pc += instr.size();
			++num;

			if ( end )
				break;
		}

		if ( pass == 2 )
		{
			stringstream sstr;
			sstr << setw( 5 ) << errcount << " TOTAL ERROR(S)" << endl;

			ostr << sstr.str();

			if ( !lstfile.empty() && cout != cerr )
			{
				cerr << sstr.str();
			}
		}
	}


	in.close();
	out.close();

}
