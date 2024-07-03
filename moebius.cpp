#include "parser.hpp"
#include <fstream>
#include <iostream>

/*-------*\
 |  AST  |
\*-------*/

class IntLiteral {
	std::int32_t value;
public:
	IntLiteral(std::int32_t value): value(value) {}
	std::int32_t get_value() const {
		return value;
	}
};

using Expression = IntLiteral;

/*----------*\
 |  PARSER  |
\*----------*/

using namespace parser;

constexpr bool any_char(char c) {
	return true;
}

constexpr bool white_space(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

constexpr bool numeric(char c) {
	return c >= '0' && c <= '9';
}

constexpr bool alphabetic(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

constexpr bool alphanumeric(char c) {
	return alphabetic(c) || numeric(c);
}

static constexpr auto parse_comment = choice(
	sequence("//", zero_or_more(sequence(not_("\n"), any_char))),
	sequence("/*", zero_or_more(sequence(not_("*/"), any_char)), expect("*/"))
);

static constexpr auto parse_white_space = sequence(
	zero_or_more(white_space),
	zero_or_more(sequence(
		parse_comment,
		zero_or_more(white_space)
	))
);

template <class P> constexpr auto operator_(P p) {
	return sequence(
		parse_white_space,
		p,
		parse_white_space
	);
}

class MoebiusParser {
	static IntLiteral to_int_literal(const StringView& s) {
		std::int32_t number = 0;
		for (char c: s) {
			number *= 10;
			number += c - '0';
		}
		return IntLiteral(number);
	}
	Parser<Expression> int_literal = map<to_int_literal>(sequence(numeric, zero_or_more(numeric)));
	Parser<Expression> expression_last = choice(
		sequence(
			'(',
			parse_white_space,
			&expression,
			parse_white_space,
			expect(")")
		),
		&int_literal,
		error<Expression>("expected an expression")
	);
	static IntLiteral add(IntLiteral left, IntLiteral right) {
		return IntLiteral(left.get_value() + right.get_value());
	}
	Parser<Expression> expression = operator_levels(
		operator_level(
			infix_ltr<add>(operator_('+'))
		),
		&expression_last
	);
	Parser<Expression> program = sequence(
		parse_white_space,
		&expression,
		parse_white_space,
		choice(not_(any_char), error("unexpected character at end of program"))
	);
public:
	Result<Expression> parse(Context& context) const {
		return program.parse(context);
	}
};

Result<Expression> parse_program(const char* path) {
	static MoebiusParser parser;
	SourceFile file(path);
	Context context(&file);
	return parser.parse(context);
}

/*---------------*\
 |  INTERPRETER  |
\*---------------*/

void interpret_program(const Expression& program) {
	const std::int32_t result = program.get_value();
	print(ln(print_number(result)));
}

int main(int argc, char** argv) {
	if (argc > 1) {
		auto result = parse_program(argv[1]);
		if (result.get_failure()) {
			print(std::cerr, ln("Failure"));
			return 1;
		}
		else if (Error* error = result.get_error()) {
			print(std::cerr, ErrorPrinter(error));
			return 1;
		}
		std::cout << "Success\n";
		interpret_program(*result.get_success());
	}
}
