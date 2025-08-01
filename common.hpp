#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <string>
#include <fstream>
#include <vector>
#include <iterator>

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

template <class T> class Tag {
public:
	constexpr Tag() {}
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

inline std::vector<char> read_file(const char* path) {
	std::ifstream file(path);
	return std::vector<char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

class SourceFile {
	const char* path;
	std::vector<char> content;
public:
	SourceFile(const char* path): path(path), content(read_file(path)) {}
	const char* get_path() const {
		return path;
	}
	const char* data() const {
		return content.data();
	}
	std::size_t size() const {
		return content.size();
	}
	const char* begin() const {
		return content.data();
	}
	const char* end() const {
		return content.data() + content.size();
	}
};
