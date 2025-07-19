#pragma once

#include "common.hpp"
#include "printer.hpp"

class Context {
	const char* position;
	const char* end;
	const char* begin;
	std::string error;
public:
	Context(const StringView& s): position(s.begin()), end(s.end()), begin(s.begin()) {}
	template <std::size_t N> constexpr Context(const char (&s)[N]): position(s), end(s + N), begin(s) {}
	explicit constexpr operator bool() const {
		return position < end;
	}
	constexpr char operator *() const {
		return *position;
	}
	Context& operator ++() {
		++position;
		return *this;
	}
	bool has_error() const {
		return !error.empty();
	}
	void set_error(std::string&& error) {
		this->error = std::move(error);
	}
	template <class P> void set_error(P&& p) {
		this->error = print_to_string(std::forward<P>(p));
	}
	const std::string& get_error() const {
		return error;
	}
	using Savepoint = const char*;
	Savepoint save() const {
		return position;
	}
	void restore(Savepoint savepoint) {
		position = savepoint;
	}
	StringView operator -(Savepoint savepoint) const {
		return StringView(savepoint, position - savepoint);
	}
	std::size_t get_position() const {
		return position - begin;
	}
};

template <class F> class Char {
	F f;
public:
	constexpr Char(F f): f(f) {}
	constexpr bool operator ()(char c) const {
		return f(c);
	}
};

class String: public StringView {
public:
	constexpr String(const StringView& s): StringView(s) {}
};

template <class... P> class Sequence;
template <> class Sequence<> {
public:
	constexpr Sequence() {}
};
template <class P0, class... P> class Sequence<P0, P...> {
public:
	P0 head;
	Sequence<P...> tail;
	constexpr Sequence(P0 p0, P... p): head(p0), tail(p...) {}
	const P0& get(Index<0>) const {
		return head;
	}
	template <std::size_t I> const auto& get(Index<I>) const {
		return tail.get(Index<I - 1>());
	}
};
template <class... P> Sequence(P...) -> Sequence<P...>;

template <class... P> class Choice;
template <> class Choice<> {
public:
	constexpr Choice() {}
};
template <class P0, class... P> class Choice<P0, P...> {
public:
	P0 head;
	Choice<P...> tail;
	constexpr Choice(P0 p0, P... p): head(p0), tail(p...) {}
	const P0& get(Index<0>) const {
		return head;
	}
	template <std::size_t I> const auto& get(Index<I>) const {
		return tail.get(Index<I - 1>());
	}
};
template <class... P> Choice(P...) -> Choice<P...>;

template <class P> class Repetition {
	P p;
public:
	constexpr Repetition(P p): p(p) {}
	const P& get() const {
		return p;
	}
};

template <class P> class Not {
	P p;
public:
	constexpr Not(P p): p(p) {}
	const P& get() const {
		return p;
	}
};

template <class P> class Peek {
	P p;
public:
	constexpr Peek(P p): p(p) {}
	const P& get() const {
		return p;
	}
};

template <class P> class ToStringView {
	P p;
public:
	constexpr ToStringView(P p): p(p) {}
	const P& get() const {
		return p;
	}
};

class Error {
public:
	StringView s;
	constexpr Error(const StringView& s): s(s) {}
};

class Expect {
public:
	StringView s;
	constexpr Expect(const StringView& s): s(s) {}
};

template <class P> class Reference_ {
public:
	constexpr Reference_() {}
};

template <class F, class = bool> struct is_char_class: std::false_type {};
template <class F> struct is_char_class<F, decltype(std::declval<F>()(std::declval<char>()))>: std::true_type {};

