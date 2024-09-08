#pragma once

#include "FileSource.h"
#include "MacroSource.h"
#include "RefCounter.h"
#include "Debug.h"


class Source : public Source_I, private RefCounter
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

