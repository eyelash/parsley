#pragma once

#include "common.hpp"
#include <iostream>
#include <sstream>

namespace printer {

class Context {
	std::ostream& ostream;
	unsigned int indentation = 0;
	bool is_at_bol = true;
public:
	Context(std::ostream& ostream = std::cout): ostream(ostream) {}
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

template <class P, class = void> struct is_printer: std::false_type {};
template <class P> struct is_printer<P, decltype(std::declval<const P&>().print(std::declval<Context&>()))>: std::true_type {};

constexpr char get_printer(char c) {
	return c;
}
constexpr StringView get_printer(const StringView& s) {
	return StringView(s);
}
constexpr StringView get_printer(const char* s) {
	return StringView(s);
}
StringView get_printer(const std::string& s) {
	return StringView(s.data(), s.size());
}
template <class P> constexpr std::enable_if_t<is_printer<P>::value, P> get_printer(P p) {
	return p;
}

inline void print_impl(char c, Context& context) {
	context.print(c);
}

inline void print_impl(const StringView& s, Context& context) {
	for (char c: s) {
		context.print(c);
	}
}

template <class P> std::enable_if_t<is_printer<P>::value> print_impl(const P& p, Context& context) {
	p.print(context);
}

template <class P> class Ln {
	P p;
public:
	constexpr Ln(P p): p(p) {}
	void print(Context& context) const {
		print_impl(p, context);
		context.print('\n');
	}
};
template <class P> constexpr auto ln(P&& p) {
	return Ln(get_printer(std::forward<P>(p)));
}
constexpr char ln() {
	return '\n';
}

template <class P> class Indent {
	P p;
public:
	constexpr Indent(P p): p(p) {}
	void print(Context& context) const {
		context.increase_indentation();
		print_impl(p, context);
		context.decrease_indentation();
	}
};
template <class P> constexpr auto indented(P&& p) {
	return Indent(get_printer(std::forward<P>(p)));
}

template <class F> class PrintFunctor {
	F f;
public:
	constexpr PrintFunctor(F f): f(f) {}
	void print(Context& context) const {
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
	void print(Context& context) const {}
	void print_formatted(Context& context, const char* s) const {
		print_impl(get_printer(s), context);
	}
};
template <class T0, class... T> class PrintTuple<T0, T...> {
	T0 t0;
	PrintTuple<T...> t;
public:
	constexpr PrintTuple(T0 t0, T... t): t0(t0), t(t...) {}
	void print(Context& context) const {
		print_impl(t0, context);
		t.print(context);
	}
	void print_formatted(Context& context, const char* s) const {
		while (*s) {
			if (*s == '%') {
				++s;
				if (*s != '%') {
					print_impl(t0, context);
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
	void print(Context& context) const {
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
constexpr auto blue = [](const auto& t) {
	return print_tuple("\x1B[34m", t, "\x1B[39m");
};
constexpr auto magenta = [](const auto& t) {
	return print_tuple("\x1B[35m", t, "\x1B[39m");
};
constexpr auto cyan = [](const auto& t) {
	return print_tuple("\x1B[36m", t, "\x1B[39m");
};

class NumberPrinter {
	unsigned int n;
public:
	constexpr NumberPrinter(unsigned int n): n(n) {}
	void print(Context& context) const {
		if (n >= 10) {
			NumberPrinter(n / 10).print(context);
		}
		context.print(static_cast<char>('0' + n % 10));
	}
	unsigned int get_width() const {
		unsigned int width = 0;
		if (n >= 10) {
			width += NumberPrinter(n / 10).get_width();
		}
		return width + 1;
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
	void print(Context& context) const {
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
	void print(Context& context) const {
		if (n >= 8 || digits > 1) {
			OctalPrinter(n / 8, digits > 1 ? digits - 1 : digits).print(context);
		}
		context.print(static_cast<char>('0' + n % 8));
	}
};
constexpr OctalPrinter print_octal(unsigned int n, unsigned int digits = 1) {
	return OctalPrinter(n, digits);
}

template <class P> class Repeat {
	P p;
	unsigned int count;
public:
	constexpr Repeat(P p, unsigned int count): p(p), count(count) {}
	void print(Context& context) const {
		for (unsigned int i = 0; i < count; ++i) {
			print_impl(p, context);
		}
	}
};
template <class P> constexpr auto repeat(P&& p, unsigned int count) {
	return Repeat(get_printer(std::forward<P>(p)), count);
}

class Plural {
	const char* word;
	unsigned int count;
public:
	constexpr Plural(const char* word, unsigned int count): word(word), count(count) {}
	void print(Context& context) const {
		print_impl(print_number(count), context);
		context.print(' ');
		print_impl(get_printer(word), context);
		if (count != 1) {
			context.print('s');
		}
	}
};
constexpr Plural print_plural(const char* word, unsigned int count) {
	return Plural(word, count);
}

template <class P, class C> void print_message(Context& context, const C& color, const char* severity, const P& p) {
	print_impl(bold(color(format("%: ", severity))), context);
	print_impl(p, context);
	context.print('\n');
}
template <class P, class C> void print_message(Context& context, const char* path, const StringView& source, std::size_t source_position, const C& color, const char* severity, const P& p) {
	unsigned int line_number = 1;
	const char* c = source.begin();
	const char* end = source.end();
	const char* position = std::min(c + source_position, end);
	const char* line_start = c;
	while (c < position) {
		if (*c == '\n') {
			++line_number;
			line_start = c + 1;
		}
		++c;
	}
	const unsigned int column = 1 + (c - line_start);
	const unsigned int line_number_width = print_number(line_number).get_width();

	print_message(context, color, severity, p);

	print_impl(ln(format(" %--> %:%:%:", repeat(' ', line_number_width), path, print_number(line_number), print_number(column))), context);

	print_impl(ln(format(" % |", repeat(' ', line_number_width))), context);

	print_impl(format(" % | ", print_number(line_number)), context);
	c = line_start;
	while (c < end && *c != '\n') {
		context.print(*c);
		++c;
	}
	context.print('\n');

	print_impl(format(" % | ", repeat(' ', line_number_width)), context);
	c = line_start;
	while (c < position) {
		context.print(*c == '\t' ? '\t' : ' ');
		++c;
	}
	print_impl(bold(color('^')), context);
	context.print('\n');
}
inline void print_error(const char* path, std::size_t source_position, const std::string& message) {
	Context context(std::cerr);
	if (path == nullptr) {
		print_message(context, red, "error", get_printer(message));
		return;
	}
	auto source = read_file(path);
	print_message(context, path, StringView(source.data(), source.size()), source_position, red, "error", get_printer(message));
}
inline void print_error(const char* path, const StringView& source, std::size_t source_position, const std::string& message) {
	Context context(std::cerr);
	print_message(context, path, source, source_position, red, "error", get_printer(message));
}
inline void print_warning(const char* path, const StringView& source, std::size_t source_position, const std::string& message) {
	Context context(std::cerr);
	print_message(context, path, source, source_position, yellow, "warning", get_printer(message));
}

}

template <class P> void print(std::ostream& ostream, P&& p) {
	printer::Context context(ostream);
	printer::print_impl(printer::get_printer(std::forward<P>(p)), context);
}
template <class P> void print(P&& p) {
	print(std::cout, std::forward<P>(p));
}
template <class P> std::string print_to_string(P&& p) {
	std::ostringstream ostream;
	print(ostream, std::forward<P>(p));
	return ostream.str();
}
