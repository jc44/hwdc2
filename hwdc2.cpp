#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "ptr.hpp"

using namespace std;

static wstring
itowstring(const int i, const int radix = 10)
{
	wchar_t buf[32];
	_itow(i, buf, radix);
	return wstring(buf);
}

class hwdc_error
{
	wstring err_text;

public:
	virtual ~hwdc_error()
	{
		// Empty
	}

	hwdc_error(int line_no, const wstring& text) :
		err_text(itowstring(line_no))
	{
		err_text += L": ";
		err_text += text;
	}

	operator wstring() const
	{
		return err_text;
	}
};

class pp_stream
{
	bool ungot;
	int last_c;
	int line_no;
	wifstream stream_in;

public:
	virtual ~pp_stream()
	{
		// Empty
	}

	pp_stream(const wchar_t * const filename) :
		ungot(false),
		last_c(-1),
		line_no(1),
		stream_in(filename)
	{
		// Empty
		wcout << L"Opening '" << filename << L"': open is " << stream_in.is_open() << L"\n";
	}

	int get(const bool quoted = false)
	{
		int c;

		if (ungot)
		{
			ungot = false;
			c = last_c;
		}
		else
		{
			c = stream_in.get();
		}

		// Dump comments
		if (!quoted && c == '/' && stream_in.peek() == '/')
		{
			while ((c = stream_in.get()) != WEOF && c != '\n')
				/* loop */;
		}

		if (c == '\n')
			++line_no;

		last_c = c;
		return c;
	}

	void unget()
	{
		ungot = true;
		if (last_c == '\n')
			--line_no;
	}

	int line() const
	{
		return line_no;
	}

	void skip_ws()
	{
		int c;
		while (iswspace(c = get()))
		{
			// Loop
		}
		unget();
	}
};

class thing_sequence;

class thing : public virtual Pted
{
public:
	enum eType
	{
		empty,
		eof,
		unquoted_str,
		quoted_str,
		number,
		section_start,
		section_end,
		square_bracket_start,
		square_bracket_end,
		assign,
		comma,
		op
	};

private:
	enum eType thing_type;
	int line_no;
	wstring strval;
	long numval;
	Ptr<thing_sequence> section;
	Ptr<thing> parent;

	static inline bool isidentifier(const int c)
	{
		return isalnum(c) || c == '_';
	}

public:
	virtual ~thing()
	{
		// Empty
	}

	thing() :
		thing_type(empty),
		numval(0)
	{
		// Empty
	}

	pp_stream& operator<< (pp_stream& in)
	{
		numval = 0;

		in.skip_ws();

		// Set the line_no of this thing after skipping whitespace which may
		// well contain newlines
		line_no = in.line();

		int c = in.get();
		if (c == WEOF)
		{
			thing_type = eof;
		}
		else if (c == '"')
		{
			thing_type = quoted_str;
			while ((c = in.get()) != '"')
			{
				// ******** throw error on EOF or '\n'
				strval += (wchar_t)c;
			}
		}
		else if (isidentifier(c))
		{
			thing_type = unquoted_str;

			// Accumulate string
			for (; isidentifier(c); c = in.get())
			{
				strval += (wchar_t)c;
			}
			in.unget();

			// Test if this is a valid number and if so then mark as such
			// At the moment we use strtol for conversion but this might want
			// to be changed if we want binary numbers too.
			{
				wchar_t * endptr;
				const wchar_t * const cstr = strval.c_str();
				numval = wcstol(cstr, &endptr, 0);
				// Its a good number if strtol consumes the entire string
				if (endptr - cstr == strval.length())
				{
					thing_type = number;
				}
			}
		}
		else if (c == '{')
		{
			thing_type = section_start;
		}
		else if (c == '}')
		{
			thing_type = section_end;
		}
		else if (c == '[')
		{
			thing_type = square_bracket_start;
		}
		else if (c == ']')
		{
			thing_type = square_bracket_end;
		}
		else if (c == '=')
		{
			thing_type = assign;
		}
		else if (c == ',')
		{
			thing_type = comma;
		}
		else
		{
			thing_type = op;
			strval = (wchar_t)c;
		}
		return in;
	}

	thing(pp_stream& in) :
		thing_type(empty),
		numval(0)
	{
		*this << in;
	}

	bool isro() const
	{
		return !strval.empty() && strval[0] == '_';
	}

	bool iseof() const
	{
		return thing_type == eof;
	}

	eType el_type() const
	{
		return thing_type;
	}

	const wstring& el_string() const
	{
		return strval;
	}

	const int el_number() const
	{
		return numval;
	}

	int el_line_no() const
	{
		return line_no;
	}

	thing_sequence& el_sequence() const
	{
		return *section;
	}

