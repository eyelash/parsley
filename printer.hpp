#pragma once

#include "common.hpp"
#include "os.hpp"

namespace printer {

class Context {
	BufferedOutput& output;
	unsigned int indentation = 0;
	bool is_at_bol = true;
public:
	Context(BufferedOutput& output = StandardOutput::get()): output(output) {}
	void print(char c) {
		if (c == '\n') {
			output.write('\n');
			is_at_bol = true;
		}
		else {
			if (is_at_bol) {
				for (unsigned int i = 0; i < indentation; ++i) {
					output.write('\t');
				}
				is_at_bol = false;
			}
			output.write(c);
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

inline void print_impl(char c, Context& context) {
	context.print(c);
}

inline void print_impl(const StringView& s, Context& context) {
	for (char c: s) {
		context.print(c);
	}
}

inline void print_impl(const char* s, Context& context) {
	print_impl(StringView(s), context);
}

inline void print_impl(const std::string& s, Context& context) = delete;

template <class P> enable_if_t<is_printer<P>::value> print_impl(const P& p, Context& context) {
	p.print(context);
}

template <class... P> class Sequence;
template <> class Sequence<> {
public:
	constexpr Sequence() {}
};
template <class P0, class... P> class Sequence<P0, P...> {
public:
	P0 p0;
	Sequence<P...> p;
	constexpr Sequence(P0 p0, P... p): p0(p0), p(p...) {}
};
template <class... P> constexpr Sequence<P...> sequence(P... p) {
	return Sequence<P...>(p...);
}

inline void print_impl(const Sequence<>& p, Context& context) {}
template <class P0, class... P> void print_impl(const Sequence<P0, P...>& p, Context& context) {
	print_impl(p.p0, context);
	print_impl(p.p, context);
}

template <class... P> class Format {
public:
	Sequence<P...> p;
	const char* s;
	constexpr Format(const char* s, P... p): p(p...), s(s) {}
};
template <class... P> constexpr Format<P...> format(const char* s, P... p) {
	return Format<P...>(s, p...);
}

inline void print_formatted(const Sequence<>& p, const char* s, Context& context) {
	print_impl(s, context);
}
template <class P0, class... P> void print_formatted(const Sequence<P0, P...>& p, const char* s, Context& context) {
	while (*s) {
		if (*s == '%') {
			++s;
			if (*s != '%') {
				print_impl(p.p0, context);
				print_formatted(p.p, s, context);
				return;
			}
		}
		context.print(*s);
		++s;
	}
}
template <class... P> void print_impl(const Format<P...>& p, Context& context) {
	print_formatted(p.p, p.s, context);
}

template <class P> class Ln {
public:
	P p;
	constexpr Ln(P p): p(p) {}
};
template <class P> constexpr Ln<P> ln(P p) {
	return Ln<P>(p);
}
constexpr Ln<Sequence<>> ln() {
	return ln(sequence());
}

template <class P> void print_impl(const Ln<P>& p, Context& context) {
	print_impl(p.p, context);
	#ifdef HTML
	print_impl("<br/>", context);
	#else
	print_impl('\n', context);
	#endif
}

template <class P> class Indented {
public:
	P p;
	constexpr Indented(P p): p(p) {}
};
template <class P> constexpr Indented<P> indented(P p) {
	return Indented<P>(p);
}

template <class P> void print_impl(const Indented<P>& p, Context& context) {
	context.increase_indentation();
	print_impl(p.p, context);
	context.decrease_indentation();
}

template <class P> class Reference_ {
public:
	const P* p;
	constexpr Reference_(const P& p): p(&p) {}
};
template <class P> constexpr Reference_<P> ref(const P& p) {
	return Reference_<P>(p);
}

template <class P> void print_impl(const Reference_<P>& p, Context& context) {
	print_impl(*p.p, context);
}

template <class F> class Functor {
public:
	F f;
	constexpr Functor(F f): f(f) {}
};
template <class F> constexpr Functor<F> functor(F f) {
	return Functor<F>(f);
}

template <class F> void print_impl(const Functor<F>& p, Context& context) {
	p.f(context);
}

template <class T, T base, char fill_char> class Number {
	T n;
	unsigned int min_width;
	static constexpr char get_digit(T n) {
		return n < 10 ? '0' + n : 'A' + (n - 10);
	}
	static void print(Context& context, T n, unsigned int min_width, unsigned int i = 1) {
		if (n / base > 0) {
			print(context, n / base, min_width, i + 1);
		}
		else for (; i < min_width; ++i) {
			context.print(fill_char);
		}
		context.print(get_digit(n % base));
	}
	static constexpr unsigned int get_width(T n, unsigned int i = 1) {
		return n / base > 0 ? get_width(n / base, i + 1) : i;
	}
public:
	constexpr Number(T n, unsigned int min_width): n(n), min_width(min_width) {}
	void print(Context& context) const {
		print(context, n, min_width);
	}
	constexpr unsigned int get_width() const {
		return get_width(n);
	}
};
constexpr Number<unsigned int, 10, ' '> number(unsigned int n, unsigned int min_width = 0) {
	return Number<unsigned int, 10, ' '>(n, min_width);
}

constexpr Number<unsigned int, 16, '0'> hexadecimal(unsigned int n, unsigned int min_width = 0) {
	return Number<unsigned int, 16, '0'>(n, min_width);
}
template <class T> constexpr Number<std::uintptr_t, 16, '0'> pointer(const T* ptr) {
	return Number<std::uintptr_t, 16, '0'>(reinterpret_cast<std::uintptr_t>(ptr), sizeof(T*) * 2);
}

constexpr Number<unsigned int, 8, '0'> octal(unsigned int n, unsigned int min_width = 0) {
	return Number<unsigned int, 8, '0'>(n, min_width);
}

template <class P> class SGRPrinter {
	unsigned int enable;
	unsigned int disable;
	P p;
public:
	constexpr SGRPrinter(unsigned int enable, unsigned int disable, P p): enable(enable), disable(disable), p(p) {}
	void print(Context& context) const {
		constexpr const char* CSI = "\x1B[";
		print_impl(sequence(CSI, number(enable), 'm', p, CSI, number(disable), 'm'), context);
	}
};

class SGRFunctor {
	unsigned int enable;
	unsigned int disable;
public:
	constexpr SGRFunctor(unsigned int enable, unsigned int disable): enable(enable), disable(disable) {}
	template <class P> constexpr SGRPrinter<P> operator ()(P p) const {
		return SGRPrinter<P>(enable, disable, p);
	}
};

template <class P> class HTMLSpanPrinter {
	const char* property;
	const char* value;
	P p;
public:
	constexpr HTMLSpanPrinter(const char* property, const char* value, P p): property(property), value(value), p(p) {}
	void print(Context& context) const {
		print_impl(format("<span style=\"%:%\">%</span>", property, value, p), context);
	}
};

class HTMLSpanFunctor {
	const char* property;
	const char* value;
public:
	constexpr HTMLSpanFunctor(const char* property, const char* value): property(property), value(value) {}
	template <class P> constexpr HTMLSpanPrinter<P> operator ()(P p) const {
		return HTMLSpanPrinter<P>(property, value, p);
	}
};

#if HTML
constexpr auto bold = HTMLSpanFunctor("font-weight", "bold");
constexpr auto red = HTMLSpanFunctor("color", "red");
constexpr auto green = HTMLSpanFunctor("color", "lime");
constexpr auto yellow = HTMLSpanFunctor("color", "yellow");
constexpr auto blue = HTMLSpanFunctor("color", "blue");
constexpr auto magenta = HTMLSpanFunctor("color", "fuchsia");
constexpr auto cyan = HTMLSpanFunctor("color", "aqua");
#else
constexpr auto bold = SGRFunctor(1, 22);
constexpr auto red = SGRFunctor(31, 39);
constexpr auto green = SGRFunctor(32, 39);
constexpr auto yellow = SGRFunctor(33, 39);
constexpr auto blue = SGRFunctor(34, 39);
constexpr auto magenta = SGRFunctor(35, 39);
constexpr auto cyan = SGRFunctor(36, 39);
#endif

template <class P> class Repeat {
public:
	P p;
	unsigned int count;
	constexpr Repeat(P p, unsigned int count): p(p), count(count) {}
};
template <class P> constexpr Repeat<P> repeat(P p, unsigned int count) {
	return Repeat<P>(p, count);
}

template <class P> void print_impl(const Repeat<P>& p, Context& context) {
	for (unsigned int i = 0; i < p.count; ++i) {
		print_impl(p.p, context);
	}
}

class Plural {
public:
	const char* word;
	unsigned int count;
	constexpr Plural(const char* word, unsigned int count): word(word), count(count) {}
};
constexpr Plural plural(const char* word, unsigned int count) {
	return Plural(word, count);
}

inline void print_impl(const Plural& p, Context& context) {
	print_impl(number(p.count), context);
	context.print(' ');
	print_impl(p.word, context);
	if (p.count != 1) {
		context.print('s');
	}
}

struct DiagnosticType {
	struct Error {
		static constexpr const char* severity = "error";
		static constexpr auto& color = red;
	};
	struct Warning {
		static constexpr const char* severity = "warning";
		static constexpr auto& color = yellow;
	};
};

template <class Type, class P> void print_diagnostic(Context& context, const P& p) {
	print_impl(bold(Type::color(format("%: ", Type::severity))), context);
	print_impl(p, context);
	print_impl(ln(), context);
}
template <class Type, class P> void print_diagnostic(Context& context, const StringView& path, const P& p) {
	print_diagnostic<Type>(context, p);
	if (path) {
		print_impl(ln(format("--> %", path)), context);
	}
}
template <class Type, class P> void print_diagnostic(Context& context, const StringView& path, const StringView& source, SourceLocation location, const P& p) {
	location.begin = std::min(location.begin, source.size());
	location.end = std::min(std::max(location.end, location.begin + 1), source.size() + 1);
	unsigned int line_number = 1;
	std::size_t line_start = 0;
	std::size_t i;
	for (i = 0; i < location.begin; ++i) {
		if (source[i] == '\n') {
			++line_number;
			line_start = i + 1;
		}
	}
	const unsigned int first_line_number = line_number;
	for (; i + 1 < location.end; ++i) {
		if (source[i] == '\n') {
			++line_number;
		}
	}
	const unsigned int last_line_number = line_number;
	const unsigned int line_number_width = number(last_line_number).get_width();

	print_diagnostic<Type>(context, p);

	if (path) {
		print_impl(ln(format(" %--> %", repeat(' ', line_number_width), path)), context);
	}

	print_impl(ln(format(" % |", repeat(' ', line_number_width))), context);

	for (line_number = first_line_number; line_number <= last_line_number; ++line_number) {
		if (line_number == first_line_number || line_number == last_line_number) {

			constexpr unsigned int TAB_WIDTH = 4;

			print_impl(format(" % | ", number(line_number, line_number_width)), context);
			unsigned int spaces = 0;
			unsigned int carets = 0;
			unsigned int column = 0;
			for (i = line_start; i < source.size() && source[i] != '\n'; ++i) {
				unsigned int char_width;
				if (source[i] == '\t') {
					char_width = TAB_WIDTH - column % TAB_WIDTH;
					print_impl(repeat(' ', char_width), context);
				}
				else {
					char_width = 1;
					context.print(source[i]);
				}
				if (i < location.end) {
					if (i < location.begin) {
						spaces += char_width;
					}
					else {
						carets += char_width;
					}
				}
				column += char_width;
			}
			if (i == location.begin || i + 1 == location.end) {
				++carets;
			}
			print_impl(ln(), context);

			print_impl(format(" % | ", repeat(' ', line_number_width)), context);
			print_impl(sequence(repeat(' ', spaces), bold(Type::color(repeat('^', carets)))), context);
			print_impl(ln(), context);

		}
		else {
			if (line_number == first_line_number + 1) {
				print_impl(ln(format(" %...", repeat(' ', line_number_width))), context);
			}
			for (i = line_start; i < source.size() && source[i] != '\n'; ++i) {}
		}
		line_start = i + 1;
	}
}
template <class... A> void print_error(A&&... a) {
	Context context(StandardError::get());
	print_diagnostic<DiagnosticType::Error>(context, std::forward<A>(a)...);
}
template <class... A> void print_warning(A&&... a) {
	Context context(StandardError::get());
	print_diagnostic<DiagnosticType::Warning>(context, std::forward<A>(a)...);
}

}

template <class P> void print(printer::Context& context, P&& p) {
	using namespace printer;
	print_impl(std::forward<P>(p), context);
}
template <class P> void print(BufferedOutput& output, P&& p) {
	printer::Context context(output);
	print(context, std::forward<P>(p));
}
template <class P> void print(P&& p) {
	print(StandardOutput::get(), std::forward<P>(p));
}
template <class P> void println(BufferedOutput& output, P&& p) {
	print(output, printer::ln(std::forward<P>(p)));
	output.flush();
}
template <class P> void println(P&& p) {
	println(StandardOutput::get(), std::forward<P>(p));
}
inline void println() {
	println(printer::sequence());
}
template <class P> std::string print_to_string(P&& p) {
	std::string s;
	StringOutput output(s);
	BufferedOutput buffered(output);
	print(buffered, std::forward<P>(p));
	return s;
}

class Diagnostic {
	Path path;
	SourceLocation location;
	std::string message;
public:
	template <class P> Diagnostic(const char* path, const SourceLocation& location, P&& p): path(path), location(location), message(print_to_string(std::forward<P>(p))) {}
	template <class P> Diagnostic(const char* path, P&& p): path(path), message(print_to_string(std::forward<P>(p))) {}
	template <class P> Diagnostic(P&& p): message(print_to_string(std::forward<P>(p))) {}
	template <class Type> void print(printer::Context& context) const {
		if (path && location) {
			MemoryMappedFile source(path);
			printer::print_diagnostic<Type>(context, path, StringView(source.data(), source.size()), location, StringView(message));
		}
		else {
			printer::print_diagnostic<Type>(context, path, StringView(message));
		}
		print_impl(printer::ln(), context);
	}
};

class Diagnostics {
	std::vector<Diagnostic> errors;
	std::vector<Diagnostic> warnings;
public:
	bool has_error() const {
		return !errors.empty();
	}
	template <class... A> void add_error(A&&... a) {
		errors.emplace_back(std::forward<A>(a)...);
	}
	template <class... A> void add_warning(A&&... a) {
		warnings.emplace_back(std::forward<A>(a)...);
	}
	void print(printer::Context& context) const {
		for (const Diagnostic& warning: warnings) {
			warning.print<printer::DiagnosticType::Warning>(context);
		}
		for (const Diagnostic& error: errors) {
			error.print<printer::DiagnosticType::Error>(context);
		}
	}
	void print() const {
		printer::Context context(StandardError::get());
		print(context);
	}
};
