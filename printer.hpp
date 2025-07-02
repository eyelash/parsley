#pragma once

#include "common.hpp"
#include <iostream>
#include <sstream>

class PrintContext {
	std::ostream& ostream;
	unsigned int indentation = 0;
	bool is_at_bol = true;
public:
	PrintContext(std::ostream& ostream = std::cout): ostream(ostream) {}
	void print(char c) {
		if (c == '\n') {
			ostream.put('\n');
			is_at_bol = true;
		}
		else {
			if (is_at_bol) {
				for (unsigned int i = 0; i < indentation; ++i) {
					ostream.put('\t');
				}
				is_at_bol = false;
			}
			ostream.put(c);
		}
	}
	void increase_indentation() {
		++indentation;
	}
	void decrease_indentation() {
		--indentation;
	}
};

class CharPrinter {
	char c;
public:
	constexpr CharPrinter(char c): c(c) {}
	void print(PrintContext& context) const {
		context.print(c);
	}
};

class StringPrinter {
	StringView s;
public:
	constexpr StringPrinter(const StringView& s): s(s) {}
	void print(PrintContext& context) const {
		for (char c: s) {
			context.print(c);
		}
	}
};

template <class P, class = void> struct is_printer: std::false_type {};
template <class P> struct is_printer<P, decltype(std::declval<P>().print(std::declval<PrintContext&>()))>: std::true_type {};

constexpr CharPrinter get_printer(char c) {
	return CharPrinter(c);
}
constexpr StringPrinter get_printer(const StringView& s) {
	return StringPrinter(s);
}
constexpr StringPrinter get_printer(const char* s) {
	return StringPrinter(s);
}
StringPrinter get_printer(const std::string& s) {
	return StringPrinter(StringView(s.data(), s.size()));
}
template <class P> constexpr std::enable_if_t<is_printer<P>::value, P> get_printer(P p) {
	return p;
}

template <class P> void print(std::ostream& ostream, P&& p) {
	PrintContext context(ostream);
	get_printer(std::forward<P>(p)).print(context);
}
template <class P> void print(P&& p) {
	print(std::cout, std::forward<P>(p));
}
template <class P> std::string print_to_string(P&& p) {
	std::ostringstream ostream;
	print(ostream, std::forward<P>(p));
	return ostream.str();
}

template <class P> class LnPrinter {
	P p;
public:
	constexpr LnPrinter(P p): p(p) {}
	void print(PrintContext& context) const {
		p.print(context);
		context.print('\n');
	}
};
template <class P> constexpr auto ln(P&& p) {
	return LnPrinter(get_printer(std::forward<P>(p)));
}
constexpr CharPrinter ln() {
	return CharPrinter('\n');
}

template <class P> class IndentPrinter {
	P p;
public:
	constexpr IndentPrinter(P p): p(p) {}
	void print(PrintContext& c) const {
		c.increase_indentation();
		p.print(c);
		c.decrease_indentation();
	}
};
template <class P> constexpr auto indented(P&& p) {
	return IndentPrinter(get_printer(std::forward<P>(p)));
}

template <class F> class PrintFunctor {
	F f;
public:
	constexpr PrintFunctor(F f): f(f) {}
	void print(PrintContext& context) const {
		f(context);
	}
};
template <class F> constexpr PrintFunctor<F> print_functor(F f) {
	return PrintFunctor(f);
}

template <class... T> class PrintTuple;
template <> class PrintTuple<> {
public:
	constexpr PrintTuple() {}
	void print(PrintContext& context) const {}
	void print_formatted(PrintContext& context, const char* s) const {
		StringPrinter(s).print(context);
	}
};
template <class T0, class... T> class PrintTuple<T0, T...> {
	T0 t0;
	PrintTuple<T...> t;
public:
	constexpr PrintTuple(T0 t0, T... t): t0(t0), t(t...) {}
	void print(PrintContext& context) const {
		t0.print(context);
		t.print(context);
	}
	void print_formatted(PrintContext& context, const char* s) const {
		while (*s) {
			if (*s == '%') {
				++s;
				if (*s != '%') {
					t0.print(context);
					t.print_formatted(context, s);
					return;
				}
			}
			context.print(*s);
			++s;
		}
	}
};
template <class... T> PrintTuple(T...) -> PrintTuple<T...>;
template <class... T> constexpr auto print_tuple(T&&... t) {
	return PrintTuple(get_printer(std::forward<T>(t))...);
}

template <class... T> class Format {
	PrintTuple<T...> t;
	const char* s;
public:
	constexpr Format(const char* s, T... t): t(t...), s(s) {}
	void print(PrintContext& context) const {
		t.print_formatted(context, s);
	}
};
template <class... T> constexpr auto format(const char* s, T&&... t) {
	return Format(s, get_printer(std::forward<T>(t))...);
}

constexpr auto bold = [](const auto& t) {
	return print_tuple("\x1B[1m", t, "\x1B[22m");
};
constexpr auto red = [](const auto& t) {
	return print_tuple("\x1B[31m", t, "\x1B[39m");
};
constexpr auto green = [](const auto& t) {
	return print_tuple("\x1B[32m", t, "\x1B[39m");
};
constexpr auto yellow = [](const auto& t) {
	return print_tuple("\x1B[33m", t, "\x1B[39m");
};

class NumberPrinter {
	unsigned int n;
public:
	constexpr NumberPrinter(unsigned int n): n(n) {}
	void print(PrintContext& context) const {
		if (n >= 10) {
			NumberPrinter(n / 10).print(context);
		}
		context.print(static_cast<char>('0' + n % 10));
	}
};
constexpr NumberPrinter print_number(unsigned int n) {
	return NumberPrinter(n);
}

class HexadecimalPrinter {
	unsigned int n;
	unsigned int digits;
	static constexpr char get_hex(unsigned int c) {
		return c < 10 ? '0' + c : 'A' + (c - 10);
	}
public:
	constexpr HexadecimalPrinter(unsigned int n, unsigned int digits = 1): n(n), digits(digits) {}
	void print(PrintContext& context) const {
		if (n >= 16 || digits > 1) {
			HexadecimalPrinter(n / 16, digits > 1 ? digits - 1 : digits).print(context);
		}
		context.print(get_hex(n % 16));
	}
};
constexpr HexadecimalPrinter print_hexadecimal(unsigned int n, unsigned int digits = 1) {
	return HexadecimalPrinter(n, digits);
}
template <class T> constexpr HexadecimalPrinter print_pointer(const T* ptr) {
	return HexadecimalPrinter(reinterpret_cast<std::size_t>(ptr));
}

class OctalPrinter {
	unsigned int n;
	unsigned int digits;
public:
	constexpr OctalPrinter(unsigned int n, unsigned int digits = 1): n(n), digits(digits) {}
	void print(PrintContext& context) const {
		if (n >= 8 || digits > 1) {
			OctalPrinter(n / 8, digits > 1 ? digits - 1 : digits).print(context);
		}
		context.print(static_cast<char>('0' + n % 8));
	}
};
constexpr OctalPrinter print_octal(unsigned int n, unsigned int digits = 1) {
	return OctalPrinter(n, digits);
}

class PluralPrinter {
	const char* word;
	unsigned int count;
public:
	constexpr PluralPrinter(const char* word, unsigned int count): word(word), count(count) {}
	void print(PrintContext& context) const {
		print_number(count).print(context);
		context.print(' ');
		StringPrinter(word).print(context);
		if (count != 1) {
			context.print('s');
		}
	}
};
constexpr PluralPrinter print_plural(const char* word, unsigned int count) {
	return PluralPrinter(word, count);
}
