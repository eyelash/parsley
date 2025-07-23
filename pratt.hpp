#pragma once

#include "parser.hpp"

namespace parser {

template <class P> class Terminal {
public:
	P p;
	constexpr Terminal(P p): p(p) {}
};

template <class T, class P> class InfixLTR {
public:
	P p;
	constexpr InfixLTR(P p): p(p) {}
};

template <class T, class P> class InfixRTL {
public:
	P p;
	constexpr InfixRTL(P p): p(p) {}
};

template <class T, class P> class Prefix {
public:
	P p;
	constexpr Prefix(P p): p(p) {}
};

template <class T, class P> class Postfix {
public:
	P p;
	constexpr Postfix(P p): p(p) {}
};

template <class... P> class PrattLevel;
template <> class PrattLevel<> {
public:
	constexpr PrattLevel() {}
};
template <class P0, class... P> class PrattLevel<P0, P...> {
public:
	P0 head;
	PrattLevel<P...> tail;
	constexpr PrattLevel(P0 head, P... tail): head(head), tail(tail...) {}
};

template <class T, class... P> class Pratt;
template <class T> class Pratt<T> {
public:
	constexpr Pratt() {}
};
template <class T, class P0, class... P> class Pratt<T, P0, P...> {
public:
	P0 head;
	Pratt<T, P...> tail;
	constexpr Pratt(P0 head, P... tail): head(head), tail(tail...) {}
};

template <class P> constexpr auto terminal(P p) {
	return Terminal(get_parser(p));
}
template <class T, class P> constexpr InfixLTR<T, P> infix_ltr_(P p) {
	return InfixLTR<T, P>(p);
}
template <class T, class P> constexpr auto infix_ltr(P p) {
	return infix_ltr_<T>(get_parser(p));
}
template <class T, class P> constexpr InfixRTL<T, P> infix_rtl_(P p) {
	return InfixRTL<T, P>(p);
}
template <class T, class P> constexpr auto infix_rtl(P p) {
	return infix_rtl_<T>(get_parser(p));
}
template <class T, class P> constexpr Prefix<T, P> prefix_(P p) {
	return Prefix<T, P>(p);
}
template <class T, class P> constexpr auto prefix(P p) {
	return PrattLeft(prefix_<T>(get_parser(p)));
}
template <class T, class P> constexpr Postfix<T, P> postfix_(P p) {
	return Postfix<T, P>(p);
}
template <class T, class P> constexpr auto postfix(P p) {
	return postfix_<T>(get_parser(p));
}
template <class... P> constexpr PrattLevel<P...> pratt_level(P... p) {
	return PrattLevel<P...>(p...);
}
template <class T, class... P> constexpr Pratt<T, P...> pratt_(P... p) {
	return Pratt<T, P...>(p...);
}
template <class T, class... P> constexpr auto pratt(P... p) {
	return Pratt<T, P...>(p...);
}

// parse_left
template <class P, class L, class C> Result parse_left(const P& pratt, const L& level, const PrattLevel<>& op, Context& context, const C& callback) {
	// last operator; go to next level
	return parse_left(pratt, level.tail, context, callback);
}
template <class P, class L, class Op0, class... Op, class C> Result parse_left(const P& pratt, const L& level, const PrattLevel<Op0, Op...>& op, Context& context, const C& callback) {
	// skip irrelevant operators
	return parse_left(pratt, level, op.tail, context, callback);
}
template <class P, class L, class Op0, class... Op, class C> Result parse_left(const P& pratt, const L& level, const PrattLevel<Terminal<Op0>, Op...>& op, Context& context, const C& callback) {
	// Terminal
	const Result result = parse_impl(op.head.p, context, callback);
	if (result == FAILURE) {
		return parse_left(pratt, level, op.tail, context, callback);
	}
	return result;
}
template <class P, class L, class Op_T, class Op_P, class... Op, class C> Result parse_left(const P& pratt, const L& level, const PrattLevel<Prefix<Op_T, Op_P>, Op...>& op, Context& context, const C& callback) {
	// Prefix
	const Result result = parse_impl(op.head.p, context, callback);
	if (result == FAILURE) {
		return parse_left(pratt, level, op.tail, context, callback);
	}
	if (result == ERROR) {
		return ERROR;
	}
	return parse_impl(pratt, level, context, TaggedCallback<Op_T, C>(callback));
}
template <class P, class L_T, class C> Result parse_left(const P& pratt, const Pratt<L_T>& level, Context& context, const C& callback) {
	return FAILURE;
}
template <class P, class L_T, class L0, class... L, class C> Result parse_left(const P& pratt, const Pratt<L_T, L0, L...>& level, Context& context, const C& callback) {
	return parse_left(pratt, level, level.head, context, callback);
}

// parse_right
template <class P, class L, class C> Result parse_right(const P& pratt, const L& level, const PrattLevel<>& op, Context& context, const C& callback) {
	// last operator; go to next level
	return parse_right(pratt, level.tail, context, callback);
}
template <class P, class L, class Op0, class... Op, class C> Result parse_right(const P& pratt, const L& level, const PrattLevel<Op0, Op...>& op, Context& context, const C& callback) {
	// skip irrelevant operators
	return parse_right(pratt, level, op.tail, context, callback);
}
template <class P, class L, class Op_T, class Op_P, class... Op, class C> Result parse_right(const P& pratt, const L& level, const PrattLevel<InfixLTR<Op_T, Op_P>, Op...>& op, Context& context, const C& callback) {
	// InfixLTR
	const Result result = parse_impl(op.head.p, context, callback);
	if (result == FAILURE) {
		return parse_right(pratt, level, op.tail, context, callback);
	}
	if (result == ERROR) {
		return ERROR;
	}
	return parse_impl(pratt, level.tail, context, TaggedCallback<Op_T, C>(callback));
}
template <class P, class L, class Op_T, class Op_P, class... Op, class C> Result parse_right(const P& pratt, const L& level, const PrattLevel<InfixRTL<Op_T, Op_P>, Op...>& op, Context& context, const C& callback) {
	// InfixRTL
	const Result result = parse_impl(op.head.p, context, callback);
	if (result == FAILURE) {
		return parse_right(pratt, level, op.tail, context, callback);
	}
	if (result == ERROR) {
		return ERROR;
	}
	return parse_impl(pratt, level, context, TaggedCallback<Op_T, C>(callback));
}
template <class P, class L, class Op_T, class Op_P, class... Op, class C> Result parse_right(const P& pratt, const L& level, const PrattLevel<Postfix<Op_T, Op_P>, Op...>& op, Context& context, const C& callback) {
	// Postfix
	const Result result = parse_impl(op.head.p, context, callback);
	if (result == FAILURE) {
		return parse_right(pratt, level, op.tail, context, callback);
	}
	if (result == ERROR) {
		return ERROR;
	}
	callback.push(Tag<Op_T>());
	return SUCCESS;
}
template <class P, class L_T, class C> Result parse_right(const P& pratt, const Pratt<L_T>& level, Context& context, const C& callback) {
	return FAILURE;
}
template <class P, class L_T, class L0, class... L, class C> Result parse_right(const P& pratt, const Pratt<L_T, L0, L...>& level, Context& context, const C& callback) {
	return parse_right(pratt, level, level.head, context, callback);
}

template <class T, class... P, class L, class C> Result parse_impl(const Pratt<T, P...>& pratt, const L& level, Context& context, const C& callback) {
	T collector;
	Result result = parse_left(pratt, pratt, context, CollectCallback<T>(collector));
	if (result != SUCCESS) {
		return result;
	}
	while ((result = parse_right(pratt, level, context, CollectCallback<T>(collector))) == SUCCESS) {}
	if (result == ERROR) {
		return ERROR;
	}
	collector.retrieve(callback);
	return SUCCESS;
}
template <class T, class... P, class C> Result parse_impl(const Pratt<T, P...>& p, Context& context, const C& callback) {
	return parse_impl(p, p, context, callback);
}

}
