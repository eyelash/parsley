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

struct parse_comment {
	static constexpr auto parser = choice(
		sequence("//", zero_or_more(sequence(not_("\n"), any_char))),
		sequence("/*", zero_or_more(sequence(not_("*/"), any_char)), expect("*/"))
	);
};

struct parse_white_space {
	static constexpr auto parser = sequence(
		zero_or_more(white_space),
		zero_or_more(sequence(
			reference<parse_comment>(),
			zero_or_more(white_space)
		))
	);
};

struct parse_int_literal {
	static IntLiteral to_int_literal(const StringView& s) {
		std::int32_t number = 0;
		for (char c: s) {
			number *= 10;
			number += c - '0';
		}
		return IntLiteral(number);
	}
	static constexpr auto parser = map<to_int_literal>(sequence(numeric, zero_or_more(numeric)));
};

struct parse_expression;

struct parse_expression_last {
	static constexpr auto parser = to_error(choice(
		sequence(
			'(',
			reference<parse_white_space>(),
			reference<parse_expression, Expression>(),
			reference<parse_white_space>(),
			expect(")")
		),
		reference<parse_int_literal, Expression>()
	));
};

template <class P> constexpr auto operator_(P p) {
	return sequence(
		reference<parse_white_space>(),
		p,
		reference<parse_white_space>()
	);
}

struct parse_expression {
	static IntLiteral add(IntLiteral left, IntLiteral right) {
		return IntLiteral(left.get_value() + right.get_value());
	}
	static constexpr auto parser = operator_levels<parse_expression_last, Expression>(
		binary_left_to_right(
			binary_operator<add>(operator_('+'))
		)
	);
};

struct parse_program {
	static constexpr auto parser = sequence(
		reference<parse_white_space>(),
		reference<parse_expression, Expression>(),
		reference<parse_white_space>(),
		to_error(not_(any_char))
	);
};

Result<Expression> parse_program(const char* path) {
	SourceFile file(path);
	Context context(&file);
	return reference<struct parse_program, Expression>().parse(context);
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
