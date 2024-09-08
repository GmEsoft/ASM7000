#pragma once

#include "Source_I.h"

#include <fstream>

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

