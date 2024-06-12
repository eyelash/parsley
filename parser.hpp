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

template <class... T> struct GetLastType;
template <class T> struct GetLastType<T> {
	using type = T;
};
template <class T0, class T1, class... T> struct GetLastType<T0, T1, T...> {
	using type = typename GetLastType<T1, T...>::type;
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
	template <class... A> Result(A&&... a): variant(Success<T>(std::forward<A>(a)...)) {}
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
struct SequenceImpl {
	template <class R, class... P, class... A> static R parse(Context& context, const Tuple<P...>& p, A&&... a) {
		if constexpr (sizeof...(P) == 0) {
			return R(std::forward<A>(a)...);
		}
		else {
			using T0 = decltype(p.head);
			auto result = p.head.parse(context);
			if (Error* error = result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = result.get_failure()) {
				return std::move(*failure);
			}
			if constexpr (is_basic<T0>::value) {
				return parse<R>(context, p.tail, std::forward<A>(a)...);
			}
			else {
				return parse<R>(context, p.tail, std::forward<A>(a)..., std::move(*result.get_success()));
			}
		}
	}
};
template <class... P> class Sequence {
	Tuple<P...> p;
public:
	constexpr Sequence(P... p): p(p...) {}
	using R = typename SequenceResult<Tuple<P...>>::type;
	R parse(Context& context) const {
		return SequenceImpl::parse<R>(context, p);
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
struct ChoiceImpl {
	template <class R, class... P> static R parse(Context& context, const Tuple<P...>& p) {
		if constexpr (sizeof...(P) == 0) {
			return Failure();
		}
		else {
			auto result = p.head.parse(context);
			if (Error* error = result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = result.get_failure()) {
				return parse<R>(context, p.tail);
			}
			return std::move(*result.get_success());
		}
	}
};
template <class... P> class Choice {
	Tuple<P...> p;
public:
	constexpr Choice(P... p): p(p...) {}
	using R = typename ChoiceResult<Tuple<P...>>::type;
	R parse(Context& context) const {
		return ChoiceImpl::parse<R>(context, p);
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

template <class... T> struct BinaryLeftToRight {
	Tuple<T...> t;
	constexpr BinaryLeftToRight(T... t): t(t...) {}
};

template <class... T> struct BinaryRightToLeft {
	Tuple<T...> t;
	constexpr BinaryRightToLeft(T... t): t(t...) {}
};

template <class... T> struct UnaryPrefix {
	Tuple<T...> t;
	constexpr UnaryPrefix(T... t): t(t...) {}
};

template <class... T> struct UnaryPostfix {
	Tuple<T...> t;
	constexpr UnaryPostfix(T... t): t(t...) {}
};

struct OperatorLevelsImpl {
	template <class R> static Result<R> parse_operator(Context& context, const Tuple<>& t) {
		return Failure();
	}
	template <class R, class T0, class... T> static Result<R> parse_operator(Context& context, const Tuple<T0, T...>& t) {
		auto result = t.head.p.parse(context);
		if (Error* error = result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = result.get_failure()) {
			return parse_operator<R>(context, t.tail);
		}
		return T0::create;
	}
	template <class R, class T> static Result<R> parse(Context& context, const Tuple<T>& t) {
		return t.head.parse(context);
	}
	template <class R, class T0, class... T> static Result<R> parse(Context& context, const Tuple<BinaryLeftToRight<T0>, T...>& t) {
		Result<R> left_result = parse<R>(context, t.tail);
		if (Error* error = left_result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = left_result.get_failure()) {
			return std::move(*failure);
		}
		R left = std::move(*left_result.get_success());
		while (true) {
			Result<R (*)(R, R)> operator_result = parse_operator<R (*)(R, R)>(context, t.head.t);
			if (Error* error = operator_result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = operator_result.get_failure()) {
				break;
			}
			R (*create)(R, R) = *operator_result.get_success();
			Result<R> right_result = parse<R>(context, t.tail);
			if (Error* error = right_result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = right_result.get_failure()) {
				return std::move(*failure);
			}
			R right = std::move(*right_result.get_success());
			left = create(std::move(left), std::move(right));
		}
		return std::move(left);
	}
	template <class R, class T0, class... T> static Result<R> parse(Context& context, const Tuple<BinaryRightToLeft<T0>, T...>& t) {
		Result<R> left_result = parse<R>(context, t.tail);
		if (Error* error = left_result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = left_result.get_failure()) {
			return std::move(*failure);
		}
		R left = std::move(*left_result.get_success());
		Result<R (*)(R, R)> operator_result = parse_operator<R (*)(R, R)>(context, t.head.t);
		if (Error* error = operator_result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = operator_result.get_failure()) {
			return std::move(left);
		}
		R (*create)(R, R) = *operator_result.get_success();
		Result<R> right_result = parse<R>(context, t);
		if (Error* error = right_result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = right_result.get_failure()) {
			return std::move(*failure);
		}
		R right = std::move(*right_result.get_success());
		return create(std::move(left), std::move(right));
	}
	template <class R, class T0, class... T> static Result<R> parse(Context& context, const Tuple<UnaryPrefix<T0>, T...>& t) {
		Result<R (*)(R)> operator_result = parse_operator<R (*)(R)>(context, t.head.t);
		if (Error* error = operator_result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = operator_result.get_failure()) {
			return parse<R>(context, t.tail);
		}
		R (*create)(R) = *operator_result.get_success();
		Result<R> result = parse<R>(context, t);
		if (Error* error = result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = result.get_failure()) {
			return std::move(*failure);
		}
		return create(*result.get_success());
	}
	template <class R, class T0, class... T> static Result<R> parse(Context& context, const Tuple<UnaryPostfix<T0>, T...>& t) {
		Result<R> result = parse<R>(context, t.tail);
		if (Error* error = result.get_error()) {
			return std::move(*error);
		}
		if (Failure* failure = result.get_failure()) {
			return std::move(*failure);
		}
		R left = std::move(*result.get_success());
		while (true) {
			Result<R (*)(R)> operator_result = parse_operator<R (*)(R)>(context, t.head.t);
			if (Error* error = operator_result.get_error()) {
				return std::move(*error);
			}
			if (Failure* failure = operator_result.get_failure()) {
				break;
			}
			R (*create)(R) = *operator_result.get_success();
			left = create(std::move(left));
		}
		return std::move(left);
	}
};
template <class... T> struct OperatorLevels {
	Tuple<T...> t;
	constexpr OperatorLevels(T... t): t(t...) {}
	using R = get_success_type<typename GetLastType<T...>::type>;
	Result<R> parse(Context& context) const {
		return OperatorLevelsImpl::parse<R>(context, t);
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
	return BinaryLeftToRight(t...);
}
template <class... T> constexpr auto binary_right_to_left(T... t) {
	return BinaryRightToLeft(t...);
}
template <class... T> constexpr auto unary_prefix(T... t) {
	return UnaryPrefix(t...);
}
template <class... T> constexpr auto unary_postfix(T... t) {
	return UnaryPostfix(t...);
}
template <class... T> constexpr auto operator_levels(T... t) {
	return OperatorLevels(get_parser(t)...);
}

}
