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
	constexpr SavePoint save() const {
		return position;
	}
	void restore(SavePoint save_point) {
		position = save_point;
	}
	constexpr StringView operator -(SavePoint save_point) const {
		return StringView(save_point, position - save_point);
	}
	constexpr SourceLocation get_location() const {
		return SourceLocation(position - begin);
	}
	constexpr SourceLocation get_location(SavePoint save_point) const {
		return SourceLocation(save_point - begin, position - begin);
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

template <class P> class CollectLocation {
public:
	P p;
	constexpr CollectLocation(P p): p(p) {}
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

class IgnoreCallback {
public:
	constexpr IgnoreCallback() {}
	template <class... A> constexpr void push(A&&...) const {}
	constexpr void set_location(const SourceLocation&) const {}
	template <class C> constexpr void retrieve(const C& callback) const {}
	template <class C, class... A> static constexpr void map(const C& callback, A&&...) {}
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
	constexpr MapCallback(const C& callback): callback(callback) {}
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
	void set_location(const SourceLocation& location) const {
		collector.set_location(location);
	}
};

class IdentityMapper {
public:
	constexpr IdentityMapper() {}
	template <class C, class... A> static void map(const C& callback, A&&... a) {
		callback.push(std::forward<A>(a)...);
	}
};

template <class T> class TagMapper {
public:
	constexpr TagMapper() {}
	template <class C, class... A> static void map(const C& callback, A&&... a) {
		T tag;
		callback.push(std::forward<A>(a)..., tag);
	}
};

template <class F> constexpr CharClass<F> char_class(F f) {
	return CharClass<F>(f);
}

constexpr CharClass<AnyChar> any_char() {
	return CharClass<AnyChar>(AnyChar());
}
constexpr CharClass<CharRange> range(char first, char last) {
	return CharClass<CharRange>(CharRange(first, last));
}
template <class... P> constexpr Sequence<P...> sequence(P... p) {
	return Sequence<P...>(p...);
}
template <class... P> constexpr Choice<P...> choice(P... p) {
	return Choice<P...>(p...);
}
constexpr auto empty() {
	return sequence();
}
template <class P> constexpr Repetition<P> repetition(P p) {
	return Repetition<P>(p);
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
template <class P> constexpr Not<P> not_(P p) {
	return Not<P>(p);
}
template <class P> constexpr auto and_(P p) {
	return not_(not_(p));
}
constexpr auto end() {
	return not_(any_char());
}
template <class P> constexpr Ignore<P> ignore(P p) {
	return Ignore<P>(p);
}
template <class P> constexpr ToString<P> to_string(P p) {
	return ToString<P>(p);
}
template <class T, class P> constexpr Map<T, P> map(P p) {
	return Map<T, P>(p);
}
template <class T, class P> constexpr Collect<T, P> collect(P p) {
	return Collect<T, P>(p);
}
template <class P> constexpr CollectLocation<P> collect_location(P p) {
	return CollectLocation<P>(p);
}
template <class T, class P> constexpr auto tag(P p) {
	return map<TagMapper<T>>(p);
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

template <class F, class C> Result parse_impl(const CharClass<F>& p, Context& context, const C& callback) {
	if (context && p.f(*context)) {
		callback.push(*context);
		++context;
		return SUCCESS;
	}
	return FAILURE;
}

template <class C> Result parse_impl(char c, Context& context, const C& callback) {
	return parse_impl(CharClass<Char>(Char(c)), context, callback);
}

template <class C> Result parse_impl(bool (*f)(char), Context& context, const C& callback) {
	return parse_impl(CharClass<bool (*)(char)>(f), context, callback);
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
	callback.push(context - save_point);
	return SUCCESS;
}

template <class C> Result parse_impl(const char* s, Context& context, const C& callback) {
	return parse_impl(StringView(s), context, callback);
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
	return parse_impl(p.tail, context, callback, save_point);
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
	const Result result = parse_impl(p.p, context, IgnoreCallback());
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
	const Result result = parse_impl(p.p, context, IgnoreCallback());
	if (result == ERROR) {
		return ERROR;
	}
	if (result == FAILURE) {
		return FAILURE;
	}
	callback.push(context - save_point);
	return SUCCESS;
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
	if (result == FAILURE) {
		return FAILURE;
	}
	collector.retrieve(callback);
	return SUCCESS;
}

template <class P, class C> Result parse_impl(const CollectLocation<P>& p, Context& context, const C& callback) {
	const SavePoint save_point = context.save();
	const Result result = parse_impl(p.p, context, callback);
	if (result == ERROR) {
		return ERROR;
	}
	if (result == FAILURE) {
		return FAILURE;
	}
	callback.set_location(context.get_location(save_point));
	return SUCCESS;
}

template <class C> Result parse_impl(const Error_& p, Context& context, const C& callback) {
	context.set_error(p.s);
	return ERROR;
}

template <class C> Result parse_impl(const Expect& p, Context& context, const C& callback) {
	const Result result = parse_impl(p.s, context, IgnoreCallback());
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
