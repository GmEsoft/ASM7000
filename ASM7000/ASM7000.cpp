#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <stack>
#include <string>
#include <cstdarg>
#include <time.h>

#define MACRO 1

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


using namespace std;

typedef unsigned short word;
typedef unsigned char  byte;

word pc = 0;
int pass;

#define DEBUG_MSGS 0

#define CDBG if ( DEBUG_MSGS ) cerr

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

stack< Options > optstack;

Options options;


/////// UTILITIES /////////////////////////////////////////////////////////////

/** Reference counter for classes sharing common resources.
 *
 * Those classes must derive from this one.
 * Plus:
 * - The copy constructor must call this one in its initializers list;
 * - The destructor must call delRef() and release the shared resources
 *   if delRef() returned true;
 * - The assignment operator must call setRef() and release the old shared
 *   resources if setRef() returned true, before assigning the new shared
 *   resources from the other object.
 *
 */
class RefCounter
{
protected:
	RefCounter()
	: ref_( new size_t( 1 ) )
	{
	}

	// Called by copy constructor in derived classes.
	// Missing doing that will lead to problems !!
	RefCounter( const RefCounter &other )
	: ref_( other.ref_ )
	{
		addRef();
	}

	// Called by operator=() in derived classes;
	// if returning true, the old pointers
	// must be deleted.
	bool setRef( const RefCounter &other )
	{
		bool ret = delRef();
		ref_ = other.ref_;
		addRef();
		return ret;
	}

	virtual ~RefCounter()
	{
		// delRef() called in derived class !!
	}

	// Called by destructor in devived classes;
	// if returning true, the old pointers
	// must be deleted.
	bool delRef()
	{
		bool ret;
		if ( ret = ( ref_ && !--*ref_ ) )
		{
			delete ref_;
			ref_ = 0;
		}
		return ret;
	}
private:
	void addRef()
	{
		ref_ && ++*ref_;
	}

	size_t *ref_;
};



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

const char* argtypes[] =
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

struct Arg
{
	ArgType type;
	word	data;
	string	str;
	string	text;
};


#if MACRO
/////// MACROS ////////////////////////////////////////////////////////////////

class Macro : protected RefCounter
{
public:
	Macro() : data( 0 )
	{
		CDBG << "Macro()" << endl;
	}

	Macro( const string& p_name, const string& p_type, const string& p_args )
	{
		CDBG << "Macro(" << p_name << ", " << p_type << ", " << p_args << ")" << endl;
		data = new Data();
		data->name = p_name;
		data->type = p_type;
		data->args = p_args;
		data->level = 0;
		data->adding = 1;
		data->eof = 0;
		data->itText = data->text.begin();
		data->count = 0;
	}

	Macro ( const Macro& other )
	: RefCounter( other )
	, data( other.data )
	{
		CDBG << "Macro(other)" << endl;
	}

	Macro &operator=( const Macro &other )
	{
		CDBG << "Macro = other";
		if ( setRef( other ) )
		{
			CDBG << " delete";
			delete data;
		}
		data = other.data;
		CDBG << " OK" << endl;
		return *this;
	}

	~Macro()
	{
		CDBG << "~Macro()";
		if ( delRef() )
		{
			CDBG << " delete";
			delete data;
		}
		CDBG << " OK" << endl;
	}

	operator bool() const
	{
		return data && data->adding;
	}

	bool add( const string& p_op, const string& p_line )
	{
		if ( !data )
		{
			CDBG << "add(): !data" << endl;
			return false;
		}
		if ( p_op == "MACRO" || p_op == "REPT" || p_op == "IRP" || p_op == "IRPC" )
		{
			++data->level;
		}
		else if ( p_op == "ENDM" )
		{
			if ( !data->level )
				return data->adding = false;
			--data->level;
		}

		CDBG << "add(): push_back " << p_line << endl;

		data->text.push_back( p_line );
		data->itText = data->text.end();
		return true;
	}

	void rewind()
	{
		if ( !data )
		{
			CDBG << "rewind(): !data" << endl;
			return;
		}
		data->itText = data->text.begin();
	}

