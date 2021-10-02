#ifndef PIPE_HPP
#define PIPE_HPP

#include <assert.h>
#include <stdio.h>
#include <string_view>
#include <string.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

// pipe, close, dup2, read, write all return -1 on fail
#define SYSCALL(call) do { \
	int r; \
	while((r = (call)) == -1 && errno == EINTR); \
	if(r == -1) { \
		fprintf(stderr, "system call %s failed with code %d: %s\n", #call, r, strerror(errno)); \
		abort(); \
	} \
} while(false)

// This is an abstraction for POSIX pipes. It relies on some assumptions in order to provide
// a high-level abstraction and ease of use for the programmer.
// NOTE: Attach should only be called once and will close old pipe ends
// NOTE: A process may only use .read or .write, they will close the other end
struct pipe_t {
	enum class pipe_end { read_end, write_end };
	//using enum pipe_end; // if your compiler doesn't support using enum...
	static constexpr pipe_end read_end = pipe_end::read_end;
	static constexpr pipe_end write_end = pipe_end::write_end;
	int pipe_fds[2];
	pipe_t() {
		SYSCALL(pipe(pipe_fds));
	}
	compl pipe_t() { close(); }
	void close_read_end() {
		if(pipe_fds[0] != -1) {
			SYSCALL(::close(pipe_fds[0]));
			pipe_fds[0] = -1;
		}
	}
	void close_write_end() {
		if(pipe_fds[1] != -1) {
			SYSCALL(::close(pipe_fds[1]));
			pipe_fds[1] = -1;
		}
	}
	void close() {
		// cleanup if we haven't already closed or dup2'd to a standard io stream
		close_write_end();
		close_read_end();
	}
	// pipe src -> the pipe, close both ends (unused end and old end) to cleanup
	// e.g. STDOUT -> pipe
	void attach(int src, pipe_end dest) {
		SYSCALL(dup2(pipe_fds[static_cast<std::underlying_type_t<pipe_end>>(dest)], src));
		close();
	}
	// pipe the pipe -> dest, close both ends (unused end and old end) to cleanup
	// e.g. pipe -> STDIN
	void attach(pipe_end src, int dest) {
		SYSCALL(dup2(pipe_fds[static_cast<std::underlying_type_t<pipe_end>>(src)], dest));
		close();
	}
	std::string read() {
		close_write_end();
		std::string output;
		constexpr int buffer_size = 4096;
		char buffer[buffer_size];
		ssize_t count = 0;
		while(true) {
			count = ::read(pipe_fds[0], buffer, buffer_size);
			if(count == 0) break;
			if(count == -1 && errno == EINTR) continue;
			output.insert(output.end(), buffer, buffer + count);
		}
		assert(count != -1);
		return output;
	}
	void write(const void* _data, size_t n) {
		close_read_end();
		const char* data = (const char*) _data;
		ssize_t count = 0;
		size_t transferred = 0;
		while(transferred != n) {
			count = ::write(pipe_fds[1], data, n);
			if(count == 0) return;
			if(count == -1 && errno == EINTR) continue;
			assert(count != -1);
			transferred += count;
		}
	}
	void write(std::string_view str) {
		write(str.data(), str.size());
	}
};

#endif
