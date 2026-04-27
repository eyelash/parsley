#pragma once

#include "common.hpp"
#include <cstdlib>
#include <cstring>
#include <cerrno>

#ifdef _WIN32
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif

class Path {
	static constexpr bool is_separator(char c) {
		#ifdef _WIN32
		return c == '\\' || c == '/';
		#else
		return c == '/';
		#endif
	}
	static constexpr StringView get_separator() {
		#ifdef _WIN32
		return StringView("\\");
		#else
		return StringView("/");
		#endif
	}
	static bool is_absolute(const std::string& path) {
		return path.size() > 0 && is_separator(path[0]);
	}
	static std::size_t get_start(const std::string& path) {
		return is_absolute(path) ? 1 : 0;
	}
	static void write(std::string& path, std::size_t& i, const StringView& s) {
		path.replace(i, s.size(), s.data(), s.size());
		i += s.size();
	}
	static void push_component(std::string& path, std::size_t& i, std::size_t start, const StringView& component) {
		if (i > start) {
			write(path, i, get_separator());
		}
		write(path, i, component);
	}
	static void push_component(std::string& path, const StringView& component) {
		std::size_t i = path.size();
		push_component(path, i, get_start(path), component);
	}
	static void pop_component(const std::string& path, std::size_t& i, std::size_t start) {
		while (i > start && !is_separator(path[i - 1])) {
			--i;
		}
		if (i > start) {
			--i;
		}
	}
	static StringView get_next_component(const std::string& path, std::size_t& i, std::size_t end) {
		const std::size_t component_start = i;
		while (i < end && !is_separator(path[i])) {
			++i;
		}
		const std::size_t component_end = i;
		if (i < end) {
			++i;
		}
		return StringView(path.data() + component_start, component_end - component_start);
	}
	static void normalize(std::string& path) {
		const bool relative = !is_absolute(path);
		const std::size_t start = get_start(path);
		const std::size_t end = path.size();
		std::size_t read_i = start;
		std::size_t write_i = start;
		std::size_t component_count = 0;
		while (read_i < end) {
			const StringView component = get_next_component(path, read_i, end);
			if (component.empty() || component == ".") {
				continue;
			}
			else if (component == "..") {
				if (component_count > 0) {
					pop_component(path, write_i, start);
					--component_count;
				}
				else if (relative) {
					push_component(path, write_i, start, component);
				}
			}
			else {
				push_component(path, write_i, start, component);
				++component_count;
			}
		}
		if (relative && write_i == start) {
			write(path, write_i, ".");
		}
		path.resize(write_i);
	}
	static std::size_t get_parent(const std::string& path) {
		const std::size_t start = get_start(path);
		std::size_t i = path.size();
		while (i > start && is_separator(path[i - 1])) {
			--i;
		}
		while (i > start && !is_separator(path[i - 1])) {
			--i;
		}
		while (i > start && is_separator(path[i - 1])) {
			--i;
		}
		return i;
	}
	static StringView get_file_name(const std::string& path) {
		const std::size_t start = get_start(path);
		std::size_t i = path.size();
		while (i > start && !is_separator(path[i - 1])) {
			--i;
		}
		return StringView(path.data() + i, path.size() - i);
	}
	std::string path;
public:
	Path(std::string&& path): path(std::move(path)) {}
	Path(const char* path): path(path) {}
	operator const char*() const {
		return path.c_str();
	}
	bool is_absolute() const {
		return is_absolute(path);
	}
	bool is_relative() const {
		return !is_absolute(path);
	}
	Path parent() && {
		const std::size_t i = get_parent(path);
		path.resize(i);
		return Path(std::move(path));
	}
	Path parent() const& {
		const std::size_t i = get_parent(path);
		return Path(path.substr(0, i));
	}
	StringView file_name() const {
		return get_file_name(path);
	}
	Path normalize() && {
		normalize(path);
		return Path(std::move(path));
	}
	Path normalize() const& {
		std::string new_path = path;
		normalize(new_path);
		return Path(std::move(new_path));
	}
	friend Path operator /(Path&& lhs, const Path& rhs) {
		if (rhs.is_absolute()) {
			return rhs;
		}
		push_component(lhs.path, rhs.path);
		return Path(std::move(lhs.path));
	}
	friend Path operator /(const Path& lhs, const Path& rhs) {
		if (rhs.is_absolute()) {
			return rhs;
		}
		std::string new_path = lhs.path;
		push_component(new_path, rhs.path);
		return Path(std::move(new_path));
	}
};

