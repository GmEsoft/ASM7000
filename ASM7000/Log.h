#pragma once

#include <string>
#include <vector>
#include <cstdarg>

using namespace std;

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


