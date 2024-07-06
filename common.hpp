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
	T* operator ->() const {
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
};

template <class T> struct GetSize;
template <class T> struct GetSize<T&> {
	static constexpr std::size_t value = GetSize<T>::value;
};
template <class T> struct GetSize<const T&> {
	static constexpr std::size_t value = GetSize<T>::value;
};
template <class... T> struct GetSize<Tuple<T...>> {
	static constexpr std::size_t value = sizeof...(T);
};
template <class... T> struct GetSize<Union<T...>> {
	static constexpr std::size_t value = sizeof...(T);
};

template <std::size_t I, class T, bool enable, class = void> struct EnableGet;
template <std::size_t I, class T> struct EnableGet<I, T&, true, std::void_t<typename EnableGet<I, T, true>::type>> {
	using type = typename EnableGet<I, T, true>::type&;
};
template <std::size_t I, class T> struct EnableGet<I, const T&, true, std::void_t<typename EnableGet<I, T, true>::type>> {
	using type = const typename EnableGet<I, T, true>::type&;
};
template <std::size_t I, class... T> struct EnableGet<I, Tuple<T...>, true> {
	using type = typename IndexToType<I, T...>::type;
};
template <std::size_t I, class... T> struct EnableGet<I, Union<T...>, true> {
	using type = typename IndexToType<I, T...>::type;
};

template <std::size_t I, class T> constexpr typename EnableGet<I, T, I == 0>::type&& get(T&& t) {
	return std::forward<T>(t).head;
}
template <std::size_t I, class T> constexpr typename EnableGet<I, T, I != 0>::type&& get(T&& t) {
	return get<I - 1>(std::forward<T>(t).tail);
}

template <std::size_t I, class... T, class... A> void union_construct(Union<T...>& union_, A&&... a) {
	using T0 = typename IndexToType<I, T...>::type;
	new (&get<I>(union_)) T0(std::forward<A>(a)...);
}
template <std::size_t I, class... T> void union_destruct(Union<T...>& union_) {
	using T0 = typename IndexToType<I, T...>::type;
	get<I>(union_).~T0();
}

template <class D, std::size_t... I, class... A> auto dispatch(std::index_sequence<I...>, std::size_t index, A&&... a) {
	using F = decltype(&D::template dispatch<0>);
	static constexpr F table[sizeof...(I)] = {&D::template dispatch<I>...};
	return table[index](std::forward<A>(a)...);
}
template <class D, class T, class... A> auto union_dispatch(std::size_t index, T&& t, A&&... a) {
	return dispatch<D>(std::make_index_sequence<GetSize<T>::value>(), index, std::forward<T>(t), std::forward<A>(a)...);
}

template <class T> struct TypeTag {
	constexpr TypeTag() {}
};

template <class... T> class Variant {
	Union<T...> data_;
	std::size_t index_;
	struct CopyConstruct {
		template <std::size_t I> static void dispatch(Union<T...>& data_, const Union<T...>& rhs) {
			union_construct<I>(data_, get<I>(rhs));
		}
	};
	struct MoveConstruct {
		template <std::size_t I> static void dispatch(Union<T...>& data_, Union<T...>&& rhs) {
			union_construct<I>(data_, get<I>(std::move(rhs)));
		}
	};
	struct Destruct {
		template <std::size_t I> static void dispatch(Union<T...>& data_) {
			union_destruct<I>(data_);
		}
	};
	struct CopyAssign {
		template <std::size_t I> static void dispatch(Union<T...>& data_, const Union<T...>& rhs) {
			get<I>(data_) = get<I>(rhs);
		}
	};
	struct MoveAssign {
		template <std::size_t I> static void dispatch(Union<T...>& data_, Union<T...>&& rhs) {
			get<I>(data_) = get<I>(std::move(rhs));
		}
	};
	template <class F> struct Visit {
		template <std::size_t I> static void dispatch(Union<T...>& data_, F&& f) {
			std::forward<F>(f)(get<I>(data_));
		}
	};
public:
	template <class U> Variant(U&& u) {
		constexpr std::size_t I = index_of<U>;
		union_construct<I>(data_, std::forward<U>(u));
		index_ = I;
	}
	template <class U, class... A> Variant(TypeTag<U>, A&&... a) {
		constexpr std::size_t I = index_of<U>;
		union_construct<I>(data_, std::forward<A>(a)...);
		index_ = I;
	}
	Variant(const Variant& variant) {
		union_dispatch<CopyConstruct>(variant.index_, data_, variant.data_);
		index_ = variant.index_;
	}
	Variant(Variant&& variant) {
		union_dispatch<MoveConstruct>(variant.index_, data_, std::move(variant).data_);
		index_ = variant.index_;
	}
	~Variant() {
		union_dispatch<Destruct>(index_, data_);
	}
	Variant& operator =(const Variant& variant) {
		if (index_ == variant.index_) {
			union_dispatch<CopyAssign>(index_, data_, variant.data_);
		}
		else {
			union_dispatch<Destruct>(index_, data_);
			union_dispatch<CopyConstruct>(variant.index_, data_, variant.data_);
			index_ = variant.index_;
		}
		return *this;
	}
	Variant& operator =(Variant&& variant) {
		if (index_ == variant.index_) {
			union_dispatch<MoveAssign>(index_, data_, std::move(variant).data_);
		}
		else {
			union_dispatch<Destruct>(index_, data_);
			union_dispatch<MoveConstruct>(variant.index_, data_, std::move(variant).data_);
			index_ = variant.index_;
		}
		return *this;
	}
	constexpr std::size_t index() const {
		return index_;
	}
	template <std::size_t I> using get_type = typename IndexToType<I, T...>::type;
	template <class U> static constexpr std::size_t index_of = TypeToIndex<U, T...>::index;
	template <std::size_t I> friend get_type<I>* get(Variant& variant) {
		if (I == variant.index_) {
			return &get<I>(variant.data_);
		}
		else {
			return nullptr;
		}
	}
	template <class U> friend U* get(Variant& variant) {
		return get<index_of<U>>(variant);
	}
	template <class F> void visit(F&& f) {
		union_dispatch<Visit<F>>(index_, data_, std::forward<F>(f));
	}
};

class StringView {
	const char* string;
	std::size_t length;
	static constexpr int strcmp(const char* s0, const char* s1) {
		return *s0 != *s1 ? *s0 - *s1 : *s0 == '\0' ? 0 : strcmp(s0 + 1, s1 + 1);
	}
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
	explicit constexpr operator bool() const {
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
	constexpr bool starts_with(const StringView& s) const {
		return length < s.length ? false : strncmp(string, s.string, s.length) == 0;
	}
	constexpr bool ends_with(const StringView& s) const {
		return length < s.length ? false : strncmp(string + length - s.length, s.string, s.length) == 0;
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