class Input {
public:
	virtual std::size_t read(char* data, std::size_t size) = 0;
};

class Output {
public:
	virtual void write(const char* data, std::size_t size) = 0;
};

class BufferedInput: public Input {
	static constexpr std::size_t BUFFER_SIZE = 8192;
	Input& input;
	char buffer[BUFFER_SIZE];
	std::size_t i, buffer_size;
public:
	BufferedInput(Input& input): input(input), i(0), buffer_size(0) {}
	BufferedInput(const BufferedInput&) = delete;
	BufferedInput& operator =(const BufferedInput&) = delete;
	std::size_t read(char* data, std::size_t size) override {
		const std::size_t capacity = buffer_size - i;
		if (size <= capacity) {
			std::memcpy(data, buffer + i, size);
			i += size;
			return size;
		}
		if (capacity > 0) {
			std::memcpy(data, buffer + i, capacity);
			data += capacity;
			size -= capacity;
			i = buffer_size;
		}
		if (size < BUFFER_SIZE) {
			buffer_size = input.read(buffer, BUFFER_SIZE);
			size = std::min(size, buffer_size);
			std::memcpy(data, buffer, size);
			i = size;
			return capacity + size;
		}
		else {
			return capacity + input.read(data, size);
		}
	}
	bool read(char& c) {
		return read(&c, 1);
	}
};

class BufferedOutput: public Output {
	static constexpr std::size_t BUFFER_SIZE = 8192;
	Output& output;
	char buffer[BUFFER_SIZE];
	std::size_t i;
public:
	BufferedOutput(Output& output): output(output), i(0) {}
	BufferedOutput(const BufferedOutput&) = delete;
	~BufferedOutput() {
		output.write(buffer, i);
	}
	BufferedOutput& operator =(const BufferedOutput&) = delete;
	void write(const char* data, std::size_t size) override {
		const std::size_t capacity = BUFFER_SIZE - i;
		if (size < capacity) {
			std::memcpy(buffer + i, data, size);
			i += size;
			return;
		}
		if (i > 0) {
			std::memcpy(buffer + i, data, capacity);
			data += capacity;
			size -= capacity;
			output.write(buffer, BUFFER_SIZE);
			i = 0;
		}
		if (size < BUFFER_SIZE) {
			std::memcpy(buffer, data, size);
			i = size;
		}
		else {
			output.write(data, size);
		}
	}
	void write(char c) {
		write(&c, 1);
	}
	void flush() {
		output.write(buffer, i);
		i = 0;
	}
};

class StringInput: public Input {
	StringView string;
public:
	StringInput(const StringView& string): string(string) {}
	std::size_t read(char* data, std::size_t size) override {
		size = std::min(size, string.size());
		std::memcpy(data, string.data(), size);
		string = string.substr(size);
		return size;
	}
};

class StringOutput: public Output {
	std::string& string;
public:
	StringOutput(std::string& string): string(string) {}
	void write(const char* data, std::size_t size) override {
		string.append(data, size);
	}
};

class ReadFile: public Input {
	#ifdef _WIN32
	#else
	int fd;
	#endif
public:
	#ifdef _WIN32
	#else
	ReadFile(int fd): fd(fd) {}
	#endif
	ReadFile() {
		#ifdef _WIN32
		#else
		fd = -1;
		#endif
	}
	ReadFile(const char* path) {
		#ifdef _WIN32
		#else
		fd = open(path, O_RDONLY | O_CLOEXEC);
		#endif
	}
	ReadFile(const ReadFile&) = delete;
	ReadFile(ReadFile&& file) {
		#ifdef _WIN32
		#else
		fd = file.fd;
		file.fd = -1;
		#endif
	}
	~ReadFile() {
		#ifdef _WIN32
		#else
		if (fd != -1) {
			close(fd);
		}
		#endif
	}
	ReadFile& operator =(const ReadFile&) = delete;
	ReadFile& operator =(ReadFile&& file) {
		#ifdef _WIN32
		#else
		if (fd != -1) {
			close(fd);
		}
		fd = file.fd;
		file.fd = -1;
		#endif
		return *this;
	}
	explicit operator bool() const {
		#ifdef _WIN32
		return false;
		#else
		return fd != -1;
		#endif
	}
	std::size_t read(char* data, std::size_t size) override {
		#ifdef _WIN32
		#else
		while (true) {
			const ssize_t result = ::read(fd, data, size);
			if (result == -1) {
				if (errno == EINTR) {
					continue;
				}
				return 0;
			}
			return result;
		}
		#endif
	}
	std::size_t size() const {
		#ifdef _WIN32
		return 0;
		#else
		struct stat s;
		fstat(fd, &s);
		return s.st_size;
		#endif
	}
};