	bool eof() const
	{
		if ( !data )
		{
			CDBG << "eof(): !data" << endl;
			return true;
		}
		return data->eof;
	}

	string getline()
	{
		if ( !data )
		{
			CDBG << "getline(): !data" << endl;
			return "";
		}
		CDBG << "getline(): data" << ( ( data->itText == data->text.end() ) ? "" : *(data->itText) ) << endl;
		if ( data->itText == data->text.end() )
		{
			if ( data->count++ < data->rept )
				rewind();
		}

		if ( data->itText == data->text.end() )
		{
			data->eof = true;
			return "";
		}

		return *(data->itText++);
	}

	string gettype() const
	{
		if ( !data )
		{
			CDBG << "gettype(): !data" << endl;
			return "";
		}
		return data->type;
	}

	void rept( int rept )
	{
		if ( !data )
		{
			CDBG << "rept(): !data" << endl;
			return;
		}
		data->rept = rept;
	}

	string args()
	{
		if ( !data )
		{
			CDBG << "args(): !data" << endl;
			return "";
		}
		return data->args;
	}

private:
	struct Data
	{
		string name;
		string type;
		string args;

		vector< string > text;
		vector< string >::const_iterator itText;

		int level;
		bool adding;
		bool eof;
		int rept;
		int count;
	};

	Data *data;
};

#endif

/////// SOURCE ////////////////////////////////////////////////////////////////

class Source_I
{
public:
	virtual ~Source_I()
	{
	}

	virtual string getline() = 0;

	virtual bool operator!() = 0;

	operator bool()
	{
		return !!*this;
	}

	virtual void rewind() = 0;

	virtual size_t linenum() = 0;

	virtual string getname() = 0;

	virtual string gettype() = 0;
};


class FileSource : public Source_I
{
public:
	FileSource( const string &name )
	{
		pIn_ = new ifstream( name.data() );
		fail_ = !*pIn_;
		num_ = 0;
		name_ = name;
	}

	virtual ~FileSource()
	{
		delete pIn_;
	}

	virtual string getline()
	{
		fail_ = !pIn_->getline( buf_, sizeof buf_ );
		if ( fail_ )
			return "";
		++num_;
		return buf_;
	}

	virtual bool operator!()
	{
		return fail_;
	}

	virtual void rewind()
	{
		pIn_->clear();
		pIn_->seekg(0, ios::beg);
		num_ = 0;
	}

	virtual size_t linenum()
	{
		return num_;
	}

	virtual string getname()
	{
		return name_;
	}

	virtual string gettype()
	{
		return "FILE";
	}

private:
	string name_;
	char buf_[255];
	ifstream *pIn_;
	bool fail_;
	int num_;
};

#if MACRO
class MacroSource : public Source_I
{
public:
	MacroSource( const string &name, Macro &macro )
	: name_( name ), macro_( macro )
	{
		num_ = 0;
	}

	virtual ~MacroSource()
	{
	}

	virtual string getline()
	{
		CDBG << "macro getline()" << endl;
		return macro_.getline();
	}

	virtual bool operator!()
	{
		CDBG << "macro operator!()" << endl;
		return macro_.eof();
	}

	virtual void rewind()
	{
		CDBG << "macro rewind()" << endl;
		macro_.rewind();
		num_ = 0;
	}

	virtual size_t linenum()
	{
		CDBG << "macro linenum()" << endl;
		return num_;
	}

	virtual string getname()
	{
		CDBG << "macro getname()" << endl;
		return name_;
	}

	virtual string gettype()
	{
		CDBG << "macro gettype()" << endl;
		return macro_.gettype();
	}

private:
	string name_;
	Macro macro_;
	int num_;
};
#endif

class Source : public Source_I, protected RefCounter
{
public:
	Source()
	: source_( 0 )
	{
	}

	Source( const string &name )
	: source_( new FileSource( name ) )
	{
	}

#if MACRO
	Source( const string &name, Macro &macro )
	: source_( new MacroSource( name, macro ) )
	{
	}
#endif

