#pragma once

#include "common.hpp"
#include "printer.hpp"

namespace parser {

using SavePoint = const char*;

class Context {
	const SourceFile* file;
	const char* position;
	std::string error;
public:
	Context(const SourceFile* file): file(file), position(file->begin()) {}
	Context(const SourceFile* file, const char* position): file(file), position(position) {}
	operator bool() const {
		return position < file->end();
	}
	constexpr char operator *() const {
		return *position;
	}
	Context& operator ++() {
		++position;
		return *this;
	}
	template <class P> void set_error(P&& p) {
		error = print_to_string(std::forward<P>(p));
	}
	const std::string& get_error() const {
		return error;
	}
	SavePoint save() const {
		return position;
	}
	void restore(SavePoint save_point) {
		position = save_point;
	}
	constexpr StringView operator -(SavePoint save_point) const {
		return StringView(save_point, position - save_point);
	}
	const char* get_path() const {
		return file->get_path();
	}
	std::size_t get_position() const {
		return position - file->begin();
	}
};

enum Result: char {
	SUCCESS,
	FAILURE,
	ERROR
};

template <class F> class Char {
public:
	F f;
	constexpr Char(F f): f(f) {}
};

class String {
public:
	StringView s;
	constexpr String(const StringView& s): s(s) {}
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
	constexpr Sequence(P0 head, P... tail): head(head), tail(tail...) {}
};

template <class... P> class Choice;
template <> class Choice<> {
public:
	constexpr Choice() {}
};
template <class P0, class... P> class Choice<P0, P...> {
public:
	P0 head;
	Choice<P...> tail;
	constexpr Choice(P0 head, P... tail): head(head), tail(tail...) {}
};

template <class P> class Repetition {
public:
	P p;
	constexpr Repetition(P p): p(p) {}
};

template <class P> class Not {
public:
	P p;
	constexpr Not(P p): p(p) {}
};

template <class P> class Peek {
public:
	P p;
	constexpr Peek(P p): p(p) {}
};

template <class P, auto F> class Map;
template <class P, class R, class A, R (*F)(A)> class Map<P, F> {
public:
	P p;
	constexpr Map(P p): p(p) {}
};

class Error_ {
public:
	StringView s;
	constexpr Error_(const StringView& s): s(s) {}
};

class Expect {
public:
	StringView s;
	constexpr Expect(const StringView& s): s(s) {}
};

template <class P, class R> class Cast {
public:
	P p;
	constexpr Cast(P p): p(p) {}
};

template <class T> class Reference_ {
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
template <class... P> constexpr Sequence<P...> sequence_(P... p) {
	return Sequence<P...>(p...);
}
template <class... P> constexpr auto sequence(P... p) {
	return sequence_(get_parser(p)...);
}
template <class... P> constexpr Choice<P...> choice_(P... p) {
	return Choice<P...>(p...);
}
template <class... P> constexpr auto choice(P... p) {
	return choice_(get_parser(p)...);
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
template <auto F, class P> constexpr Map<P, F> map_(P p) {
	return Map<P, F>(p);
}
template <auto F, class P> constexpr auto map(P p) {
	return map_<F>(get_parser(p));
}
constexpr Error_ error(const StringView& s) {
	return Error_(s);
}
constexpr Expect expect(const StringView& s) {
	return Expect(s);
}
template <class R, class P> constexpr Cast<P, R> cast_(P p) {
	return Cast<P, R>(p);
}
template <class R, class P> constexpr auto cast(P p) {
	return cast_<R>(get_parser(p));
}
template <class T> constexpr auto reference() {
	return Reference_<T>();
}

template <class F, class C> Result parse_impl(const Char<F>& p, Context& context, const C& callback) {
	if (context && p.f(*context)) {
		++context;
		return SUCCESS;
	}
	return FAILURE;
}

template <class C> Result parse_impl(const String& p, Context& context, const C& callback) {
	const SavePoint save_point = context.save();
	for (char c: p.s) {
		if (!(context && *context == c)) {
			context.restore(save_point);
			return FAILURE;
		}
		++context;
	}
	return SUCCESS;
}

template <class C> Result parse_impl(const Sequence<>& p, Context& context, const C& callback, const SavePoint& save_point) {
	return SUCCESS;
}
template <class P0, class... P, class C> Result parse_impl(const Sequence<P0, P...>& p, Context& context, const C& callback, const SavePoint& save_point) {
	Result result = parse_impl(p.head, context, callback);
	if (result == ERROR) {
		return ERROR;
	}
	if (result == FAILURE) {
		context.restore(save_point);
		return FAILURE;
	}
	return parse_impl(p.tail, context, callback);
}
template <class... P, class C> Result parse_impl(const Sequence<P...>& p, Context& context, const C& callback) {
	return parse_impl(p, context, callback, context.save());
}

template <class C> Result parse_impl(const Choice<>& p, Context& context, const C& callback) {
	return FAILURE;
}
template <class P0, class... P, class C> Result parse_impl(const Choice<P0, P...>& p, Context& context, const C& callback) {
	Result result = parse_impl(p.head, context, callback);
	if (result == ERROR) {
		return ERROR;
	}
	if (result == FAILURE) {
		return parse_impl(p.tail, context, callback);
	}
	return SUCCESS;
}

template <class P, class C> Result parse_impl(const Repetition<P>& p, Context& context, const C& callback) {
	while (true) {
		Result result = parse_impl(p.p, context, callback);
		if (result == ERROR) {
			return ERROR;
		}
		if (result == FAILURE) {
			break;
		}
	}
	return SUCCESS;
}

template <class P, class C> Result parse_impl(const Not<P>& p, Context& context, const C& callback) {
	const SavePoint save_point = context.save();
	Result result = parse_impl(p.p, context, callback);
	if (result == ERROR) {
		return ERROR;
	}
	if (result == FAILURE) {
		return SUCCESS;
	}
	context.restore(save_point);
	return FAILURE;
}

template <class C> Result parse_impl(const Error_& p, Context& context, const C& callback) {
	context.set_error(p.s);
	return ERROR;
}

template <class C> Result parse_impl(const Expect& p, Context& context, const C& callback) {
	Result result = parse_impl(String(p.s), context, callback);
	if (result == ERROR) {
		return ERROR;
	}
	if (result == FAILURE) {
		context.set_error(printer::format("expected \"%\"", p.s));
		return ERROR;
	}
	return SUCCESS;
}

template <class T, class C> Result parse_impl(const Reference_<T>& p, Context& context, const C& callback) {
	return parse_impl(T::parser, context, callback);
}

}
