## Pipes

This is a proof of concept abstraction for POSIX pipes. Pipes, especially when used in the context
of fork + exec, require a lot of careful management of file descriptors. Abstracting this is
challenging - working with pipes requires a lot of awareness of what's going on under the hood.

By assuming a process will only ever write xor read to/from a pipe we can abstract away a lot of
this file descriptor management and create a relatively simple, elegant, and expressive abstraction.

Consider this:

```cpp
std::string cowsay(std::string_view str) {
	// setup pipes
	pipe_t input_pipe;
	pipe_t output_pipe;
	// launch
	pid_t pid = fork();
	if(pid == -1) return "";
	if(pid == 0) {
		// child: attach pipe ends
		input_pipe .attach(pipe_t::read_end, STDIN_FILENO);
		output_pipe.attach(STDOUT_FILENO, pipe_t::write_end);
		execlp("cowsay", "cowsay", nullptr);
		_exit(1);
	}
	// parent: write to the input and read
	input_pipe.write(str);
	input_pipe.close();
	std::string output = output_pipe.read();
	// re-join child
	while(waitpid(pid, NULL, 0) == -1 && errno == EINTR); // -1 && errno == EINTR handles interrupt
	return output;
}
```

Vs:

```cpp
std::string cowsay(std::string_view str) {
	// setup pipes
	int input_pipe[2];
	int output_pipe[2];
	pipe(input_pipe);
	pipe(output_pipe);
	// launch
	pid_t pid = fork();
	if(pid == -1) return "";
	if(pid == 0) {
		// child: attach pipe ends
		dup2(input_pipe[0], STDIN_FILENO);
		dup2(output_pipe[1], STDOUT_FILENO);
		close(input_pipe[0]);
		close(input_pipe[1]);
		close(output_pipe[0]);
		close(output_pipe[1]);
		execlp("cowsay", "cowsay", nullptr);
		_exit(1);
	}
	// parent: write to the input and read
	close(input_pipe[0]);
	close(output_pipe[1]);
	write(input_pipe[1], str.data(), str.size());
	close(input_pipe[1]);
	std::string output;
	constexpr int buffer_size = 4096;
	char buffer[buffer_size];
	size_t count = 0;
	while((count = read(output_pipe[0], buffer, buffer_size)) > 0) {
		output.insert(output.end(), buffer, buffer + count);
	}
	close(output_pipe[0]);
	// re-join child
	while(waitpid(pid, NULL, 0) == -1 && errno == EINTR); // -1 && errno == EINTR handles interrupt
	return output;
}
```

And this is without all the error handling.