	Source( const Source &other )
	: RefCounter( other )
	, source_( other.source_ )
	{
	}

	virtual ~Source()
	{
		if ( delRef() )
		{
			CDBG << "~Source(): delete " << source_->getname() << endl;
			delete source_;
		}
	}

	Source &operator=( const Source &other )
	{
		if ( setRef( other ) )
		{
			CDBG << "operator=(): delete " << source_->getname() << endl;
			delete source_;
		}

		source_ = other.source_;

		return *this;
	}

	virtual string getline()
	{
		return source_->getline();
	}

	virtual bool operator!()
	{
		return !*source_;
	}

	virtual void rewind()
	{
		source_->rewind();
	}

	virtual size_t linenum()
	{
		return source_->linenum();
	}

	virtual string getname()
	{
		return source_->getname();
	}

	virtual string gettype()
	{
		return source_->gettype();
	}

private:
	Source_I *source_;
};



#define enum_pair( A, B ) ( ( (A) << 4 ) | (B) )


/////// STRINGS ///////////////////////////////////////////////////////////////

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
vector<string> split( string str, const string &sep )
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

string touppernotquoted( const string& str )
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


/////// MESSAGES //////////////////////////////////////////////////////////////

class Log
{

	vector<string> errorsLog;
	vector<string> warningsLog;
	vector<string> infoLog;
	vector<string> debugLog;
	bool enabled_, debug_, warning_;
	char buf[256];

public:
	Log()
	: debug_( false ), warning_( true ), enabled_( true )
	{}

	void setEnabled( bool flag )
	{
		enabled_ = flag;
	}

	void setDebug( bool flag )
	{
		debug_ = flag;
	}

	void setWarning( bool flag )
	{
		warning_ = flag;
	}

	void error( const char* format, ... )
	{
		if ( enabled_ )
		{
			va_list args;
			va_start( args, format );
			vsprintf_s( buf, sizeof buf, format, args );
			va_end( args );
			errorsLog.push_back( buf );
		}
	}

	void warn( const char* format, ... )
	{
		if ( enabled_ && warning_ )
		{
			va_list args;
			va_start( args, format );
			vsprintf_s( buf, sizeof buf, format, args );
			va_end( args );
			warningsLog.push_back( buf );
		}
	}

	void info( const char* format, ... )
	{
		if ( enabled_ )
		{
			va_list args;
			va_start( args, format );
			vsprintf_s( buf, sizeof buf, format, args );
			va_end( args );
			infoLog.push_back( buf );
		}
	}

	void debug( const char* format, ... )
	{
		if ( enabled_ && debug_ )
		{
			va_list args;
			va_start( args, format );
			vsprintf_s( buf, sizeof buf, format, args );
			va_end( args );
			debugLog.push_back( buf );
		}
	}

	void writeTo( ostream &ostr)
	{
		for ( int i=0; i<errorsLog.size(); ++i )
			ostr << "*** Error: " << errorsLog[i] << endl;
		for ( int i=0; i<warningsLog.size(); ++i )
			ostr << "*** Warning: " << warningsLog[i] << endl;
		for ( int i=0; i<infoLog.size(); ++i )
			ostr << "*** " << infoLog[i] << endl;
		for ( int i=0; i<debugLog.size(); ++i )
			ostr << "*** Debug: " << debugLog[i] << endl;
	}

	bool isWarning()
	{
		return !warningsLog.empty() || !errorsLog.empty();
	}

	size_t getErrorsCount()
	{
		return errorsLog.size();
	}

	size_t getWarningsCount()
	{
		return warningsLog.size();
	}

	void clear()
	{
		errorsLog.clear();
		warningsLog.clear();
		infoLog.clear();
		debugLog.clear();
	}

};

extern Log log;


/////// SYMBOLS ///////////////////////////////////////////////////////////////

