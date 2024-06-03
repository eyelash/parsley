#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>
#include <string>
#include <new>

template <class T> class Reference {
	T* pointer;
public:
	Reference(T* pointer = nullptr): pointer(pointer) {}
	Reference(const Reference&) = delete;
	Reference(Reference&& reference): pointer(std::exchange(reference.pointer, nullptr)) {}
	~Reference() {
		if (pointer) {
			delete pointer;
		}
	}
	Reference& operator =(const Reference&) = delete;
	Reference& operator =(Reference&& reference) {
		std::swap(pointer, reference.pointer);
		return *this;
	}
	operator T*() const {
		return pointer;
	}
};

template <std::size_t I, class... T> struct IndexToType;
template <class T0, class... T> struct IndexToType<0, T0, T...> {
	using type = T0;
};
template <std::size_t I, class T0, class... T> struct IndexToType<I, T0, T...> {
	using type = typename IndexToType<I - 1, T...>::type;
};

template <class U, class... T> struct TypeToIndex;
template <class U, class... T> struct TypeToIndex<U, U, T...> {
	static constexpr std::size_t index = 0;
};
template <class U, class T0, class... T> struct TypeToIndex<U, T0, T...> {
	static constexpr std::size_t index = 1 + TypeToIndex<U, T...>::index;
};

template <class... T> class Tuple;
template <> class Tuple<> {
public:
	constexpr Tuple() {}
	static constexpr std::size_t size() {
		return 0;
	}
};
template <class T0, class... T> class Tuple<T0, T...> {
public:
	T0 head;
	Tuple<T...> tail;
	template <class A0, class... A> constexpr Tuple(A0&& a0, A&&... a): head(std::forward<A0>(a0)), tail(std::forward<A>(a)...) {}
	static constexpr std::size_t size() {
		return 1 + sizeof...(T);
	}
	template <std::size_t I> using get_type = typename IndexToType<I, T0, T...>::type;
	template <class U> static constexpr std::size_t index_of = TypeToIndex<U, T0, T...>::index;
};

template <class... T> class Union;
template <> class Union<> {
public:
	constexpr Union() {}
};
template <class T0, class... T> class Union<T0, T...> {
public:
	union {
		T0 head;
		Union<T...> tail;
	};
	Union() {}
	~Union() {}
	template <std::size_t I, class... A> void construct(A&&... a) {
		if constexpr (I == 0) {
			new (&head) T0(std::forward<A>(a)...);
		}
		else {
			tail.template construct<I - 1>(std::forward<A>(a)...);
		}
	}
	template <std::size_t I> void destruct() {
		if constexpr (I == 0) {
			head.~T0();
		}
		else {
			tail.template destruct<I - 1>();
		}
	}
	template <std::size_t I> using get_type = typename IndexToType<I, T0, T...>::type;
	template <std::size_t I> get_type<I>& get() {
		if constexpr (I == 0) {
			return head;
		}
		else {
			return tail.template get<I - 1>();
		}
	}
	template <std::size_t I> const get_type<I>& get() const {
		if constexpr (I == 0) {
			return head;
		}
		else {
			return tail.template get<I - 1>();
		}
	}
};

template <class... T> class Variant {
	Union<T...> data_;
	std::size_t index_;
	template <std::size_t I> void destruct() {
		if constexpr (I < sizeof...(T)) {
			if (I == index_) {
				data_.template destruct<I>();
			}
			else {
				destruct<I + 1>();
			}
		}
	}
public:
	template <class U> Variant(U&& u) {
		constexpr std::size_t I = index_of<U>;
		data_.template construct<I>(std::forward<U>(u));
		index_ = I;
	}
	Variant(const Variant&) = delete;
	~Variant() {
		destruct<0>();
	}
	Variant& operator =(const Variant&) = delete;
	constexpr std::size_t index() const {
		return index_;
	}
	template <std::size_t I> using get_type = typename IndexToType<I, T...>::type;
	template <class U> static constexpr std::size_t index_of = TypeToIndex<U, T...>::index;
	template <std::size_t I> get_type<I>* get() {
		if (I == index_) {
			return &data_.template get<I>();
		}
		else {
			return nullptr;
		}
	}
	template <class U> U* get() {
		return get<index_of<U>>();
	}
	template <std::size_t I = 0, class F> void visit(F&& f) {
		if constexpr (I < sizeof...(T)) {
			if (I == index_) {
				std::forward<F>(f)(data_.template get<I>());
			}
			else {
				visit<I + 1>(std::forward<F>(f));
			}
		}
	}
};

