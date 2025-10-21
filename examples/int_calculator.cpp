#include "../parser.hpp"
#include "../pratt.hpp"
#include "../printer.hpp"

using namespace parser;

constexpr auto white_space = ignore(zero_or_more(' '));

template <class P> static constexpr auto op(P p) {
	return sequence(white_space, ignore(p), white_space);
}

enum Operation {
	ADD,
	SUB,
	MUL,
	DIV
};

template <int> class OpTag {
public:
	constexpr OpTag() {}
	template <class C, class... A> static void map(const C& callback, A&&... a) {
		callback.push(std::forward<A>(a)..., OpTag());
	}
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
	void push(unsigned int n, OpTag<ADD>) {
		this->n += n;
	}
	void push(unsigned int n, OpTag<SUB>) {
		this->n -= n;
	}
	void push(unsigned int n, OpTag<MUL>) {
		this->n *= n;
	}
	void push(unsigned int n, OpTag<DIV>) {
		this->n /= n;
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(n);
	}
};

constexpr auto number = collect<IntCollector>(one_or_more(range('0', '9')));

struct Expression;
constexpr auto expression = reference<Expression>();
struct Expression {
	static constexpr auto parser = pratt<IntCollector>(
		pratt_level(
			infix_ltr<OpTag<ADD>>(op('+')),
			infix_ltr<OpTag<SUB>>(op('-'))
		),
		pratt_level(
			infix_ltr<OpTag<MUL>>(op('*')),
			infix_ltr<OpTag<DIV>>(op('/'))
		),
		pratt_level(
			terminal(choice(
				number,
				sequence(ignore('('), white_space, expression, white_space, expect(")")),
				error("expected an expression")
			))
		)
	);
};

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
			print_error("", context.get_source(), context.get_position(), context.get_error());
			return 1;
		}
		if (result == FAILURE) {
			print(std::cerr, ln(bold(yellow("failure"))));
			return 1;
		}
		print(ln(format("% %", bold(green("success:")), print_number(value))));
	}
}