typedef map< string, Arg > 			symbols_t;
typedef symbols_t::iterator 		symbolptr_t;
typedef deque< symbols_t >			symstack_t;
typedef symstack_t::iterator 		symstackptr_t;

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
				params_.push_back( touppernotquoted( def[i] ) );
			}
			expr_ = touppernotquoted( def.back() );
			log.debug( "Function %s = %s", name_.data(), expr_.data() );
		}
	}

	string name_;
	vector< string > params_;
	string expr_;
};

typedef map< string, Function > FunctionSeq_t;
typedef FunctionSeq_t::const_iterator FunctionPtr_t;

FunctionSeq_t functions;


/////// ARGS //////////////////////////////////////////////////////////////////

class Parser
{
public:
	Parser( const string &p_expr )
	: expr( p_expr ), len( p_expr.size() ), p( 0 )
	{
	}

	void skipblk()
	{
		while ( p < len && ( expr[p] == ' ' || expr[p] == '\t' ) )
			++p;
	}

	string parsename()
	{
		string ret;
		skipblk();
		while ( p < len )
		{
			char c = expr[p];
			if ( c == '_' || c == '$' || isalnum( c ) )
			{
				++p;
				ret += c;
				continue;
			}
			break;
		}
		return ret;
	}

	word parsenum( int radix )
	{
		word bin = 0;
		word dec = 0;
		word hex = 0;

		skipblk();

		while ( p < len )
		{
			word dig = 0;
			char c = expr[p];
			if ( isdigit( c ) )
				dig = c - '0';
			else if ( c >= 'A' && c <= 'F' )
			{
				dig = c - 'A' + 10;
				if ( c == 'B' && radix == 0 )
					radix = 2;
			}
			else if ( c == 'H' )
			{
				radix = 16;
				++p;
				break;
			}
			else
			{
				break;
			}

			if ( c != 'B' )
				bin = ( bin << 1 ) + dig;
			dec = 10 * dec + dig;
			hex = ( hex << 4 ) + dig;

			++p;
		}

		switch ( radix )
		{
		case 2:
			return bin;
		case 10:
			return dec;
		case 16:
			return hex;
		default:
			return dec;
		}
	}

	Arg parsevalue()
	{
		Arg ret = { ARG_IMM, 0xFFFF, expr, "" };
		skipblk();
		if ( p < len )
		{
			char c = expr[p];
			if ( c == '>' )
			{
				++p;
				ret.data = parsenum( 16 );
			}
			else if ( isdigit( c ) )
			{
				ret.data = parsenum( 0 );
			}
			else if ( c == '(' )
			{
				++p;
				ret = parse();
				skipblk();
				if ( p < len && expr[p] == ')' )
					++p;
			}
			else if ( c == '"' || c == '\'' )
			{
				++p;
				bool esc = false, first = true;

				while ( p < len && ( esc || expr[p] != c ) )
				{
					char ch = expr[p++];

					if ( esc )
					{
						esc = false;
					}
					else if ( ch == '\\' )
					{
						esc = true;
						continue;
					}

					if ( first )
						first = false, ret.data = ch;

					ret.text += ch;
				}

				if ( p < len )
					++p;

				ret.type = ARG_TEXT;
				log.debug( "Text: [%s]", ret.text.data() );
			}
			else if ( c == '_' || c == '$' || isalpha( c ) )
			{
				string name = parsename();
				if ( name == "$" )
					ret.data = pc;
				else
				{
					Arg sym = getSymbol( name );

					if ( sym.type != ARG_UNDEF )
						ret = sym;
					else
					{
						FunctionPtr_t itFunc = functions.find( name );
						if ( itFunc != functions.end() )
						{
							beginSymbols();
							const Function &func = itFunc->second;
							skipblk();
							bool paren = !eof() && expr[p] == '(';

							if ( paren )
								++p;

							for ( int i=0; i<func.params_.size() ; ++i )
							{
								skipblk();
								if ( i>0 )
								{
									if ( eof() || expr[p] != ',' )
										break;
									++p;
								}
								Arg arg = parsevalue();
								addLocalSymbol( func.params_[i], arg );
								log.debug( "%s = (%s)%04X", func.params_[i].data(), argtypes[arg.type], arg.data );
							}

							skipblk();
							if ( paren )
							{
								if ( !eof() && expr[p] == ')' )
									++p;
							}

							Parser parser( func.expr_ );
							ret = parser.parse();
							log.debug( "%s = (%s)%04X", func.expr_.data(), argtypes[ret.type], ret.data );
							if ( !parser.eof() )
								log.error( "Error evaluating function %s: %s", name.data(), func.expr_.data() );

							endSymbols();

						}
						else
							log.error( "Symbol not found: [%s]", name.data() );
					}
				}
			}
			else
			{
				log.error( "Unsupported token: [%c]", c) ;
			}
		}
		return ret;
	}

