#pragma once

#include <string>

using namespace std;

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


