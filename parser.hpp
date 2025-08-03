#pragma once

#include "common.hpp"
#include "printer.hpp"

namespace parser {

using SavePoint = const char*;

class Context {
	const char* position;
	const char* end;
	const char* begin;
	std::string error;
public:
	Context(const StringView& s): position(s.begin()), end(s.end()), begin(s.begin()) {}
	Context(const char* s): Context(StringView(s)) {}
	Context(const std::vector<char>& v): Context(StringView(v.data(), v.size())) {}
	Context(const SourceFile* file): Context(StringView(file->data(), file->size())) {}
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
	constexpr StringView get_source() const {
		return StringView(begin, end - begin);
	}
	constexpr std::size_t get_position() const {
		return position - begin;
	}
};

enum Result: char {
	SUCCESS,
	FAILURE,
	ERROR
};

template <class F> class CharClass {
public:
	F f;
	constexpr CharClass(F f): f(f) {}
};

class Char {
public:
	char c;
	constexpr Char(char c): c(c) {}
	constexpr bool operator ()(char c2) const {
		return c2 == c;
	}
};

class AnyChar {
public:
	constexpr AnyChar() {}
	constexpr bool operator ()(char c) const {
		return true;
	}
};

class CharRange {
public:
	char first;
	char last;
	constexpr CharRange(char first, char last): first(first), last(last) {}
	constexpr bool operator ()(char c) const {
		return c >= first && c <= last;
	}
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

template <class P> class Ignore {
public:
	P p;
	constexpr Ignore(P p): p(p) {}
};

template <class P> class ToString {
public:
	P p;
	constexpr ToString(P p): p(p) {}
};

template <class T, class P> class Map {
public:
	P p;
	constexpr Map(P p): p(p) {}
};

template <class T, class P> class Collect {
public:
	P p;
	constexpr Collect(P p): p(p) {}
};

template <class T, class P> class Tagged {
public:
	P p;
	constexpr Tagged(P p): p(p) {}
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

template <class T> class Reference_ {
public:
	constexpr Reference_() {}
};

template <class F, class = bool> struct is_char_class: std::false_type {};
template <class F> struct is_char_class<F, decltype(std::declval<F>()(std::declval<char>()))>: std::true_type {};

constexpr CharClass<Char> get_parser(char c) {
	return CharClass<Char>(Char(c));
}
constexpr StringView get_parser(const StringView& s) {
	return StringView(s);
}
constexpr StringView get_parser(const char* s) {
	return StringView(s);
}
template <class P> constexpr std::enable_if_t<!is_char_class<P>::value, P> get_parser(P p) {
	return p;
}
template <class F> constexpr std::enable_if_t<is_char_class<F>::value, CharClass<F>> get_parser(F f) {
	return CharClass<F>(f);
}

constexpr CharClass<AnyChar> any_char() {
	return CharClass<AnyChar>(AnyChar());
}
constexpr CharClass<CharRange> range(char first, char last) {
	return CharClass<CharRange>(CharRange(first, last));
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
constexpr auto empty() {
	return sequence();
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
template <class P> constexpr auto optional(P p) {
	return choice(p, empty());
}
template <class P> constexpr auto not_(P p) {
	return Not(get_parser(p));
}
template <class P> constexpr auto and_(P p) {
	return not_(not_(p));
}
constexpr auto end() {
	return not_(any_char());
}
template <class P> constexpr auto ignore(P p) {
	return Ignore(get_parser(p));
}
template <class P> constexpr auto to_string(P p) {
	return ToString(get_parser(p));
}
template <class T, class P> constexpr Map<T, P> map_(P p) {
	return Map<T, P>(p);
}
template <class T, class P> constexpr auto map(P p) {
	return map_<T>(get_parser(p));
}
template <class T, class P> constexpr Collect<T, P> collect_(P p) {
	return Collect<T, P>(p);
}
template <class T, class P> constexpr auto collect(P p) {
	return collect_<T>(get_parser(p));
}
template <class T, class P> constexpr Tagged<T, P> tagged_(P p) {
	return Tagged<T, P>(p);
}
template <class T, class P> constexpr auto tagged(P p) {
	return tagged_<T>(get_parser(p));
}
constexpr Error_ error(const StringView& s) {
	return Error_(s);
}
constexpr Expect expect(const StringView& s) {
	return Expect(s);
}
template <class T> constexpr auto reference() {
	return Reference_<T>();
}

class IgnoreCallback {
public:
	constexpr IgnoreCallback() {}
	template <class... A> constexpr void push(A&&...) const {}
};

template <class T> class GetValueCallback {
	T& value;
public:
	constexpr GetValueCallback(T& value): value(value) {}
	template <class... A> void push(A&&... a) const {
		value = T(std::forward<A>(a)...);
	}
};

template <class T, class C> class MapCallback {
	const C& callback;
public:
	MapCallback(const C& callback): callback(callback) {}
	template <class... A> void push(A&&... a) const {
		T::map(callback, std::forward<A>(a)...);
	}
};

template <class T> class CollectCallback {
	T& collector;
public:
	constexpr CollectCallback(T& collector): collector(collector) {}
	template <class... A> void push(A&&... a) const {
		collector.push(std::forward<A>(a)...);
	}
};

template <class T, class C> class TaggedCallback {
	const C& callback;
public:
	TaggedCallback(const C& callback): callback(callback) {}
	template <class... A> void push(A&&... a) const {
		callback.push(std::forward<A>(a)..., Tag<T>());
	}
};

template <class F, class C> Result parse_impl(const CharClass<F>& p, Context& context, const C& callback) {
	if (context && p.f(*context)) {
		++context;
		return SUCCESS;
	}
	return FAILURE;
}

template <class C> Result parse_impl(const StringView& s, Context& context, const C& callback) {
	const SavePoint save_point = context.save();
	for (char c: s) {
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
	const Result result = parse_impl(p.head, context, callback);
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
	const Result result = parse_impl(p.head, context, callback);
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
		const Result result = parse_impl(p.p, context, callback);
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
	const Result result = parse_impl(p.p, context, callback);
	if (result == ERROR) {
		return ERROR;
	}
	if (result == FAILURE) {
		return SUCCESS;
	}
	context.restore(save_point);
	return FAILURE;
}

template <class P, class C> Result parse_impl(const Ignore<P>& p, Context& context, const C& callback) {
	return parse_impl(p.p, context, IgnoreCallback());
}

template <class P, class C> Result parse_impl(const ToString<P>& p, Context& context, const C& callback) {
	const SavePoint save_point = context.save();
	const Result result = parse_impl(p.p, context, callback);
	if (result == ERROR) {
		return ERROR;
	}
	if (result == SUCCESS) {
		callback.push(context - save_point);
	}
	return result;
}

template <class T, class P, class C> Result parse_impl(const Map<T, P>& p, Context& context, const C& callback) {
	return parse_impl(p.p, context, MapCallback<T, C>(callback));
}

template <class T, class P, class C> Result parse_impl(const Collect<T, P>& p, Context& context, const C& callback) {
	T collector;
	const Result result = parse_impl(p.p, context, CollectCallback<T>(collector));
	if (result == ERROR) {
		return ERROR;
	}
	if (result == SUCCESS) {
		collector.retrieve(callback);
	}
	return result;
}

template <class T, class P, class C> Result parse_impl(const Tagged<T, P>& p, Context& context, const C& callback) {
	return parse_impl(p.p, context, TaggedCallback<T, C>(callback));
}

template <class C> Result parse_impl(const Error_& p, Context& context, const C& callback) {
	context.set_error(p.s);
	return ERROR;
}

template <class C> Result parse_impl(const Expect& p, Context& context, const C& callback) {
	const Result result = parse_impl(get_parser(p.s), context, callback);
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
