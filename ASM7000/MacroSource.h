#pragma once

#include "Source_I.h"
#include "Macro.h"

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

