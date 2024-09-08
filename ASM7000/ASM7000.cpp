#include "Options.h"
#include "Parser.h"
#include "Source.h"

#include <iomanip>
#include <sstream>

using namespace std;

const char title[] = "** TMS-7000 Tiny Assembler - v0.3.0-alpha+dev - (C) 2024 GmEsoft, All rights reserved. **";

const char help[] =
	"Usage:   ASM7000 [options] -i:InputFile[.asm] -o:OutputFile[.cim] [-l:Listing[.lst]]\n"
	"         -I:inputfile[.asm[   input source file\n"
	"         -O:outputfile[.cim]  output object file\n"
	"         -L:listing[.lst]     listing file\n"
	"Options: -NC  no compatibility warning\n"
	"         -ND- enable debug output\n"
	"         -NE  no output to stderr\n"
	"         -NH  no header in listing\n"
	"         -NN  no line numbers in listing\n"
	"         -NW  no warning\n"
	;


word pc = 0;
int pass;

stack< Options > optstack;

Options options;

Symbols symbols;


FunctionSeq_t functions;

/////// UTILITIES /////////////////////////////////////////////////////////////

#define enum_pair( A, B ) ( ( (A) << 4 ) | (B) )


/////// ARGS //////////////////////////////////////////////////////////////////

bool chkargs( const string& op, const vector< Arg > &args, size_t num )
{
	if ( args.size() < num )
		log.error( "%s: Too few args %d, expecting %d", op.data(), args.size(), num );
	else if ( args.size() > num )
		log.error( "%s: Too many args %d, expecting %d", op.data(), args.size(), num );
	return args.size() == num;
}

word getimmediate( const Arg &arg )
{
	if ( arg.type != ARG_IMM )
		log.error( "Expecting immediate: [%s] (%s)", arg.str.data(), ArgTypes::get(arg.type) );
	return arg.data;
}

word getbyte( const Arg &arg )
{
	if ( arg.type != ARG_IMM && arg.type != ARG_REG )
		log.error( "Bad byte type: [%s]=%04X (%s)", arg.str.data(), arg.data, ArgTypes::get(arg.type) );
	else if ( short( arg.data ) < -128 || arg.data > 255 )
		log.error( "Byte range error: [%s]=%04X (%s)", arg.str.data(), arg.data, ArgTypes::get(arg.type) );
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
	word ret = arg.data;
	if ( arg.type == ARG_PORT )
		ret -= 0x100;
	if ( ret > 0xFF )
		log.error( "Number range error: [%s]=%d (%s)", arg.str.data(), arg.data, ArgTypes::get(arg.type) );
	return ret & 0xFF;
}

