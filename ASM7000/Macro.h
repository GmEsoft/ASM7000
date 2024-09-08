#pragma once

#define MACRO 1

#include "RefCounter.h"
#include "Debug.h"

#include <string>
#include <vector>
#include <map>

using namespace std;


#if MACRO
/////// MACROS ////////////////////////////////////////////////////////////////

class Macro : private RefCounter
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