constexpr auto get_parser(char c) {
	return Char([c](char c2) {
		return c == c2;
	});
}
constexpr String get_parser(const StringView& s) {
	return String(s);
}
constexpr String get_parser(const char* s) {
	return String(s);
}
template <class P> constexpr std::enable_if_t<!is_char_class<P>::value, P> get_parser(P p) {
	return p;
}
template <class F> constexpr std::enable_if_t<is_char_class<F>::value, Char<F>> get_parser(F f) {
	return Char(f);
}
constexpr auto range(char first, char last) {
	return Char([first, last](char c) {
		return c >= first && c <= last;
	});
}
template <class... P> constexpr auto sequence(P... p) {
	return Sequence(get_parser(p)...);
}
template <class... P> constexpr auto choice(P... p) {
	return Choice(get_parser(p)...);
}
template <class P> constexpr auto repetition(P p) {
	return Repetition(get_parser(p));
}
template <class P> constexpr auto zero_or_more(P p) {
	return repetition(p);
}
template <class P> constexpr auto one_or_more(P p) {
	return sequence(p, repetition(p));
}
template <class P> constexpr auto not_(P p) {
	return Not(get_parser(p));
}
template <class P> constexpr auto peek(P p) {
	return Peek(get_parser(p));
}
template <class P> constexpr auto to_string_view(P p) {
	return ToStringView(get_parser(p));
}
constexpr Error error(const StringView& s) {
	return Error(s);
}
constexpr Expect expect(const StringView& s) {
	return Expect(s);
}
template <class T> constexpr auto reference() {
	return Reference_<T>();
}

template <class F> bool parse(const Char<F>& p, Context& context) {
	if (context && p(*context)) {
		++context;
		return true;
	}
	return false;
}

inline bool parse(const String& p, Context& context) {
	const auto savepoint = context.save();
	for (char c: p) {
		if (!parse(get_parser(c), context)) {
			context.restore(savepoint);
			return false;
		}
	}
	return true;
}

inline bool parse(const Sequence<>& p, Context& context, const Context::Savepoint& savepoint) {
	return true;
}
template <class P0, class... P> bool parse(const Sequence<P0, P...>& p, Context& context, const Context::Savepoint& savepoint) {
	if (parse(p.head, context)) {
		return parse(p.tail, context, savepoint);
	}
	if (context.has_error()) {
		return false;
	}
	context.restore(savepoint);
	return false;
}
template <class... P> bool parse(const Sequence<P...>& p, Context& context) {
	return parse(p, context, context.save());
}

inline bool parse(const Choice<>& p, Context& context) {
	return false;
}
template <class P0, class... P> bool parse(const Choice<P0, P...>& p, Context& context) {
	if (parse(p.head, context)) {
		return true;
	}
	if (context.has_error()) {
		return false;
	}
	return parse(p.tail, context);
}

template <class P> bool parse(const Repetition<P>& p, Context& context) {
	while (parse(p.get(), context)) {}
	if (context.has_error()) {
		return false;
	}
	return true;
}

template <class P> bool parse(const Not<P>& p, Context& context) {
	const auto savepoint = context.save();
	if (parse(p.get(), context)) {
		context.restore(savepoint);
		return false;
	}
	else {
		if (context.has_error()) {
			return false;
		}
		return true;
	}
}

template <class P> bool parse(const Peek<P>& p, Context& context) {
	const auto savepoint = context.save();
	if (parse(p.get(), context)) {
		context.restore(savepoint);
		return true;
	}
	else {
		return false;
	}
}

template <class P> StringView parse(const ToStringView<P>& p, Context& context) {
	const auto savepoint = context.save();
	if (parse(p.get(), context)) {
		return context - savepoint;
	}
	else {
		return StringView();
	}
}

inline bool parse(const Error& p, Context& context) {
	context.set_error(p.s);
	return false;
}

inline bool parse(const Expect& p, Context& context) {
	if (parse(get_parser(p.s), context)) {
		return true;
	}
	else {
		context.set_error(format("expected \"%\"", p.s));
		return false;
	}
}

template <class P> bool parse(const Reference_<P>& p, Context& context) {
	return parse(P::parser, context);
}
