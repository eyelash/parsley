#include "parser.hpp"
#include "printer.hpp"

static constexpr bool any_char(char c) {
	return true;
}
static constexpr bool white_space_char(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
static constexpr bool numeric_char(char c) {
	return c >= '0' && c <= '9';
}
static constexpr bool alphabetic_char(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static constexpr bool alphanumeric_char(char c) {
	return alphabetic_char(c) || numeric_char(c);
}

struct MoebiusParser {
	static constexpr auto comment = choice(
		sequence("//", zero_or_more(sequence(not_("\n"), any_char))),
		sequence("/*", zero_or_more(sequence(not_("*/"), any_char)), expect("*/"))
	);
	struct WhiteSpace {
		static constexpr auto parser = sequence(
			zero_or_more(white_space_char),
			zero_or_more(sequence(
				comment,
				zero_or_more(white_space_char)
			))
		);
	};
	static constexpr auto white_space = reference<WhiteSpace>();

	static constexpr auto number = one_or_more(numeric_char);
	static constexpr auto identifier = sequence(alphabetic_char, zero_or_more(alphanumeric_char));

	struct Expression;
	static constexpr auto expression = reference<Expression>();
	struct Expression {
		static constexpr auto let = sequence(
			"let",
			white_space,
			choice(identifier, error("expected an identifier")),
			white_space,
			expect("="),
			white_space,
			expression,
			white_space,
			expect(";"),
			white_space,
			expression
		);
		static constexpr auto last = choice(
			sequence('(', white_space, expression, white_space, expect(")")),
			let,
			number,
			identifier,
			error("expected an expression")
		);
		static constexpr auto parser = sequence(
			last,
			white_space,
			zero_or_more(sequence(
				'+',
				white_space,
				last,
				white_space
			))
		);
	};

	static constexpr auto program = sequence(
		expression,
		choice(not_(any_char), error("unexpected character at end of program"))
	);
};

struct Numbers {
	static constexpr auto white_space = zero_or_more(white_space_char);

	struct Result {
		int number;
		Result() {}
		Result(int number): number(number) {}
		Result(const StringView& s) {
			number = 0;
			for (char c: s) {
				number = number * 10 + (c - '0');
			}
		}
		void print(PrintContext& context) const {
			print_number(number).print(context);
		}
	};
	struct Multiply {
		int number = 1;
		void push(Result r) {
			number *= r.number;
		}
		template <class C> void retrieve(const C& callback) {
			callback.push(Result(number));
		}
	};
	struct Add {
		int number = 0;
		void push(Result r) {
			number += r.number;
		}
		template <class C> void retrieve(const C& callback) {
			callback.push(Result(number));
		}
	};

	static constexpr auto number = cast<Result>(to_string_view(one_or_more(numeric_char)));
	//static constexpr auto identifier = sequence(alphabetic_char, zero_or_more(alphanumeric_char));
	
	struct Expression;
	static constexpr auto expression = reference<Expression>();
	struct Expression {
		static constexpr auto last = choice(
			sequence('(', white_space, expression, white_space, expect(")")),
			number,
			error("expected an expression")
		);
		static constexpr auto multiplication = collect<Multiply>(sequence(
			last,
			white_space,
			zero_or_more(sequence(
				'*',
				white_space,
				last,
				white_space
			))
		));
		static constexpr auto addition = collect<Add>(sequence(
			multiplication,
			white_space,
			zero_or_more(sequence(
				'+',
				white_space,
				multiplication,
				white_space
			))
		));
		static constexpr auto parser = addition;
	};
};

template <class P> static void test(P p, const StringView& s) {
	Context context(s);
	const auto result = parse(p, context);
	if (result) {
		print(ln(green("success")));
		print(ln(format("at position %", print_number(context.get_position()))));
	}
	else {
		if (context.has_error()) {
			print(ln(format("% %", red("error:"), context.get_error())));
			print(ln(format("at position %", print_number(context.get_position()))));
		}
		else {
			print(ln(yellow("failure")));
			print(ln(format("at position %", print_number(context.get_position()))));
		}
	}
}

template <class C> class LambdaCallback {
	const C& callback;
public:
	LambdaCallback(const C& callback): callback(callback) {}
	template <class A> void push(A&& a) const {
		callback(std::forward<A>(a));
	}
};

template <class T> class GetValueCallback {
	T& value;
public:
	GetValueCallback(T& value): value(value) {}
	template <class A> void push(A&& a) const {
		value = T(std::forward<A>(a));
	}
};

template <class P, class C> static void test2_impl(P p, const StringView& s, const C& callback) {
	Context context(s);
	const Result result = parse2(p, context, callback);
	if (result == MATCH) {
		print(ln(green("match")));
	}
	else if (result == ERROR) {
		print(ln(format("% %", red("error:"), context.get_error())));
	}
	else {
		print(ln(yellow("no match")));
	}
	print(ln(format("at position %", print_number(context.get_position()))));
}
template <class P> static void test2(P p, const StringView& s) {
	test2_impl(p, s, IgnoreCallback());
}
template <class P, class C> static void test2(P p, const StringView& s, const C& callback) {
	test2_impl(p, s, LambdaCallback<C>(callback));
}
template <class T, class P> static void test2(P p, const StringView& s) {
	T result;
	test2_impl(p, s, GetValueCallback<T>(result));
	print(ln(format("result: %", result)));
}

int main(int argc, char** argv) {
	test2<Numbers::Result>(Numbers::expression, "( 123+2 ) * 2");
}