word getoffset( word addr, const Arg &arg )
{
	short offset = arg.data - addr;
	if ( offset < -128 || offset > 127 )
	{
		log.error( "Offset range error: [%s] (%s)", arg.str.data(), ArgTypes::get(arg.type) );
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
			log.error( "Bad arg(s): %s %s,%s (%s,%s)",
				op.data(), args[0].str.data(), args[1].str.data(), ArgTypes::get(args[0].type), ArgTypes::get(args[1].type) );
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
			log.error( "Bad arg(s): %s %s,%s (%s,%s)",
				op.data(), args[0].str.data(), args[1].str.data(), ArgTypes::get(args[0].type), ArgTypes::get(args[1].type) );
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
			log.error( "Bad arg(s): %s %s,%s (%s,%s)",
				op.data(), args[0].str.data(), args[1].str.data(), ArgTypes::get(args[0].type), ArgTypes::get(args[1].type) );
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
		case ARG_IMM: // extension
		case ARG_REG: // extension
			if ( !options.nocompatwarning )
				log.warn( "Got type %s, assuming DIR: %s=%04X", ArgTypes::get(args[0].type), args[0].str.data(), args[0].data );
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
			log.error( "Bad arg: %s [%s] (%s)", op.data(), args[0].str.data(), ArgTypes::get(args[0].type) );
			instr.push_back( 0x80 | bits );
			instr.push_back( gethigh( args[0] ) );
			instr.push_back( getlow( args[0] ) );
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
			log.error( "Bad arg: %s [%s] (%s)", op.data(), args[0].str.data(), ArgTypes::get(args[0].type) );
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
			log.error( "Bad arg(s): %s %s,%s (%s,%s)",
				op.data(), args[0].str.data(), args[1].str.data(), ArgTypes::get(args[0].type), ArgTypes::get(args[1].type) );
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
			log.error( "Bad arg(s): %s %s,%s (%s,%s)",
				op.data(), args[0].str.data(), args[1].str.data(), ArgTypes::get(args[0].type), ArgTypes::get(args[1].type) );
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
			log.error( "Bad arg: %s [%s] (%s)", op.data(), args[0].str.data(), ArgTypes::get(args[0].type) );
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
			instr.push_back( 0xFF - args[0].data );
			break;
		default:
			log.error( "Bad arg: %s [%s] (%s)", op.data(), args[0].str.data(), ArgTypes::get(args[0].type) );
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
				log.error( "Bad arg: %s [%s] (%s)", op.data(), args[0].str.data(), ArgTypes::get(args[0].type) );
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


/////// MAIN //////////////////////////////////////////////////////////////////

Log log;

void main( int argc, const char* argp[] )
{
	cerr << title << endl << endl;

	char buf[256];
	const string stab = "\t";

	string infile, outfile, lstfile;

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
				c = toupper( *p );
				++p;
				switch ( toupper( c ) )
				{
				case 'C':
					options.nocompatwarning = ( *p != '-' );
					break;
				case 'D':
					options.nodebug = ( *p != '-' );
					break;
				case 'E':
					options.nocerr = ( *p != '-' );
					break;
				case 'H':
					options.noheader = ( *p != '-' );
					break;
				case 'N':
					options.nolinenum = ( *p != '-' );
					break;
				case 'W':
					options.nowarning = ( *p != '-' );
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


	Source in( infile );

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

	if ( !options.noheader )
	{
		ostr << title << endl << endl;

		ostr  << "\t" << Strings::timeStr() << endl << endl;
		if ( !outfile.empty() )
			ostr  << "\tAssembly of  : " << infile << endl;
		ostr  << "\tOutput file  : " << outfile << endl;
		if ( !lstfile.empty() )
			ostr  << "\tListing file : " << lstfile << endl;

		ostr  << endl;
	}

	symbols.beginSymbols();

	for ( int i=0; i<0x100; ++i )
	{
		stringstream sstr;
		sstr << "R" << i;
		string label = sstr.str();
		Arg arg = { ARG_REG, i, label, "" };
		symbols.addSymbol( label, arg );
	}

	for ( int i=0; i<0x100; ++i )
	{
		stringstream sstr;
		sstr << "P" << i;
		string label = sstr.str();
		Arg arg = { ARG_PORT, i + 0x100, label, "" };
		symbols.addSymbol( label, arg );
	}

	Arg argDate = { ARG_TEXT, 0, "DATE", "DD-MM-YYYY" };
	symbols.addSymbol( "DATE", argDate );

	Arg argTime = { ARG_TEXT, 0, "DATE", "HH:MM:SS" };
	symbols.addSymbol( "TIME", argTime );


	for ( pass=1; pass<=2; ++pass )
	{
		log.setEnabled( pass == 2 );
		log.setDebug( !options.nodebug );
		log.setWarning( !options.nowarning );

		cerr << "Pass: " << pass << endl;
		pc = 0;
		bool end = false;

		in.rewind();

		int errcount = 0;
		int warncount = 0;

#if MACRO
		map< string, Macro > macros;
		Macro macro;
#endif

		stack< Source > sources;
		stack< int > conditions;
		bool condit = true;

		functions.clear();

		while( true )
		{
			string 	line = in.getline();

			bool isline = in;

			if ( !isline )
			{
				if (!sources.empty() )
				{
					//log.info( "END INCLUDE" );
					in = sources.top();
					log.info( "File: %s ***", in.getname().data() );
					sources.pop();
					symbols.endSymbols();
				}
				else
				{
					log.warn( "No END statement" );
					end = true;
				}
			}

			int num = in.linenum();

			vector< string > tokens = Strings::split( line, "\t :" );

			// get tokens
			string label, op, comment, argstr;


			for ( int i=0; i<tokens.size(); ++i )
			{
				const string token = tokens[i];
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
						label = Strings::touppernotquoted( token );
						if ( !label.empty() && label[label.size()-1] == ':' )
							label = label.substr( 0, label.size()-1 );
						break;
					case 1:
						op = Strings::touppernotquoted( token );
						break;
					case 2:
						argstr = token;
						break;
					default:
						argstr += " " + token;
						break;
					}
				}
			}

			vector< string > argstrs = Strings::split( argstr, "," );
			size_t nargs = argstrs.size();

			word addr = pc;
			vector< byte > instr;

			bool outaddr = false;
			bool listblock = true;
			bool oldcond = condit;

#if MACRO
			if ( macro )
			{
				CDBG << "if ( macro ) ok" << endl;
				if ( !macro.add( op, line ) )
				{
					CDBG << "if ( !macro.add() ) ok" << endl;
					if ( macro.gettype() == "REPT" )
					{
						CDBG << "macro rept push" << endl;
						Arg arg = Parser::getarg( Strings::touppernotquoted( macro.args() ) );
						macro.rept( arg.data );
						sources.push( in );
						in = Source( "REPT", macro );
						symbols.beginSymbols();
						log.info( "Macro: %s ***", in.getname().data() );
					}
				}
				CDBG << "if ( macro ) ok" << endl;
			}
			else if ( op == "REPT" )
			{
				CDBG << "if ( op == REPT )" << endl;
				macro = Macro( "REPT", "REPT", argstr );
				CDBG << "if ( op == REPT )" << endl;
			}
			else
#endif
			if ( op == "IF" || op == "$IF" || op == "COND" )
			{
				conditions.push( condit );
				if ( nargs == 1 )
				{
					if ( condit )
					{
						Arg arg = Parser::getarg( Strings::touppernotquoted( argstrs[0] ) );
						condit = bool( arg.data );
					}
				}
				else if ( nargs == 0 )
				{
					log.error( "IF: missing argument" );
				}
				else
				{
					log.error( "IF: too many argument" );
				}
			}
			else if ( op == "ELSE" || op == "$ELSE" )
			{
				if ( !conditions.empty() )
				{
					if ( nargs == 0 )
					{
						condit = conditions.top() && !condit;
					}
					else
					{
						log.error( "IF: too many argument" );
					}
				}
				else
				{
					log.error( "ELSE without IF" );
				}
			}
			else if ( op == "ENDIF" || op == "$ENDIF" || op == "$ENDC" )
			{
				if ( !conditions.empty() )
				{
					if ( nargs == 0 )
					{
						condit = conditions.top();
						conditions.pop();
					}
					else
					{
						log.error( "IF: too many argument" );
					}
				}
				else
				{
					log.error( "ENDIF" );
				}
			}
			else if ( condit )
			{

				if ( op == "COPY" || op == "INCLUDE" || op == "GET" )
				{
					if ( nargs == 1 )
					{
						sources.push( in );
						in = Source( argstrs[0] );
						if ( in )
						{
							log.info( "File: %s ***", in.getname().data() );
							symbols.beginSymbols();
						}
						else
						{
							log.error( "Can't open include file %s", argstrs[0].data() );
							in = sources.top();
							sources.pop();
						}
					}
					else
					{
						log.error( "Expecting 1 arg %s", argstr.data() );
					}
				}
				else if ( op == "SAVE" )
				{
					optstack.push( options );
				}
				else if ( op == "RESTORE" )
				{
					if ( optstack.empty() )
					{
						log.error( "No saved options" );
					}
					else
					{
						options = optstack.top();
						optstack.pop();
					}
				}
				else if ( op == "CPU" )
				{
					log.debug( "%s %s ignored", op.data(), argstr.data() );
				}
				else if ( op == "PAGE" )
				{
					if ( nargs == 1 )
					{
						string arg = Strings::touppernotquoted( argstrs[0] );
						options.page = arg == "ON" || arg == "1";
					}
					else
					{
						log.error( "Expecting 1 arg %s", argstr.data() );
					}
				}
				else if ( op == "LISTING" )
				{
					if ( nargs == 1 )
					{
						string arg = Strings::touppernotquoted( argstrs[0] );
						options.list = arg == "ON" || arg == "1";
					}
					else
					{
						log.error( "Expecting 1 arg %s", argstr.data() );
					}
				}
				else if ( op == "FUNCTION" || op == "FUNC" )
				{
					if ( nargs == 0 )
					{
						log.error( "Missing arg %s", argstr.data() );
					}
					else if ( label.empty() )
					{
						log.error( "Missing function label" );
					}
					else if ( functions.find( label ) != functions.end() )
					{
						log.error( "Duplicate function definition: %s", label.data() );
					}
					else
					{
						Function func( label, argstrs );
						functions[label] = func;
					}
				}
				else if ( op == "MACRO" )
				{
					log.warn( "%s %s currently not handled", op.data(), argstr.data() );
				}
				else
				{
					vector< Arg > args( nargs );
					for ( int i=0; i<nargs; ++i )
					{
						args[i] = Parser::getarg( Strings::touppernotquoted( argstrs[i] ) );
					}

					ArgType type = ARG_IMM;

					if ( !label.empty() )
					{
						if ( op == "AORG" || op == "ORG" )
						{
							if ( chkargs( op, args, 1 ) )
							{
								addr = getimmediate( args[0] );
								if ( op == "AORG" || op == "ORG" )
									pc = addr;
							}
						}
						else if ( op == "EQU" )
						{
							if ( chkargs( op, args, 1 ) )
							{
								switch ( args[0].type )
								{
								case ARG_IMM:
								case ARG_REG:
								case ARG_PORT:
									addr = args[0].data;
									type = args[0].type;
									//log.error( "debug: set [%s] (%s=%04x)", args[0].str.data(), ArgTypes::get(type], addr );
									break;
								default:
									log.error( "Bad addressing mode: [%s] (%s)", args[0].str.data(), ArgTypes::get(args[0].type) );
								}
							}
						}

						Arg sym = symbols.getSymbol( label );
						if ( pass == 2 )
						{
							if ( sym.type != ARG_UNDEF && sym.data != addr )
								log.error ( "Multiple definition: [%s] (%s=%04X)",
									label.data(), ArgTypes::get(sym.type), sym.data );
						}
						Arg arg = { type, addr, label, "" };
						symbols.addSymbol( label, arg );
					}


					bool first = true;

					outaddr = true;

					// ============= AORG
					if ( op == "AORG" || op == "ORG" )	// AORG aaaa
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
						ass_implicit( op, args, 0x00, instr );
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
							instr.push_back( getoffset( pc+instr.size()+1, args[1] ) );
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
							log.error( "Missing byte value(s)" );
						for ( int i=0; i<args.size(); ++i )
						{
							instr.push_back( getbyte( args[i] ) );
						}
					}
					else if ( op == "DB" ) // DB x,... (8-bit data)
					{
						if ( !options.nocompatwarning )
							log.warn( "Non-standard DB statement: %s", argstr.data() );
						if ( !args.size() )
							log.error( "Missing byte value(s)" );
						for ( int i=0; i<args.size(); ++i )
						{
							const Arg &arg = args[i];
							switch( arg.type )
							{
							case ARG_IMM:
								instr.push_back( getbyte( arg ) );
								break;
							case ARG_DUP:
								listblock = false;
							case ARG_TEXT:
								for ( int p=0; p<arg.text.size(); ++p )
									instr.push_back( arg.text[p] );
								break;
							default:
								log.error( "Bad arg type: %s", ArgTypes::get(arg.type) );
							}

						}
					}
					else if ( op == "DS" ) // DS x,... (8-bit data)
					{
						if ( !options.nocompatwarning )
							log.warn( "Non-standard DB statement: %s", argstr.data() );
						if ( chkargs( op, args, 1 ) )
						{
							int skip = args[0].data;
							pc += skip;
						}
					}
					else if ( op == "TEXT" ) // TEXT "..."
					{
						if ( chkargs( op, args, 1 ) )
						{
							const Arg &arg = args[0];
							switch( arg.type )
							{
							case ARG_TEXT:
								for ( int p=0; p<arg.text.size(); ++p )
									instr.push_back( arg.text[p] );
								break;
							default:
								log.error( "Bad arg type: %s", ArgTypes::get(arg.type) );
							}
						}
					}
					else if ( op == "DATA" || op == "DW" ) // DATA xxxx,... (16-bit data)
					{
						if ( !options.nocompatwarning && op == "DW" )
							log.warn( "Got DW, assuming DATA: %s", argstr.data() );
						if ( !args.size() )
							log.error( "Missing byte value(s)" );
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
					else if ( op == "ERROR" || op == "WARNING" || op == "MESSAGE" || op == "INFO" ) //
					{
						if ( chkargs( op, args, 1 ) )
						{
							const Arg &arg = args[0];
							switch( arg.type )
							{
							case ARG_TEXT:
								if ( op == "ERROR" )
									log.error( "%s", arg.text.data() );
								else if ( op == "WARNING" )
									log.warn( "%s", arg.text.data() );
								else
									log.info( "%s", arg.text.data() );
								break;
							default:
								log.error( "Bad arg type: %s", ArgTypes::get(arg.type) );
							}
						}
					}
					else if ( op == "ASSERT_EQUAL" ) //
					{
						if ( chkargs( op, args, 2 ) )
						{
							const Arg &arg0 = args[0];
							const Arg &arg1 = args[1];
							if ( arg0.type == ARG_TEXT && arg1.type == ARG_TEXT )
							{
								if ( arg0.text != arg1.text )
								{
									log.error( "Assertion failed: %s != %s", arg0.str.data(), arg1.str.data() );
									log.info( "with %s = '%s'", arg0.str.data(), arg0.text.data() );
									log.info( " and %s = '%s'", arg1.str.data(), arg1.text.data() );
								}
							}
							else if ( arg0.type != ARG_TEXT && arg1.type != ARG_TEXT )
							{
								if ( arg0.data != arg1.data )
								{
									log.error( "Assertion failed: %s != %s", arg0.str.data(), arg1.str.data() );
									log.info( "with %s = %04X", arg0.str.data(), arg0.data );
									log.info( " and %s = %04X", arg1.str.data(), arg1.data );
								}
							}
							else
							{
								log.error( "Assertion failed: type of %s incompatible with type of %s", arg0.str.data(), arg1.str.data() );
								log.info( "with %s as %s", arg0.str.data(), ArgTypes::get(arg0.type) );
								log.info( " and %s as %s", arg1.str.data(), ArgTypes::get(arg1.type) );
							}
						}
					}
					else if ( op.empty() )
					{
						outaddr = false;
					}
					else					// Unrecognized op-code
					{
						log.error( "Unrecognized op-code: [%s]", op.data() );
					}
				}
			}

			if 	( 	( 	options.listall
					|| 	(	options.list
						&& 	( 	options.clist || condit || oldcond )
						)
					)
				&& 	!options.nolist
				&& 	pass == 2
				)
			{
				if ( isline )
				{
					stringstream sstr;

					if ( !options.nolinenum )
						sstr << setw( 5 ) << num << ":  ";
					sstr << hex << uppercase << setfill( '0' );
					if ( outaddr )
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
					while ( listblock && i<instr.size() )
					{
						int imax = i+4;
						addr += 4;
						if ( !options.nolinenum )
							sstr << "        ";
						sstr << "      ";
						for ( ; i<instr.size() && i<imax; ++i )
							sstr << setw(2) << int(word(instr[i]));
						sstr << endl;
					}

					ostr << sstr.str();

					if ( !options.nocerr && !lstfile.empty() && cout != cerr
						&& log.isWarning() )
					{
						CDBG << sstr.str();
					}

				}

				log.writeTo( ostr );

				if ( !options.nocerr && !lstfile.empty() && cout != cerr )
				{
					log.writeTo( cerr );
				}

				if ( out )
					for ( int i=0; i<instr.size(); ++i )
						out.put( instr[i] );
			}

			errcount += log.getErrorsCount();
			warncount += log.getWarningsCount();

			log.clear();

			pc += instr.size();


			if ( end && sources.empty() )
				break;

		}


		if ( pass == 2 )
		{
			stringstream sstr;

			sstr << setw( 5 ) << errcount  << " TOTAL ERROR(S)" << endl;
			sstr << setw( 5 ) << warncount << " TOTAL WARNING(S)" << endl;

			ostr << endl << sstr.str();

			if ( !lstfile.empty() && cout != cerr )
			{
				cerr << sstr.str();
			}
		}

	} // pass


	out.close();

}
