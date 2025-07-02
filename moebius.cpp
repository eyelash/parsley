#include "parser.hpp"
#include "printer.hpp"

struct MoebiusParser {
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

int main(int argc, char** argv) {
	test(MoebiusParser::program, "let x = 123; x + x");
}
