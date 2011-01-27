#ifndef PTR_HXX
#define PTR_HXX

#define NEXCEPT 1

// Common templates

//----------------------------------------------------------------------------
//
// Smart pointer (with reference counts)
//
// The thing that its pointing to must be derived from Pted
// This class is MAD safe.

template<class T> class Ptr
{
	T * p;

	// Safe deref - zeros p to avoid circular deletions

	inline void deref ();

protected:
	// These functions included for use in sub-classes where 'implicit'
	// assignment fails

	const Ptr& PtrSet (T * const in);
	const Ptr& PtrSet (const Ptr &in);

	inline T * PtrGet () const
	{
		return p;
	}
public:
	// Constructors - these must not try and dereference what they were
	// pointing to before 'cos they weren't!

	Ptr () :
		p (0)
	{
		// Empty
	}

	Ptr (T * const in);
	Ptr (const Ptr &in);
	
	// Destructor

	~Ptr ()
	{
		deref ();
	}
	
	// Functions - mostly trivial

	inline bool isnull () const
	{
		return p == 0;
	}

	// MAD code may require this function

	void ref_zapped (T * const x)
	{
		if (x == p)
			p = 0;
	}

	// Is required 'cos Ptr<thing>=0 has very tedious type conversions

	inline void null ()
	{
		deref ();
	}
	inline const Ptr& operator= (const Ptr in)
	{
		return PtrSet (in);
	}
	inline const Ptr& operator= (T * const in)
	{
		return PtrSet (in);
	}
	inline bool operator== (const T * const x) const
	{
		return x == p;
	}
	inline bool operator!= (const T * const x) const
	{
		return x != p;
	}
	inline operator T* () const
	{
		return p;
	}
	inline T* operator-> () const
	{
		return p;
	}
	inline T& operator* () const
	{
		return *p;
	}
};

template <class T>
void
Ptr<T>::deref ()
{
	if (p != 0)
	{
		T * const p2 = p;
		p = 0;
		p2->DecReferenceCount ();
	}
}

template <class T>
const Ptr<T>&
Ptr<T>::PtrSet (T * const in)
{
	if (in != 0)
		in->IncReferenceCount ();
	deref ();
	p = in;
	return *this;
}

template <class T>
const Ptr<T>&
Ptr<T>::PtrSet (const Ptr<T>& in)
{
	if (!in.isnull ())
		in->IncReferenceCount ();
	deref ();
	p = in.p;
	return *this;
}

template <class T>
Ptr<T>::Ptr (T * const in) :
	p (in)
{
	if (in != 0)
		in->IncReferenceCount ();
}

template <class T>
Ptr<T>::Ptr (const Ptr<T>& in) :
	p (in.p)
{
	if (!in.isnull ())
		in->IncReferenceCount ();
}
	
//---------------------------------- Pted ------------------------------------
//
// The base class for thisngs that are pointed to be Ptr<T> and PtrList<T>
// classes.  Maintains ref count
//
// Should probably be declared as a virtual base class whenever used...

class Pted
{
	int count;

	Pted& operator= (Pted&);  // In general Pted objects shouldn't be copied
	Pted (const Pted&);
public:
	Pted () : count (0)
	{
		// Empty
	}

	// Must have a virtual destructor if we are going to commit suicide
	// correctly

	virtual ~Pted ()
	{
#ifndef NEXCEPT
		if (count > 0)
			throw Error ("Pted: Object deleted with positive ref count");
#endif
	}

	// Obvious really

	void IncReferenceCount ()
	{
		++count;
	}
	
	// If we are no longer loved commit suicide
	// The zero check for delete means we only die once if MAD

	bool DecReferenceCount ()
	{
		if (--count > 0)
			return false;

		if (count == 0)
			delete this;

		return true;
	}

	// A couple of useful? tests

	bool IsUnreferenced () const
	{
		return count == 0;
	}
	bool IsUniqueReference () const
	{
		return count == 1;
	}
};


//-------------------------------- Ptr1<T> -----------------------------------
//
// Convienient holder for things.
// Ensures deletion on lack of reference.
// Unline Ptr - it assumes there is just one pointer
//
// Mainly used for slightly late initialization of things that would have been
// inited as part of the class member init phase except that they required
// 'this' to have been defined
//
// Is MAD safe

template<class T>
class Ptr1
{
	T * p;

	Ptr1 (const Ptr1 &in);  // Copy not allowed
	Ptr1& operator= (const Ptr1 in);

public:
	// We zero p before deleteing it so we can write MAD code safely

	void null ();

protected:
	inline T * PtrSet (T * const in)
	{
		null ();
		return p = in;
	}
	inline T * PtrGet () const
	{
		return p;
	}

public:
	// Constructors - these must not try and dereference what they were
	// pointing to before 'cos they weren't!

	Ptr1 (T * const in = 0) :
		p (in)
	{
		// Empty
	}
	
	// Destructor - zero p in case of MAD

	~Ptr1 ()
	{
		null ();
	}

	// Zero the pointer without deallocation
	// Required by MAD destructors

	inline void dead ()
	{
		p = 0;
	}
	
	// Functions - mostly trivial

	inline bool isnull () const
	{
		return p == 0;
	}


