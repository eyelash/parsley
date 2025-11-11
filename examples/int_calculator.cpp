#include "../parser.hpp"
#include "../pratt.hpp"
#include "../printer.hpp"

using namespace parser;

constexpr auto white_space = ignore(zero_or_more(' '));

template <class P> static constexpr auto op(P p) {
	return sequence(white_space, ignore(p), white_space);
}

static unsigned int add(unsigned int lhs, unsigned int rhs) {
	return lhs + rhs;
}
static unsigned int subtract(unsigned int lhs, unsigned int rhs) {
	return lhs - rhs;
}
static unsigned int multiply(unsigned int lhs, unsigned int rhs) {
	return lhs * rhs;
}
static unsigned int divide(unsigned int lhs, unsigned int rhs) {
	return lhs / rhs;
}
static unsigned int negate(unsigned int x) {
	return -x;
}

using BinaryOperation = unsigned int (*)(unsigned int, unsigned int);
using UnaryOperation = unsigned int (*)(unsigned int);

template <BinaryOperation> class BinaryOperationTag {
public:
	constexpr BinaryOperationTag() {}
};
template <UnaryOperation> class UnaryOperationTag {
public:
	constexpr UnaryOperationTag() {}
};

class IntCollector {
	unsigned int n;
public:
	IntCollector(): n(0) {}
	void push(char c) {
		n = n * 10 + (c - '0');
	}
	void push(unsigned int n) {
		this->n = n;
	}
	template <BinaryOperation operation> void push(unsigned int n, BinaryOperationTag<operation>) {
		this->n = operation(this->n, n);
	}
	void set_location(const SourceLocation& location) {}
	template <class C> void retrieve(const C& callback) {
		callback.push(n);
	}
};

constexpr auto number = collect<IntCollector>(one_or_more(range('0', '9')));

struct expression_t;
constexpr auto expression = reference<expression_t>();
constexpr auto expression_impl = pratt<IntCollector>(
	pratt_level(
		infix_ltr<TagMapper<BinaryOperationTag<add>>>(op('+')),
		infix_ltr<TagMapper<BinaryOperationTag<subtract>>>(op('-'))
	),
	pratt_level(
		infix_ltr<TagMapper<BinaryOperationTag<multiply>>>(op('*')),
		infix_ltr<TagMapper<BinaryOperationTag<divide>>>(op('/'))
	),
	pratt_level(
		terminal(choice(
			number,
			sequence(ignore('('), white_space, expression, white_space, expect(")")),
			error("expected an expression")
		))
	)
);
struct expression_t {
	static constexpr auto parser = expression_impl;
};
constexpr decltype(expression_impl) expression_t::parser;

constexpr auto program = sequence(
	white_space,
	expression,
	white_space,
	choice(
		end(),
		error("unexpected character at end of program")
	)
);

int main(int argc, const char** argv) {
	using namespace printer;
	if (argc > 1) {
		StringView source(argv[1]);
		unsigned int value;
		parser::Context context(source);
		const Result result = parse_impl(program, context, GetValueCallback<unsigned int>(value));
		if (result == ERROR) {
			print_error("", context.get_source(), context.get_location(), context.get_error());
			return 1;
		}
		if (result == FAILURE) {
			print(std::cerr, ln(bold(yellow("failure"))));
			return 1;
		}
		print(ln(format("% %", bold(green("success:")), print_number(value))));
	}
}