	Arg parse()
	{
		Arg ret = parsevalue();
		skipblk();
		while ( p < len )
		{
			char c = expr[p];
			if ( c == '+' )
			{
				++p;
				Arg rhs = parsevalue();
				if ( ret.type == ARG_IMM && rhs.type == ARG_IMM )
					ret.data += rhs.data;
				else if ( ret.type == ARG_TEXT && rhs.type == ARG_IMM && ret.text.size() == 1 )
				{
					ret.type = rhs.type;
					ret.data += rhs.data;
				}
				else
					log.error( "%c: incompatible types: %s and %s",
						c, argtypes[ret.type], argtypes[rhs.type] );
			}
			else if ( c == '-' )
			{
				++p;
				Arg rhs = parsevalue();
				if ( ret.type == ARG_IMM && rhs.type == ARG_IMM )
					ret.data -= rhs.data;
				else if ( ret.type == ARG_TEXT && rhs.type == ARG_IMM && ret.text.size() == 1 )
				{
					ret.type = rhs.type;
					ret.data -= rhs.data;
				}
				else
					log.error( "%c: incompatible types: %s and %s",
						c, argtypes[ret.type], argtypes[rhs.type] );
			}
			else if ( c == '&' )
			{
				++p;
				Arg rhs = parsevalue();
				if ( ret.type == ARG_IMM && rhs.type == ARG_IMM )
					ret.data &= rhs.data;
				else if ( ret.type == ARG_TEXT && rhs.type == ARG_IMM && ret.text.size() == 1 )
				{
					ret.type = rhs.type;
					ret.data &= rhs.data;
				}
				else
					log.error( "%c: incompatible types: %s and %s",
						c, argtypes[ret.type], argtypes[rhs.type] );
			}
			else if ( c == '|' )
			{
				++p;
				Arg rhs = parsevalue();
				if ( ret.type == ARG_IMM && rhs.type == ARG_IMM )
					ret.data |= rhs.data;
				else if ( ret.type == ARG_TEXT && rhs.type == ARG_IMM && ret.text.size() == 1 )
				{
					ret.type = rhs.type;
					ret.data |= rhs.data;
				}
				else
					log.error( "%c: incompatible types: %s and %s",
						c, argtypes[ret.type], argtypes[rhs.type] );
			}
			else if ( expr.substr( p, 2 ) == "<<" )
			{
				p += 2;
				Arg rhs = parsevalue();
				if ( ret.type == ARG_IMM && rhs.type == ARG_IMM )
					ret.data <<= rhs.data;
				else if ( ret.type == ARG_TEXT && rhs.type == ARG_IMM && ret.text.size() == 1 )
				{
					ret.type = rhs.type;
					ret.data <<= rhs.data;
				}
				else
					log.error( "%c: incompatible types: %s and %s",
						c, argtypes[ret.type], argtypes[rhs.type] );
			}
			else if ( expr.substr( p, 2 ) == ">>" )
			{
				p += 2;
				Arg rhs = parsevalue();
				if ( ret.type == ARG_IMM && rhs.type == ARG_IMM )
					ret.data >>= rhs.data;
				else if ( ret.type == ARG_TEXT && rhs.type == ARG_IMM && ret.text.size() == 1 )
				{
					ret.type = rhs.type;
					ret.data >>= rhs.data;
				}
				else
					log.error( "%c: incompatible types: %s and %s",
						c, argtypes[ret.type], argtypes[rhs.type] );
			}
			else if ( expr.substr( p, 3 ) == "DUP" )
			{
				p += 3;
				Arg rhs = parsevalue();
				if ( ret.type == ARG_IMM && rhs.type == ARG_IMM )
				{
					ret.text = string( ret.data, rhs.data );
					ret.type = ARG_DUP;
				}
				else
					log.error( "%c: incompatible types: %s and %s",
						c, argtypes[ret.type], argtypes[rhs.type] );
			}
			else if ( c == ')' )
			{
				break;
			}
			else
			{
				log.error( "[%c]: unsupported operator in [%s]", c, expr.data() ) ;
				break;
			}

			skipblk();
		}
		return ret;
	}