class StringView {
	const char* string;
	std::size_t length;
	static constexpr int strncmp(const char* s0, const char* s1, std::size_t n) {
		return n == 0 ? 0 : *s0 != *s1 ? *s0 - *s1 : strncmp(s0 + 1, s1 + 1, n - 1);
	}
	static constexpr const char* strchr(const char* s, char c) {
		return *s == c ? s : *s == '\0' ? nullptr : strchr(s + 1, c);
	}
	static constexpr std::size_t strlen(const char* s) {
		return strchr(s, '\0') - s;
	}
public:
	constexpr StringView(): string(nullptr), length(0) {}
	constexpr StringView(const char* string, std::size_t length): string(string), length(length) {}
	constexpr StringView(const char* string): string(string), length(strlen(string)) {}
	constexpr char operator [](std::size_t i) const {
		return string[i];
	}
	constexpr char operator *() const {
		return *string;
	}
	constexpr operator bool() const {
		return string != nullptr;
	}
	constexpr std::size_t size() const {
		return length;
	}
	constexpr const char* data() const {
		return string;
	}
	constexpr bool empty() const {
		return length == 0;
	}
	constexpr bool operator ==(const StringView& s) const {
		return length != s.length ? false : strncmp(string, s.string, length) == 0;
	}
	constexpr bool operator !=(const StringView& s) const {
		return !operator ==(s);
	}
	constexpr bool operator <(const StringView& s) const {
		return length != s.length ? length < s.length : strncmp(string, s.string, length) < 0;
	}
	constexpr const char* begin() const {
		return string;
	}
	constexpr const char* end() const {
		return string + length;
	}
	constexpr bool contains(char c) const {
		return strchr(string, c);
	}
	constexpr StringView substr(std::size_t pos, std::size_t count) const {
		return StringView(string + pos, count);
	}
	constexpr StringView substr(std::size_t pos) const {
		return StringView(string + pos, length - pos);
	}
};

inline std::uint32_t next_codepoint(StringView& s) {
	std::uint32_t codepoint = 0;
	if (s.size() >= 1 && (s[0] & 0b1000'0000) == 0b0000'0000) {
		codepoint = s[0];
		s = s.substr(1);
	}
	else if (s.size() >= 2 && (s[0] & 0b1110'0000) == 0b1100'0000) {
		codepoint |= (s[0] & 0b0001'1111) << 6;
		codepoint |= (s[1] & 0b0011'1111);
		s = s.substr(2);
	}
	else if (s.size() >= 3 && (s[0] & 0b1111'0000) == 0b1110'0000) {
		codepoint |= (s[0] & 0b0000'1111) << 12;
		codepoint |= (s[1] & 0b0011'1111) << 6;
		codepoint |= (s[2] & 0b0011'1111);
		s = s.substr(3);
	}
	else if (s.size() >= 4 && (s[0] & 0b1111'1000) == 0b1111'0000) {
		codepoint |= (s[0] & 0b0000'0111) << 18;
		codepoint |= (s[1] & 0b0011'1111) << 12;
		codepoint |= (s[2] & 0b0011'1111) << 6;
		codepoint |= (s[3] & 0b0011'1111);
		s = s.substr(4);
	}
	return codepoint;
}

inline std::string from_codepoint(std::uint32_t codepoint) {
	std::string s;
	if (codepoint < 0b1000'0000) {
		s.push_back(codepoint);
	}
	else if (codepoint < 0b1000'0000'0000) {
		s.push_back(0b1100'0000 | codepoint >> 6);
		s.push_back(0b1000'0000 | codepoint & 0b0011'1111);
	}
	else if (codepoint < 0b0001'0000'0000'0000'0000) {
		s.push_back(0b1110'0000 | codepoint >> 12);
		s.push_back(0b1000'0000 | codepoint >> 6 & 0b0011'1111);
		s.push_back(0b1000'0000 | codepoint & 0b0011'1111);
	}
	else if (codepoint < 0b0010'0000'0000'0000'0000'0000) {
		s.push_back(0b1111'0000 | codepoint >> 18);
		s.push_back(0b1000'0000 | codepoint >> 12 & 0b0011'1111);
		s.push_back(0b1000'0000 | codepoint >> 6 & 0b0011'1111);
		s.push_back(0b1000'0000 | codepoint & 0b0011'1111);
	}
	return s;
}

class CodePoints {
	StringView s;
public:
	class Iterator {
		StringView s;
		std::uint32_t codepoint;
	public:
		Iterator(const StringView& s): s(s), codepoint(next_codepoint(this->s)) {}
		Iterator(): s(), codepoint(0) {}
		bool operator !=(const Iterator& rhs) const {
			return codepoint != rhs.codepoint;
		}
		std::uint32_t operator *() const {
			return codepoint;
		}
		Iterator& operator ++() {
			codepoint = next_codepoint(s);
			return *this;
		}
	};
	constexpr CodePoints(const StringView& s): s(s) {}
	Iterator begin() const {
		return Iterator(s);
	}
	Iterator end() const {
		return Iterator();
	}
};
constexpr CodePoints code_points(const StringView& s) {
	return CodePoints(s);
}
inline CodePoints code_points(const std::string& s) {
	return CodePoints(StringView(s.data(), s.size()));
}
