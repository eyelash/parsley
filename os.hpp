#pragma once

#include "common.hpp"
#include <cstring>
#include <cerrno>

#ifdef _WIN32
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

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