	bool eof()
	{
		return p >= len;
	}

	static Arg parse( const string &arg )
	{
		Arg ret = { ARG_NONE, 0xFFFF, arg, "" };

		size_t size = arg.size();

		if ( size > 0 )
		{
			Parser parser( arg );
			ret = parser.parse();
			if ( !parser.eof() )
				log.error( "Parse error: %s", arg.data() );
		}
		else
		{
			log.error( "Missing literal", arg.data() );
		}
		return ret;
	}

	static Arg getarg( const string &arg )
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
			if ( size > 3 && arg.find( "(B)" ) == arg.size() - 3 )
			{
				ret.type = ARG_EFFEC;
				ret.data = parse( arg.substr( 1, arg.size() - 4 ) ).data;
			}
			else
			{
				ret.type = ARG_IMM;
				ret.data = parse( arg.substr( 1 ) ).data;
			}
		}
		else if ( size > 2 && arg[0] == '*' )
		{
			ret.type = ARG_INDIR;
			ret.data = parse( arg.substr( 1 ) ).data;
		}
		else if ( arg[0] == '@' )
		{
			if ( arg.find( "(B)" ) == arg.size() - 3 )
			{
				ret.type = ARG_INDEX;
				ret.data = parse( arg.substr( 1, arg.size() - 4 ) ).data;
			}
			else
			{
				ret.type = ARG_DIR;
				ret.data = parse( arg.substr( 1 ) ).data;
			}
		}
		else
		{
			ret = parse( arg );
		}
		return ret;
	}

private:
	const string &expr;
	const size_t len;
	size_t p;
};




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
		log.error( "Expecting immediate: [%s] (%s)", arg.str.data(), argtypes[arg.type] );
	return arg.data;
}

word getbyte( const Arg &arg )
{
	if ( arg.type != ARG_IMM && arg.type != ARG_REG )
		log.error( "Bad byte type: [%s]=%04X (%s)", arg.str.data(), arg.data, argtypes[arg.type] );
	else if ( short( arg.data ) < -128 || arg.data > 255 )
		log.error( "Byte range error: [%s]=%04X (%s)", arg.str.data(), arg.data, argtypes[arg.type] );
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
		log.error( "Number range error: [%s]=%d (%s)", arg.str.data(), arg.data, argtypes[arg.type] );
	return ret & 0xFF;
}

