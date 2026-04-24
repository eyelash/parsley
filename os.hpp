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

class ReadFile: public Input {
	#ifdef _WIN32
	#else
	int fd;
	#endif
public:
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