	inline T * operator= (T * const in)
	{
		null ();
		return p = in;
	}
	inline bool operator == (const T * const x) const
	{
		return p == x;
	}

	inline operator T* () const
	{
		return p;
	}
	inline T* operator-> () const
	{
		return p;
	}
	inline T& operator* () const
	{
		return *p;
	}
};


template <class T>
void
Ptr1<T>::null ()
{
	if (p != 0)
	{
		T * const oldp = p;
		p = 0;
		delete oldp;
	}
}

//-------------------------------- PList<T> ----------------------------------
//
// Convienient 'list of dumb pointers' code
// Allows easy adding of new stuff.
// Doesn't delete refs on deleteion of List

template<class T>
class PList
{
protected:
	T** array;
	size_t allocated, alen;

	void newalloc ()
	{
		if (alen >= allocated)
		{
			if (allocated == 0)
			{
				allocated = 4;
				array = (T**)malloc (sizeof (T*) * allocated);
//				if ((array = (T**)malloc (sizeof (T*) * allocated)) == 0)
//					_standard_new_handler (sizeof (T*) * allocated);
			}
			else
			{
				if (allocated < 64)
					allocated *= 2;
				else
					allocated += 64;

				array = (T**)realloc (array, sizeof (T*) * allocated);
					/* panic */;
//					_standard_new_handler (sizeof (T*) * allocated);
			}
		}
	}

public:
	PList () :
		array (0),
		allocated (0),
		alen (0)
	{
		// Empty
	}

	~PList ()
	{
		if (allocated != 0)
			free (array);
	}

	PList& operator << (T * const x)
	{
		newalloc ();
		array [alen++] = x;
		return *this;
	}

	T* operator [] (size_t offset) const
	{
#ifndef NEXCEPT
		if (offset >= alen)
			throw Error ("PList::[] offset >= alen");
#endif
		return array [offset];		
	}	

	T* operator [] (int offset) const
	{
#ifndef NEXCEPT
		if (offset < 0)
			throw Error ("PList::[] offset < 0");

		if (size_t (offset) >= alen)
			throw Error ("PList::[] offset >= alen");
#endif
		return array [offset];		
	}

	size_t len () const
	{
		return alen;
	}

	// Insert the given value into a NULL entry or at the end if none

	size_t insert (T * const x)
	{
		// *** If we use this a lot it could be MUCH smarter

		for (size_t i = 0; i < alen; ++i)
			if (array [i] == 0)
			{
				array [i] = x;
				return i;
			}

		newalloc ();
		array [alen] = x;
		return alen++;
	}

	int find (const T * const x) const
	{
		for (size_t i = 0; i < alen; ++i)
			if (array [i] == x)
				return i;
		return -1;
	}


	// Functions to simply forget & functions to delete are separated
	// in this version of reality.
	//
	// In particular deleteing the list doesn't attempt to delete its
	// members.

	void empty ()
	{
		alen = 0;
	}

	T * set (const size_t offset, T * const x)
	{
#ifndef NEXCEPT
		if (offset >= alen)
			throw Error ("PList::set offset >= alen");
#endif
		return array [offset] = x;
	}

	void empty (const int offset)
	{
		if (offset > 0)
			set ((size_t)offset, 0);
	}

	T * replace (const size_t offset, T * const x);
	void deletel (const size_t offset);
	void deleteall ();
};


template<class T>
T *
PList<T>::replace (const size_t offset, T * const x)
{
#ifndef NEXCEPT
	if (offset >= alen)
		throw Error ("PList::replace offset >= alen");
#endif
	if (array [offset] != 0)
		delete array [offset];
	return array [offset] = x;
}

template<class T>
void
PList<T>::deletel (const size_t offset)
{
	if (offset < alen && array [offset] != 0)
	{
		delete array [offset];
		array [offset] = 0;
	}
}

template<class T>
void
PList<T>::deleteall ()
{
	while (alen != 0)
		if (array [--alen] != 0)
			delete array [alen];
}




//------------------------------- PtrList<T> ---------------------------------
//
// List of smart pointers
// Does deref on deleteion / empty

template<class T>
class PtrList : private PList<T>
{
public:
	// Export some stuff
	PList<T>::operator [];
	PList<T>::len;

	// The enstuffing operators

	PtrList& operator << (T * const x)
	{
		PList<T>::operator << (x);
		x->IncReferenceCount ();
		return *this;
	}

	PtrList& operator << (const Ptr<T> x)
	{
		PList<T>::operator << ((T *)x);
		x->IncReferenceCount ();
		return *this;
	}

	// Insert / Remove can be used if we don't care about ordering

	size_t insert (T * const x)
	{
		const size_t i = PList<T>::insert (x);
		x->IncReferenceCount ();
		return i;
	}

	PtrList& remove (const size_t i)
	{
		T * const p = array [i];
		PList<T>::set (i, 0);
		p->DecReferenceCount ();
		return *this;
	}


	// Delete & empty operations are combined here as they make much
	// more sense & can be done safely
	//
	// List objects are 'deleted' when the list is deleted

	void empty ()
	{
		while (alen != 0)
			array [--alen]->DecReferenceCount ();
	}

	~PtrList ()
	{
		empty ();
	}
};


#endif