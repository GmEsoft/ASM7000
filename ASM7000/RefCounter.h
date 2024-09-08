#pragma once

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