class WriteFile: public Output {
	#ifdef _WIN32
	#else
	int fd;
	#endif
public:
	#ifdef _WIN32
	#else
	WriteFile(int fd): fd(fd) {}
	#endif
	WriteFile() {
		#ifdef _WIN32
		#else
		fd = -1;
		#endif
	}
	WriteFile(const char* path) {
		#ifdef _WIN32
		#else
		fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
		#endif
	}
	WriteFile(const WriteFile&) = delete;
	WriteFile(WriteFile&& file) {
		#ifdef _WIN32
		#else
		fd = file.fd;
		file.fd = -1;
		#endif
	}
	~WriteFile() {
		#ifdef _WIN32
		#else
		if (fd != -1) {
			close(fd);
		}
		#endif
	}
	WriteFile& operator =(const WriteFile&) = delete;
	WriteFile& operator =(WriteFile&& file) {
		#ifdef _WIN32
		#else
		if (fd != -1) {
			close(fd);
		}
		fd = file.fd;
		file.fd = -1;
		#endif
		return *this;
	}
	explicit operator bool() const {
		#ifdef _WIN32
		return false;
		#else
		return fd != -1;
		#endif
	}
	void write(const char* data, std::size_t size) override {
		#ifdef _WIN32
		#else
		while (size > 0) {
			const ssize_t result = ::write(fd, data, size);
			if (result == -1) {
				if (errno == EINTR) {
					continue;
				}
				return;
			}
			data += result;
			size -= result;
		}
		#endif
	}
};

class StandardInput {
public:
	static BufferedInput& get() {
		#ifdef _WIN32
		#else
		static ReadFile standard_input(STDIN_FILENO);
		#endif
		static BufferedInput buffered_input(standard_input);
		return buffered_input;
	}
};

class StandardOutput {
public:
	static BufferedOutput& get() {
		#ifdef _WIN32
		#else
		static WriteFile standard_output(STDOUT_FILENO);
		#endif
		static BufferedOutput buffered_output(standard_output);
		return buffered_output;
	}
};

class StandardError {
public:
	static BufferedOutput& get() {
		#ifdef _WIN32
		#else
		static WriteFile standard_error(STDERR_FILENO);
		#endif
		static BufferedOutput buffered_output(standard_error);
		return buffered_output;
	}
};

class Process {
	#ifdef _WIN32
	#else
	pid_t pid;
	#endif
public:
	WriteFile standard_input;
private:
	#ifdef _WIN32
	#else
	template <class T> static T* malloc(std::size_t size) {
		return static_cast<T*>(std::malloc(sizeof(T) * size));
	}
	static char* malloc_string(const StringView& s) {
		char* result = malloc<char>(s.size() + 1);
		std::memcpy(result, s.data(), s.size());
		result[s.size()] = '\0';
		return result;
	}
	static char** malloc_arguments(const StringView& program, const std::vector<StringView>& arguments) {
		char** result = malloc<char*>(arguments.size() + 2);
		result[0] = malloc_string(program);
		for (std::size_t i = 0; i < arguments.size(); ++i) {
			result[i + 1] = malloc_string(arguments[i]);
		}
		result[arguments.size() + 1] = nullptr;
		return result;
	}
	Process(pid_t pid, int stdin_fd): pid(pid), standard_input(stdin_fd) {}
	#endif
public:
	static Process spawn(const StringView& program, const std::vector<StringView>& arguments) {
		#ifdef _WIN32
		#else
		int stdin_pipe[2];
		pipe2(stdin_pipe, O_CLOEXEC);
		pid_t pid = fork();
		if (pid == 0) {
			dup2(stdin_pipe[0], STDIN_FILENO);
			char** arguments_ = malloc_arguments(program, arguments);
			execvp(arguments_[0], arguments_);
			_exit(EXIT_FAILURE);
		}
		close(stdin_pipe[0]);
		return Process(pid, stdin_pipe[1]);
		#endif
	}
	Process(const Process&) = delete;
	Process& operator =(const Process&) = delete;
	int join() {
		#ifdef _WIN32
		#else
		standard_input = WriteFile();
		int status;
		waitpid(pid, &status, 0);
		return WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
		#endif
	}
};
