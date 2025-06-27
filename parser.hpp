#pragma once

#include "common.hpp"

class Context {
	const char* position;
	const char* end;
	const char* begin;
public:
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
	using Savepoint = const char*;
	Savepoint save() const {
		return position;
	}
	void restore(Savepoint savepoint) {
		position = savepoint;
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

template <class... P> class Sequence {
	Tuple<P...> p;
public:
	constexpr Sequence(P... p): p(p...) {}
	template <std::size_t I> const auto& get(Index<I> i) const {
		return p.get(i);
	}
};

template <class... P> class Choice {
	Tuple<P...> p;
public:
	constexpr Choice(P... p): p(p...) {}
	template <std::size_t I> const auto& get(Index<I> i) const {
		return p.get(i);
	}
};

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

template <class... P, std::size_t I> bool parse(const Sequence<P...>& p, Context& context, Index<I> i, const Context::Savepoint& savepoint) {
	if constexpr (I == sizeof...(P)) {
		return true;
	}
	else {
		if (!parse(p.get(i), context)) {
			context.restore(savepoint);
			return false;
		}
		return parse(p, context, Index<I + 1>(), savepoint);
	}
}
template <class... P> bool parse(const Sequence<P...>& p, Context& context) {
	return parse(p, context, Index<0>(), context.save());
}

template <class... P, std::size_t I> bool parse(const Choice<P...>& p, Context& context, Index<I> i) {
	if constexpr (I == sizeof...(P)) {
		return false;
	}
	else {
		if (parse(p.get(i), context)) {
			return true;
		}
		return parse(p, context, Index<I + 1>());
	}
}
template <class... P> bool parse(const Choice<P...>& p, Context& context) {
	return parse(p, context, Index<0>());
}

template <class P> bool parse(const Repetition<P>& p, Context& context) {
	while (parse(p.get(), context)) {}
	return true;
}

template <class P> bool parse(const Not<P>& p, Context& context) {
	const auto savepoint = context.save();
	if (parse(p.get(), context)) {
		context.restore(savepoint);
		return false;
	}
	else {
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