	const wstring el_name(int offset = 0, int depth = 0) const
	{
		return offset > 0 ? (parent.isnull() ? wstring() : parent->el_name(offset - 1, depth)) :
			parent.isnull() || depth == 1 ? el_string().substr(isro() ? 1 : 0) :
			parent->el_name(0, depth - 1) + L"_" + el_string().substr(isro() ? 1 : 0);
	}

	const wstring el_argname() const
	{
		const size_t wlen = strval.length();
		wstring x;
		x.reserve(wlen + 1);
		x += L'_';
		for (size_t i = 0; i != wlen; ++i)
		{
			x += towlower(strval[i]);
		}
		return x;
	}


	void set_sequence(thing_sequence * const seq);

	void set_parent(thing * new_parent)
	{
		parent = new_parent;
	}
};

class syntax_error : public hwdc_error
{
public:
	virtual ~syntax_error()
	{
		// Empty
	}

	syntax_error(const thing& bad_thing) :
		hwdc_error(bad_thing.el_line_no(), wstring(L"Syntax error near thing: ") + bad_thing.el_string())
	{
		// Empty
	}
};

class thing_sequence: public virtual Pted, public PtrList<thing>
{
public:
	virtual ~thing_sequence()
	{
		// Empty
	}

	thing_sequence()
	{
		// Empty
	}

	thing_sequence(pp_stream& in, const thing::eType expected_end = thing::eof);
	virtual void generate_c(wostream& os, thing * parent = NULL, const int argno = 0);

	thing& extract(size_t i)
	{
		if (i >= len())
		{
			throw syntax_error(*(*this)[i - 1]);
		}
		return *(*this)[i];
	}

	virtual wstring sb_val() const
	{
		return wstring();
	}
};

class square_bracket_sequence : public thing_sequence
{
	int val_shift;
	int field_shift;
	int mask;

public:
	virtual ~square_bracket_sequence()
	{
		// Empty
	}
	square_bracket_sequence(pp_stream& in) :
		thing_sequence(in, thing::square_bracket_end),
		val_shift(0),
		field_shift(0),
		mask(0)
	{
		// Empty
	}

	virtual void generate_c(wostream& os, thing * parent = NULL, const int argno = 0);

	virtual wstring sb_val() const
	{
		return wstring (L"(((x) >> ") +
			itowstring(val_shift) + wstring(L") & 0x") + itowstring(mask, 16) + wstring(L")");
	}
};

class section_sequence : public thing_sequence
{
public:
	virtual ~section_sequence()
	{
		// Empty
	}
	section_sequence(pp_stream& in) :
		thing_sequence(in, thing::section_end)
	{
		// Empty
	}
};


thing_sequence::thing_sequence(pp_stream& in, const thing::eType expected_end)
{
	for (;;)
	{
		Ptr<thing> t = new thing(in);
		thing::eType t_type = t->el_type();

		switch (t_type)
		{
			case thing::square_bracket_start:
			{
				t->set_sequence(new square_bracket_sequence(in));
				*this << t;
				break;
			}
			case thing::section_start:
			{
				t->set_sequence(new section_sequence(in));
				*this << t;
				break;
			}

			case thing::eof:
			case thing::section_end:
			case thing::square_bracket_end:
				if (t_type != expected_end)
				{
					wcout << L"**** bad brackets ***\n";
					throw syntax_error(*t);
				}
				return;

			default:
				*this << t;
				break;
		}
	}
}