word getoffset( word addr, const Arg &arg )
{
	short offset = arg.data - addr;
	if ( offset < -128 || offset > 127 )
	{
		log.error( "Offset range error: [%s] (%s)", arg.str.data(), argtypes[arg.type] );
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
				op.data(), args[0].str.data(), args[1].str.data(), argtypes[args[0].type], argtypes[args[1].type] );
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
				op.data(), args[0].str.data(), args[1].str.data(), argtypes[args[0].type], argtypes[args[1].type] );
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
				op.data(), args[0].str.data(), args[1].str.data(), argtypes[args[0].type], argtypes[args[1].type] );
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
				log.warn( "Got type %s, assuming DIR: %s=%04X", argtypes[args[0].type], args[0].str.data(), args[0].data );
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
			log.error( "Bad arg: %s [%s] (%s)", op.data(), args[0].str.data(), argtypes[args[0].type] );
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
			log.error( "Bad arg: %s [%s] (%s)", op.data(), args[0].str.data(), argtypes[args[0].type] );
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
				op.data(), args[0].str.data(), args[1].str.data(), argtypes[args[0].type], argtypes[args[1].type] );
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
				op.data(), args[0].str.data(), args[1].str.data(), argtypes[args[0].type], argtypes[args[1].type] );
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
			log.error( "Bad arg: %s [%s] (%s)", op.data(), args[0].str.data(), argtypes[args[0].type] );
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
			log.error( "Bad arg: %s [%s] (%s)", op.data(), args[0].str.data(), argtypes[args[0].type] );
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
				log.error( "Bad arg: %s [%s] (%s)", op.data(), args[0].str.data(), argtypes[args[0].type] );
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

		ostr  << "\t" << timeStr() << endl << endl;
		if ( !outfile.empty() )
			ostr  << "\tAssembly of  : " << infile << endl;
		ostr  << "\tOutput file  : " << outfile << endl;
		if ( !lstfile.empty() )
			ostr  << "\tListing file : " << lstfile << endl;

		ostr  << endl;
	}

	beginSymbols();

	for ( int i=0; i<0x100; ++i )
	{
		stringstream sstr;
		sstr << "R" << i;
		string label = sstr.str();
		Arg arg = { ARG_REG, i, label, "" };
		addSymbol( label, arg );
	}

	for ( int i=0; i<0x100; ++i )
	{
		stringstream sstr;
		sstr << "P" << i;
		string label = sstr.str();
		Arg arg = { ARG_PORT, i + 0x100, label, "" };
		addSymbol( label, arg );
	}

	Arg argDate = { ARG_TEXT, 0, "DATE", "DD-MM-YYYY" };
	addSymbol( "DATE", argDate );

	Arg argTime = { ARG_TEXT, 0, "DATE", "HH:MM:SS" };
	addSymbol( "TIME", argTime );


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
					endSymbols();
				}
				else
				{
					log.warn( "No END statement" );
					end = true;
				}
			}

			int num = in.linenum();

			vector< string > tokens = split( line, "\t :" );

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
						label = touppernotquoted( token );
						if ( !label.empty() && label[label.size()-1] == ':' )
							label = label.substr( 0, label.size()-1 );
						break;
					case 1:
						op = touppernotquoted( token );
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

			vector< string > argstrs = split( argstr, "," );
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
						Arg arg = Parser::getarg( touppernotquoted( macro.args() ) );
						macro.rept( arg.data );
						sources.push( in );
						in = Source( "REPT", macro );
						beginSymbols();
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
						Arg arg = Parser::getarg( touppernotquoted( argstrs[0] ) );
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
							beginSymbols();
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
						string arg = touppernotquoted( argstrs[0] );
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
						string arg = touppernotquoted( argstrs[0] );
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
						args[i] = Parser::getarg( touppernotquoted( argstrs[i] ) );
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
									//log.error( "debug: set [%s] (%s=%04x)", args[0].str.data(), argtypes[type], addr );
									break;
								default:
									log.error( "Bad addressing mode: [%s] (%s)", args[0].str.data(), argtypes[args[0].type] );
								}
							}
						}

						Arg sym = getSymbol( label );
						if ( pass == 2 )
						{
							if ( sym.type != ARG_UNDEF && sym.data != addr )
								log.error ( "Multiple definition: [%s] (%s=%04X)",
									label.data(), argtypes[sym.type], sym.data );
						}
						Arg arg = { type, addr, label, "" };
						addSymbol( label, arg );
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
								log.error( "Bad arg type: %s", argtypes[arg.type] );
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
								log.error( "Bad arg type: %s", argtypes[arg.type] );
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
								log.error( "Bad arg type: %s", argtypes[arg.type] );
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
								log.info( "with %s as %s", arg0.str.data(), argtypes[arg0.type] );
								log.info( " and %s as %s", arg1.str.data(), argtypes[arg1.type] );
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
