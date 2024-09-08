#pragma once

#include "Symbols.h"
#include "Function.h"
#include "Log.h"

extern word pc;


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
					Arg sym = symbols.getSymbol( name );

					if ( sym.type != ARG_UNDEF )
						ret = sym;
					else
					{
						FunctionPtr_t itFunc = functions.find( name );
						if ( itFunc != functions.end() )
						{
							symbols.beginSymbols();
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
								symbols.addLocalSymbol( func.params_[i], arg );
								log.debug( "%s = (%s)%04X", func.params_[i].data(), ArgTypes::get(arg.type), arg.data );
							}

							skipblk();
							if ( paren )
							{
								if ( !eof() && expr[p] == ')' )
									++p;
							}

							Parser parser( func.expr_ );
							ret = parser.parse();
							log.debug( "%s = (%s)%04X", func.expr_.data(), ArgTypes::get(ret.type), ret.data );
							if ( !parser.eof() )
								log.error( "Error evaluating function %s: %s", name.data(), func.expr_.data() );

							symbols.endSymbols();

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
						c, ArgTypes::get(ret.type), ArgTypes::get(rhs.type) );
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
						c, ArgTypes::get(ret.type), ArgTypes::get(rhs.type) );
			}
			else if ( c == '*' )
			{
				++p;
				Arg rhs = parsevalue();
				if ( ret.type == ARG_IMM && rhs.type == ARG_IMM )
					ret.data *= rhs.data;
				else if ( ret.type == ARG_TEXT && rhs.type == ARG_IMM && ret.text.size() == 1 )
				{
					ret.type = rhs.type;
					ret.data *= rhs.data;
				}
				else
					log.error( "%c: incompatible types: %s and %s",
						c, ArgTypes::get(ret.type), ArgTypes::get(rhs.type) );
			}
			else if ( c == '/' )
			{
				++p;
				Arg rhs = parsevalue();
				if ( ret.type == ARG_IMM && rhs.type == ARG_IMM )
					ret.data /= rhs.data;
				else if ( ret.type == ARG_TEXT && rhs.type == ARG_IMM && ret.text.size() == 1 )
				{
					ret.type = rhs.type;
					ret.data /= rhs.data;
				}
				else
					log.error( "%c: incompatible types: %s and %s",
						c, ArgTypes::get(ret.type), ArgTypes::get(rhs.type) );
			}
			else if ( c == '%' )
			{
				++p;
				Arg rhs = parsevalue();
				if ( ret.type == ARG_IMM && rhs.type == ARG_IMM )
					ret.data %= rhs.data;
				else if ( ret.type == ARG_TEXT && rhs.type == ARG_IMM && ret.text.size() == 1 )
				{
					ret.type = rhs.type;
					ret.data %= rhs.data;
				}
				else
					log.error( "%c: incompatible types: %s and %s",
						c, ArgTypes::get(ret.type), ArgTypes::get(rhs.type) );
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
						c, ArgTypes::get(ret.type), ArgTypes::get(rhs.type) );
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
						c, ArgTypes::get(ret.type), ArgTypes::get(rhs.type) );
			}
			else if ( c == '^' )
			{
				++p;
				Arg rhs = parsevalue();
				if ( ret.type == ARG_IMM && rhs.type == ARG_IMM )
					ret.data ^= rhs.data;
				else if ( ret.type == ARG_TEXT && rhs.type == ARG_IMM && ret.text.size() == 1 )
				{
					ret.type = rhs.type;
					ret.data ^= rhs.data;
				}
				else
					log.error( "%c: incompatible types: %s and %s",
						c, ArgTypes::get(ret.type), ArgTypes::get(rhs.type) );
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
						c, ArgTypes::get(ret.type), ArgTypes::get(rhs.type) );
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
						c, ArgTypes::get(ret.type), ArgTypes::get(rhs.type) );
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
						c, ArgTypes::get(ret.type), ArgTypes::get(rhs.type) );
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

		if ( size )
		{
			Parser parser( arg );
			ret = parser.parse();
			if ( !parser.eof() )
				log.error( "Parse error: %s", arg.data() );
		}
		else
		{
			log.error( "Missing literal" );
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