void thing_sequence::generate_c(wostream& os, thing * parent, const int argno)
{
	size_t i = 0;
	int sb_count = 0;
	bool seen_default = false;
	bool all_ro = true;
	PtrList<thing> bthings;

	while (i < len())
	{
		thing& name = *(*this)[i++];
		name.set_parent(parent);

		// Start with unquoted-string
		// Numbers are valid unless prefix is empty
		if (!(name.el_type() == thing::unquoted_str ||
			(name.el_type() == thing::number && parent != NULL)))
		{
			throw syntax_error(name);
		}

		// Various things valid next
		thing& el = extract(i++);

		switch (el.el_type())
		{
			case thing::assign:
			{
				thing& el2 = extract(i++);

				// All sorts of things possible
				os << L"#define " << name.el_name() << L" " << el2.el_string() << L"\n";
				if (argno > 0)
				{
					os << L"#define _" << name.el_name(2) << L"_arg" << argno <<
						L"_" << name.el_name(0, 2) <<
						L" " << el2.el_string() << L"\n";
				}

				if (name.el_string() == L"DEFAULT")
					seen_default = true;

				break;
			}

			case thing::square_bracket_start:
			{
				++sb_count;

				el.el_sequence().generate_c(os, &name, sb_count);

				// We expect section start next
				thing& el2 = extract(i++);
				if (el2.el_type() != thing::section_start)
					throw syntax_error(el2);

				el2.el_sequence().generate_c(os, &name, sb_count);
				os << L"\n";

				bthings << &name;
				if (!name.isro())
					all_ro = false;

				break;
			}

			case thing::section_start:
			{
				el.el_sequence().generate_c(os, &name);
				break;
			}

			default:
				throw syntax_error(el);
		}
	}

	if (argno > 0)
	{
		if (!seen_default)
		{
			os << "#define " << parent->el_name() << L"_DEFAULT 0\n";
		}
	}

	if (bthings.len() != 0)
	{
		const size_t blen = bthings.len();

		if (!all_ro)
		{
			bool arg1 = true;
	
			os << L"#define " << parent->el_name() << "_RMK(";
			for (size_t i = 0; i != blen; ++i)
			{
				if (!arg1)
					os << L',';
				if (!bthings[i]->isro())
				{
					arg1 = false;
					os << bthings[i]->el_argname();
				}
			}
			os << L") (\\\n\t((";
			for (size_t i = 0; i != blen; ++i)
			{
				if (i != 0)
					os << L" | \\\n\t((";
				if (bthings[i]->isro())
					os << bthings[i]->el_name() << "_DEFAULT";
				else
					os << bthings[i]->el_argname();
	
				os << L") << _" << bthings[i]->el_name() << "_SHIFT)";
			}
			os << L")\n";
	
			os << L"#define " << parent->el_name() << "_RMKS(";
			arg1 = true;
			for (size_t i = 0; i != blen; ++i)
			{
				if (!arg1)
					os << L',';
				if (!bthings[i]->isro())
				{
					arg1 = false;
					os << bthings[i]->el_argname();
				}
			}
			os << L") (\\\n\t((";
			for (size_t i = 0; i != blen; ++i)
			{
				if (i != 0)
					os << L" | \\\n\t((";
	
				if (bthings[i]->isro())
					os << bthings[i]->el_name() << "_DEFAULT";
				else
					os << parent->el_name() << L"_arg" << (i + 1) << L"_##" <<
						 bthings[i]->el_argname();
				os << L") << _" << bthings[i]->el_name() << "_SHIFT)";
			}
			os << L")\n";
		}

		os << L"#define " << parent->el_name() << L"_DEFAULT (\\\n\t(";
		for (size_t i = 0; i != blen; ++i)
		{
			if (i != 0)
				os << L" | \\\n\t(";

			os << bthings[i]->el_name() << L"_DEFAULT << _" << bthings[i]->el_name() << "_SHIFT)";
		}
		os << L")\n\n";


	}
}

void square_bracket_sequence::generate_c(wostream& os, thing * parent, const int argno)
{
	size_t i = 0;
	size_t j = 0;
	int vals[3];

	vals[0] = 0;
	vals[1] = 1;
	vals[2] = 0;

	while (i < len())
	{
		thing& el = *(*this)[i++];

		switch (el.el_type())
		{
			case thing::comma:
				if (++j >= 3)
					throw syntax_error(el);
				break;
			case thing::number:
				vals[j] = el.el_number();
				break;
			default:
				throw syntax_error(el);
		}
	}

	field_shift = vals[0];
	mask = vals[1] == 32 ? 0xffffffff : (1U << vals[1]) - 1;
	val_shift = vals[2];

	os << L"#define _" << parent->el_name() << L"_SHIFT " << field_shift << L"\n";
	os << L"#define _" << parent->el_name() << L"_MASK 0x" << itowstring(mask << field_shift, 16) << L"\n";

	if (!parent->isro())
	{
		os << "#define " << parent->el_name() << L"_OF(x) (x)\n";
		os << L"#define _" << parent->el_name(1) << L"_arg" << argno <<
			L"_" << parent->el_name(0, 1) << L"_OF(x) (x)\n";
	
		os << "#define " << parent->el_name() << L"_VAL(x) " << sb_val() << L"\n";
		os << L"#define _" << parent->el_name(1) << L"_arg" << argno <<
			L"_" << parent->el_name(0, 1) << L"_VAL(x) " << sb_val() << L"\n";;
	}


}



void thing::set_sequence(thing_sequence * const seq)
{
	section = seq;
}

int
wmain(int argc, wchar_t *argv[])
{
	if (argc < 2)
	{
		wcerr << L"Usage: hwdc2 <infile>\n";
		return 1;
	}

	try
	{
		pp_stream src(argv[1]);
		thing_sequence things(src);
		if (argc <= 2)
			things.generate_c(wcout, NULL);
		else
		{
			things.generate_c(wofstream(argv[2]), NULL);
		}
	}
	catch (hwdc_error& err)
	{
		wcout << wstring(err) << L"\n";
	}

	return 0;
}
