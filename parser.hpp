#pragma once

#include "common.hpp"
#include "printer.hpp"
#include <vector>

template <class U, class... T> struct ContainsType;
template <class U> struct ContainsType<U>: std::false_type {};
template <class T0, class... T> struct ContainsType<T0, T0, T...>: std::true_type {};
template <class U, class T0, class... T> struct ContainsType<U, T0, T...> {
	static constexpr bool value = ContainsType<U, T...>::value;
};

namespace parser {

class Context {
	const SourceFile* file;
	const char* position;
public:
	Context(const SourceFile* file): file(file), position(file->begin()) {}
	constexpr Context(const SourceFile* file, const char* position): file(file), position(position) {}
	operator bool() const {
		return position < file->end();
	}
	constexpr bool operator <(const Context& rhs) const {
		return position < rhs.position;
	}
	constexpr char operator *() const {
		return *position;
	}
	Context& operator ++() {
		++position;
		return *this;
	}
	constexpr StringView operator -(const Context& start) const {
		return StringView(start.position, position - start.position);
	}
	const char* get_path() const {
		return file->get_path();
	}
	std::size_t get_position() const {
		return position - file->begin();
	}
};

class Empty {
public:
	constexpr Empty() {}
};
template <class T> class Success {
public:
	T value;
	template <class... A> constexpr Success(A&&... a): value(std::forward<A>(a)...) {}
	Success(const Success& success): value(success.value) {}
	Success(Success&& success): value(std::move(success.value)) {}
};
class Failure {
public:
	constexpr Failure() {}
};
template <class T> class Result {
	Variant<Success<T>, Failure, Error> variant;
public:
	template <class... A> Result(A&&... a): variant(TypeTag<Success<T>>(), std::forward<A>(a)...) {}
	Result(Failure&& failure): variant(std::move(failure)) {}
	Result(Error&& error): variant(std::move(error)) {}
	using type = T;
	T* get_success() {
		Success<T>* success = get<Success<T>>(variant);
		return success ? &success->value : nullptr;
	}
	Failure* get_failure() {
		return get<Failure>(variant);
	}
	Error* get_error() {
		return get<Error>(variant);
	}
};

template <class T, class M> struct GetResultType;
template <class T, class R> struct GetResultType<T, R (T::*)(Context&) const> {
	using type = R;
};
template <class T> using get_result_type = typename GetResultType<T, decltype(&T::parse)>::type;

template <class T> using get_success_type = typename get_result_type<T>::type;

using BasicResult = Result<Empty>;
template <class... T> using is_basic = std::conjunction<std::is_same<get_result_type<T>, BasicResult>...>;

template <class F> class Char {
	F f;
public:
	constexpr Char(F f): f(f) {}
	BasicResult parse(Context& context) const {
		if (context && f(*context)) {
			++context;
			return BasicResult();
		}
		return Failure();
	}
};

class String {
	StringView s;
public:
	constexpr String(const StringView& s): s(s) {}
	BasicResult parse(Context& context) const {
		const Context start = context;
		for (char c: s) {
			if (!(context && *context == c)) {
				context = start;
				return Failure();
			}
			++context;
		}
		return BasicResult();
	}
};

template <class T, class... A> struct SequenceResult;
template <> struct SequenceResult<Tuple<>> {
	using type = BasicResult;
};
template <class A> struct SequenceResult<Tuple<>, A> {
	using type = Result<A>;
};
template <class... A> struct SequenceResult<Tuple<>, A...> {
	using type = Result<Tuple<A...>>;
};
template <class T0, class... T, class... A> struct SequenceResult<Tuple<T0, T...>, A...> {
	using type = std::conditional_t<
		is_basic<T0>::value,
		typename SequenceResult<Tuple<T...>, A...>::type,
		typename SequenceResult<Tuple<T...>, A..., get_success_type<T0>>::type
	>;
};
template <class... P> class Sequence {
	Tuple<P...> p;
public:
	constexpr Sequence(P... p): p(p...) {}
	using R = typename SequenceResult<Tuple<P...>>::type;
	template <std::size_t I, class... A> R parse_impl(Context& context, A&&... a) const {
		if constexpr (I == sizeof...(P)) {
			return R(std::forward<A>(a)...);
		}
		else {
			auto result = get<I>(p).parse(context);
			if (Error* error = result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = result.get_failure()) {
				return std::move(*failure);
			}
			if constexpr (std::is_same<decltype(result), BasicResult>::value) {
				return parse_impl<I + 1>(context, std::forward<A>(a)...);
			}
			else {
				return parse_impl<I + 1>(context, std::forward<A>(a)..., std::move(*result.get_success()));
			}
		}
	}
	R parse(Context& context) const {
		return parse_impl<0>(context);
	}
};

template <class T, class... A> struct ChoiceResult;
template <> struct ChoiceResult<Tuple<>> {
	using type = BasicResult;
};
template <class A> struct ChoiceResult<Tuple<>, A> {
	using type = Result<A>;
};
template <class... A> struct ChoiceResult<Tuple<>, A...> {
	using type = Result<Variant<A...>>;
};
template <class T0, class... T, class... A> struct ChoiceResult<Tuple<T0, T...>, A...> {
	using type = std::conditional_t<
		ContainsType<get_success_type<T0>, A...>::value,
		typename ChoiceResult<Tuple<T...>, A...>::type,
		typename ChoiceResult<Tuple<T...>, A..., get_success_type<T0>>::type
	>;
};
template <class... P> class Choice {
	Tuple<P...> p;
public:
	constexpr Choice(P... p): p(p...) {}
	using R = typename ChoiceResult<Tuple<P...>>::type;
	template <std::size_t I> R parse_impl(Context& context) const {
		if constexpr (I == sizeof...(P)) {
			return Failure();
		}
		else {
			auto result = get<I>(p).parse(context);
			if (Error* error = result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = result.get_failure()) {
				return parse_impl<I + 1>(context);
			}
			return std::move(*result.get_success());
		}
	}
	R parse(Context& context) const {
		return parse_impl<0>(context);
	}
};

template <class P> class Repetition {
	P p;
public:
	constexpr Repetition(P p): p(p) {}
	using R = std::conditional_t<is_basic<P>::value, BasicResult, Result<std::vector<get_success_type<P>>>>;
	R parse(Context& context) const {
		typename R::type vector;
		while (true) {
			auto result = p.parse(context);
			if (Error* error = result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = result.get_failure()) {
				break;
			}
			if constexpr (!is_basic<P>::value) {
				vector.push_back(std::move(*result.get_success()));
			}
		}
		return std::move(vector);
	}
};

template <class P> class Not {
	P p;
public:
	constexpr Not(P p): p(p) {}
	BasicResult parse(Context& context) const {
		const Context start = context;
		auto result = p.parse(context);
		if (Error* error = result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = result.get_failure()) {
			return BasicResult();
		}
		context = start;
		return Failure();
	}
};

template <class P> class Peek {
	P p;
public:
	constexpr Peek(P p): p(p) {}
	BasicResult parse(Context& context) const {
		const Context start = context;
		auto result = p.parse(context);
		if (Error* error = result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = result.get_failure()) {
			return Failure();
		}
		context = start;
		return BasicResult();
	}
};

template <class P, auto F> class Map;
template <class P, class R, class A, R (*F)(A)> class Map<P, F> {
	P p;
public:
	constexpr Map(P p): p(p) {}
	Result<R> parse(Context& context) const {
		if constexpr (is_basic<P>::value) {
			const Context start = context;
			auto result = p.parse(context);
			if (Error* error = result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = result.get_failure()) {
				return std::move(*failure);
			}
			return F(context - start);
		}
		else {
			auto result = p.parse(context);
			if (Error* error = result.get_error()) {
				return *error;
			}
			if (Failure* failure = result.get_failure()) {
				return std::move(*failure);
			}
			return F(*result.get_success());
		}
	}
};

template <class R> class Error_ {
	StringView s;
public:
	constexpr Error_(const StringView& s): s(s) {}
	Result<R> parse(Context& context) const {
		return Error(context.get_path(), context.get_position(), s);
	}
};

class Expect {
	StringView s;
public:
	constexpr Expect(const StringView& s): s(s) {}
	BasicResult parse(Context& context) const {
		auto result = String(s).parse(context);
		if (Error* error = result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = result.get_failure()) {
			return Error(context.get_path(), context.get_position(), format("expected \"%\"", s));
		}
		return BasicResult();
	}
};

template <class T, class R> class Reference {
public:
	constexpr Reference() {}
	Result<R> parse(Context& context) const {
		return T::parser.parse(context);
	}
};

template <class R> class Parser {
	class Interface {
	public:
		virtual ~Interface() = default;
		virtual Result<R> parse(Context& context) const = 0;
	};
	template <class P> class Implementation final: public Interface {
		P p;
	public:
		constexpr Implementation(P p): p(p) {}
		Result<R> parse(Context& context) const override {
			return p.parse(context);
		}
	};
	::Reference<Interface> p;
public:
	Parser() {}
	template <class P> Parser(P p): p(new Implementation<P>(p)) {}
	Result<R> parse(Context& context) const {
		return p->parse(context);
	}
};
using BasicParser = Parser<BasicResult::type>;

template <class P> class Pointer {
	P* p;
public:
	constexpr Pointer(P* p): p(p) {}
	get_result_type<P> parse(Context& context) const {
		return p->parse(context);
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
template <class P> constexpr auto get_parser(Parser<P>* p) {
	return Pointer(p);
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
template <auto F, class P> constexpr Map<P, F> map_(P p) {
	return Map<P, F>(p);
}
template <auto F, class P> constexpr auto map(P p) {
	return map_<F>(get_parser(p));
}
template <class R = BasicResult::type> constexpr Error_<R> error(const StringView& s) {
	return Error_<R>(s);
}
constexpr Expect expect(const StringView& s) {
	return Expect(s);
}
template <class T, class R = BasicResult::type> constexpr auto reference() {
	return Reference<T, R>();
}

template <class P, auto F> struct InfixLTR;
template <class P, class A, A (*F)(A, A)> struct InfixLTR<P, F> {
	P p;
	constexpr InfixLTR(P p): p(p) {}
	template <std::size_t level, class O> Result<A> parse_left(Context& context, const O& levels) const {
		return Failure();
	}
	template <std::size_t level, class O> Result<A> parse_right(Context& context, A& left, const O& levels) const {
		auto result = p.parse(context);
		if (Error* error = result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = result.get_failure()) {
			return std::move(*failure);
		}
		auto right_result = levels.template parse_level<level + 1>(context);
		if (Error* error = right_result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = right_result.get_failure()) {
			return std::move(*failure);
		}
		A right = std::move(*right_result.get_success());
		return F(std::move(left), std::move(right));
	}
};

template <class P, auto F> struct InfixRTL;
template <class P, class A, A (*F)(A, A)> struct InfixRTL<P, F> {
	P p;
	constexpr InfixRTL(P p): p(p) {}
	template <std::size_t level, class O> Result<A> parse_left(Context& context, const O& levels) const {
		return Failure();
	}
	template <std::size_t level, class O> Result<A> parse_right(Context& context, A& left, const O& levels) const {
		auto result = p.parse(context);
		if (Error* error = result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = result.get_failure()) {
			return std::move(*failure);
		}
		auto right_result = levels.template parse_level<level>(context);
		if (Error* error = right_result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = right_result.get_failure()) {
			return std::move(*failure);
		}
		A right = std::move(*right_result.get_success());
		return F(std::move(left), std::move(right));
	}
};

template <class... T> struct OperatorLevel {
	Tuple<T...> t;
	constexpr OperatorLevel(T... t): t(t...) {}
	static constexpr std::size_t size() {
		return sizeof...(T);
	}
};

template <class... T> struct OperatorLevels {
	Tuple<T...> t;
	constexpr OperatorLevels(T... t): t(t...) {}
	using R = get_success_type<typename IndexToType<sizeof...(T) - 1, T...>::type>;
	template <std::size_t level, std::size_t L = 0, std::size_t I = 0> Result<R> parse_left(Context& context) const {
		if constexpr (L == sizeof...(T) - 1) {
			Result<R> result = get<L>(t).parse(context);
			if (Error* error = result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = result.get_failure()) {
				return std::move(*failure);
			}
			R left = std::move(*result.get_success());
			return parse_right<level>(context, left);
		}
		else if constexpr (I == IndexToType<L, T...>::type::size()) {
			return parse_left<level, L + 1, 0>(context);
		}
		else {
			Result<R> result = get<I>(get<L>(t).t).template parse_left<L>(context, *this);
			if (Error* error = result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = result.get_failure()) {
				return parse_left<level, L, I + 1>(context);
			}
			R left = std::move(*result.get_success());
			return parse_right<level>(context, left);
		}
	}
	template <std::size_t level, std::size_t L = level, std::size_t I = 0> Result<R> parse_right(Context& context, R& left) const {
		if constexpr (L == sizeof...(T) - 1) {
			return std::move(left);
		}
		else if constexpr (I == IndexToType<L, T...>::type::size()) {
			return parse_right<level, L + 1, 0>(context, left);
		}
		else {
			Result<R> result = get<I>(get<L>(t).t).template parse_right<L>(context, left, *this);
			if (Error* error = result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = result.get_failure()) {
				return parse_right<level, L, I + 1>(context, left);
			}
			left = std::move(*result.get_success());
			return parse_right<level>(context, left);
		}
	}
	template <std::size_t level> Result<R> parse_level(Context& context) const {
		return parse_left<level>(context);
	}
	Result<R> parse(Context& context) const {
		return parse_level<0>(context);
	}
};

template <auto F, class P> constexpr InfixLTR<P, F> infix_ltr_(P p) {
	return InfixLTR<P, F>(p);
}
template <auto F, class P> constexpr auto infix_ltr(P p) {
	return infix_ltr_<F>(get_parser(p));
}
template <auto F, class P> constexpr InfixRTL<P, F> infix_rtl_(P p) {
	return InfixRTL<P, F>(p);
}
template <auto F, class P> constexpr auto infix_rtl(P p) {
	return infix_rtl_<F>(get_parser(p));
}
template <class... T> constexpr auto operator_level(T... t) {
	return OperatorLevel(t...);
}
template <class... T> constexpr auto operator_levels(T... t) {
	return OperatorLevels(get_parser(t)...);
}

}
