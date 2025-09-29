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
	return prefix_<T>(get_parser(p));
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

// parse_nud
template <class P, class L, class C> Result parse_nud(const P& pratt, const L& level, const PrattLevel<>& op, Context& context, const C& callback) {
	// last operator; go to next level
	return parse_nud(pratt, level.tail, context, callback);
}
template <class P, class L, class Op0, class... Op, class C> Result parse_nud(const P& pratt, const L& level, const PrattLevel<Op0, Op...>& op, Context& context, const C& callback) {
	// skip irrelevant operators
	return parse_nud(pratt, level, op.tail, context, callback);
}
template <class P, class L, class Op_T, class... Op, class C> Result parse_nud(const P& pratt, const L& level, const PrattLevel<Terminal<Op_T>, Op...>& op, Context& context, const C& callback) {
	// Terminal
	const Result result = parse_impl(op.head.p, context, callback);
	if (result == ERROR) {
		return ERROR;
	}
	if (result == FAILURE) {
		return parse_nud(pratt, level, op.tail, context, callback);
	}
	return SUCCESS;
}
template <class P, class L, class Op_T, class Op_P, class... Op, class C> Result parse_nud(const P& pratt, const L& level, const PrattLevel<Prefix<Op_T, Op_P>, Op...>& op, Context& context, const C& callback) {
	// Prefix
	const Result result = parse_impl(op.head.p, context, callback);
	if (result == ERROR) {
		return ERROR;
	}
	if (result == FAILURE) {
		return parse_nud(pratt, level, op.tail, context, callback);
	}
	return parse_pratt(pratt, level, context, MapCallback<Op_T, C>(callback));
}

template <class P, class L_T, class C> Result parse_nud(const P& pratt, const Pratt<L_T>& level, Context& context, const C& callback) {
	return FAILURE;
}
template <class P, class L_T, class L0, class... L, class C> Result parse_nud(const P& pratt, const Pratt<L_T, L0, L...>& level, Context& context, const C& callback) {
	return parse_nud(pratt, level, level.head, context, callback);
}

// parse_led
template <class P, class L, class C> Result parse_led(const P& pratt, const L& level, const PrattLevel<>& op, Context& context, const C& callback) {
	// last operator; go to next level
	return parse_led(pratt, level.tail, context, callback);
}
template <class P, class L, class Op0, class... Op, class C> Result parse_led(const P& pratt, const L& level, const PrattLevel<Op0, Op...>& op, Context& context, const C& callback) {
	// skip irrelevant operators
	return parse_led(pratt, level, op.tail, context, callback);
}
template <class P, class L, class Op_T, class Op_P, class... Op, class C> Result parse_led(const P& pratt, const L& level, const PrattLevel<InfixLTR<Op_T, Op_P>, Op...>& op, Context& context, const C& callback) {
	// InfixLTR
	const Result result = parse_impl(op.head.p, context, callback);
	if (result == ERROR) {
		return ERROR;
	}
	if (result == FAILURE) {
		return parse_led(pratt, level, op.tail, context, callback);
	}
	return parse_pratt(pratt, level.tail, context, MapCallback<Op_T, C>(callback));
}
template <class P, class L, class Op_T, class Op_P, class... Op, class C> Result parse_led(const P& pratt, const L& level, const PrattLevel<InfixRTL<Op_T, Op_P>, Op...>& op, Context& context, const C& callback) {
	// InfixRTL
	const Result result = parse_impl(op.head.p, context, callback);
	if (result == ERROR) {
		return ERROR;
	}
	if (result == FAILURE) {
		return parse_led(pratt, level, op.tail, context, callback);
	}
	return parse_pratt(pratt, level, context, MapCallback<Op_T, C>(callback));
}
template <class P, class L, class Op_T, class Op_P, class... Op, class C> Result parse_led(const P& pratt, const L& level, const PrattLevel<Postfix<Op_T, Op_P>, Op...>& op, Context& context, const C& callback) {
	// Postfix
	const Result result = parse_impl(op.head.p, context, callback);
	if (result == ERROR) {
		return ERROR;
	}
	if (result == FAILURE) {
		return parse_led(pratt, level, op.tail, context, callback);
	}
	Op_T::map(callback);
	return SUCCESS;
}

template <class P, class L_T, class C> Result parse_led(const P& pratt, const Pratt<L_T>& level, Context& context, const C& callback) {
	return FAILURE;
}
template <class P, class L_T, class L0, class... L, class C> Result parse_led(const P& pratt, const Pratt<L_T, L0, L...>& level, Context& context, const C& callback) {
	return parse_led(pratt, level, level.head, context, callback);
}

template <class T, class... P, class L, class C> Result parse_pratt(const Pratt<T, P...>& pratt, const L& level, Context& context, const C& callback) {
	T collector;
	const Result result = parse_nud(pratt, pratt, context, CollectCallback<T>(collector));
	if (result != SUCCESS) {
		return result;
	}
	while (true) {
		const Result result = parse_led(pratt, level, context, CollectCallback<T>(collector));
		if (result == ERROR) {
			return ERROR;
		}
		if (result == FAILURE) {
			break;
		}
	}
	collector.retrieve(callback);
	return SUCCESS;
}
template <class T, class... P, class C> Result parse_impl(const Pratt<T, P...>& p, Context& context, const C& callback) {
	return parse_pratt(p, p, context, callback);
}

}
