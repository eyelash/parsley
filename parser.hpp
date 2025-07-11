#pragma once

#include "common.hpp"
#include "printer.hpp"

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
class Error {
public:
	const char* path;
	std::size_t source_position;
	std::string message;
	template <class P> Error(const char* path, std::size_t source_position, P&& p): path(path), source_position(source_position), message(print_to_string(std::forward<P>(p))) {}
};
template <class T> class Result {
	Variant<Success<T>, Failure, Error> variant;
public:
	template <class... A> Result(A&&... a): variant(Type<Success<T>>(), std::forward<A>(a)...) {}
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
				return std::move(*error);
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
			return Error(context.get_path(), context.get_position(), printer::format("expected \"%\"", s));
		}
		return BasicResult();
	}
};

template <class P, class R> class Cast {
	P p;
public:
	constexpr Cast(P p): p(p) {}
	Result<R> parse(Context& context) const {
		return p.parse(context);
	}
};

template <class T> class Reference_ {
public:
	constexpr Reference_() {}
	auto parse(Context& context) const {
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
	Reference<Interface> p;
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
template <class R, class P> constexpr Cast<P, R> cast_(P p) {
	return Cast<P, R>(p);
}
template <class R, class P> constexpr auto cast(P p) {
	return cast_<R>(get_parser(p));
}
template <class T> constexpr auto reference() {
	return Reference_<T>();
}

template <class P, auto F> struct BinaryOperator;
template <class P, class A, A (*F)(A, A)> struct BinaryOperator<P, F> {
	P p;
	constexpr BinaryOperator(P p): p(p) {}
	static A create(A left, A right) {
		return F(std::move(left), std::move(right));
	}
};

template <class P, auto F> struct UnaryOperator;
template <class P, class A, A (*F)(A)> struct UnaryOperator<P, F> {
	P p;
	constexpr UnaryOperator(P p): p(p) {}
	static A create(A a) {
		return F(std::move(a));
	}
};

template <class... T> struct LeftToRight {
	Tuple<T...> t;
	constexpr LeftToRight(T... t): t(t...) {}
	template <std::size_t I = 0, class O> Result<typename O::R> parse_operator(Context& context, typename O::R& left, const O& next_level) const {
		using R = typename O::R;
		if constexpr (I == sizeof...(T)) {
			return std::move(left);
		}
		else {
			using T0 = typename IndexToType<I, T...>::type;
			BasicResult operator_result = get<I>(t).p.parse(context);
			if (Error* error = operator_result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = operator_result.get_failure()) {
				return parse_operator<I + 1>(context, left, next_level);
			}
			if constexpr (std::is_same<decltype(T0::create), R(R, R)>::value) {
				Result<R> right_result = next_level.parse(context);
				if (Error* error = right_result.get_error()) {
					return std::move(*error);
				}
				if (Failure* failure = right_result.get_failure()) {
					return std::move(*failure);
				}
				R right = std::move(*right_result.get_success());
				left = T0::create(std::move(left), std::move(right));
			}
			else {
				left = T0::create(std::move(left));
			}
			return parse_operator(context, left, next_level);
		}
	}
	template <class O> Result<typename O::R> parse(Context& context, const O& next_level) const {
		using R = typename O::R;
		Result<R> left_result = next_level.parse(context);
		if (Error* error = left_result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = left_result.get_failure()) {
			return std::move(*failure);
		}
		R left = std::move(*left_result.get_success());
		return parse_operator(context, left, next_level);
	}
};

template <class... T> struct RightToLeft {
	Tuple<T...> t;
	constexpr RightToLeft(T... t): t(t...) {}
	template <std::size_t I = 0, class O> Result<typename O::R> parse_binary_operator(Context& context, typename O::R& left, const O& next_level) const {
		using R = typename O::R;
		if constexpr (I == sizeof...(T)) {
			return std::move(left);
		}
		else {
			using T0 = typename IndexToType<I, T...>::type;
			if constexpr (std::is_same<decltype(T0::create), R(R)>::value) {
				// skip unary operators
				return parse_binary_operator<I + 1>(context, left, next_level);
			}
			else {
				BasicResult operator_result = get<I>(t).p.parse(context);
				if (Error* error = operator_result.get_error()) {
					return std::move(*error);
				}
				if (Failure* failure = operator_result.get_failure()) {
					return parse_binary_operator<I + 1>(context, left, next_level);
				}
				Result<R> right_result = parse(context, next_level);
				if (Error* error = right_result.get_error()) {
					return std::move(*error);
				}
				if (Failure* failure = right_result.get_failure()) {
					return std::move(*failure);
				}
				R right = std::move(*right_result.get_success());
				return T0::create(std::move(left), std::move(right));
			}
		}
	}
	template <std::size_t I = 0, class O> Result<typename O::R> parse_unary_operator(Context& context, const O& next_level) const {
		using R = typename O::R;
		if constexpr (I == sizeof...(T)) {
			Result<R> left_result = next_level.parse(context);
			if (Error* error = left_result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = left_result.get_failure()) {
				return std::move(*failure);
			}
			R left = std::move(*left_result.get_success());
			return parse_binary_operator(context, left, next_level);
		}
		else {
			using T0 = typename IndexToType<I, T...>::type;
			if constexpr (std::is_same<decltype(T0::create), R(R, R)>::value) {
				// skip binary operators
				return parse_unary_operator<I + 1>(context, next_level);
			}
			else {
				BasicResult operator_result = get<I>(t).p.parse(context);
				if (Error* error = operator_result.get_error()) {
					return std::move(*error);
				}
				if (Failure* failure = operator_result.get_failure()) {
					return parse_unary_operator<I + 1>(context, next_level);
				}
				Result<R> right_result = parse(context, next_level);
				if (Error* error = right_result.get_error()) {
					return std::move(*error);
				}
				if (Failure* failure = right_result.get_failure()) {
					return std::move(*failure);
				}
				R right = std::move(*right_result.get_success());
				return T0::create(std::move(right));
			}
		}
	}
	template <class O> Result<typename O::R> parse(Context& context, const O& next_level) const {
		return parse_unary_operator(context, next_level);
	}
};

template <class O, std::size_t L = 0> class OperatorLevelsParser {
	const O& levels;
public:
	constexpr OperatorLevelsParser(const O& levels): levels(levels) {}
	using R = typename O::R;
	Result<R> parse(Context& context) const {
		return levels.template parse_level<L>(context);
	}
};

template <class... T> struct OperatorLevels {
	Tuple<T...> t;
	constexpr OperatorLevels(T... t): t(t...) {}
	using R = get_success_type<typename IndexToType<sizeof...(T) - 1, T...>::type>;
	template <std::size_t level> Result<R> parse_level(Context& context) const {
		if constexpr (level == sizeof...(T) - 1) {
			return get<level>(t).parse(context);
		}
		else {
			return get<level>(t).parse(context, OperatorLevelsParser<OperatorLevels, level + 1>(*this));
		}
	}
	Result<R> parse(Context& context) const {
		return parse_level<0>(context);
	}
};

template <auto F, class P> constexpr BinaryOperator<P, F> binary_operator_(P p) {
	return BinaryOperator<P, F>(p);
}
template <auto F, class P> constexpr auto binary_operator(P p) {
	return binary_operator_<F>(get_parser(p));
}
template <auto F, class P> constexpr UnaryOperator<P, F> unary_operator_(P p) {
	return UnaryOperator<P, F>(p);
}
template <auto F, class P> constexpr auto unary_operator(P p) {
	return unary_operator_<F>(get_parser(p));
}
template <class... T> constexpr auto binary_left_to_right(T... t) {
	return LeftToRight(t...);
}
template <class... T> constexpr auto binary_right_to_left(T... t) {
	return RightToLeft(t...);
}
template <class... T> constexpr auto unary_prefix(T... t) {
	return RightToLeft(t...);
}
template <class... T> constexpr auto unary_postfix(T... t) {
	return LeftToRight(t...);
}
template <class... T> constexpr auto left_to_right(T... t) {
	return LeftToRight(t...);
}
template <class... T> constexpr auto right_to_left(T... t) {
	return RightToLeft(t...);
}
template <class... T> constexpr auto operator_levels(T... t) {
	return OperatorLevels(get_parser(t)...);
}

}
