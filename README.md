## Pipes

This is a proof of concept abstraction for POSIX pipes. Pipes, especially when used in the context
of fork + exec, require a lot of careful management of file descriptors.

Abstracting this is challenging: Working with pipes requires a lot of awareness of what's going on
under the hood.

By assuming a process will only ever write xor read to/from a pipe we can abstract away a lot of
this file descriptor management and create a relatively simple, elegant, and expressive abstraction.

Consider this:

```cpp
std::string cowsay(std::string_view data) {
	// setup pipes
	pipe_t output_pipe;
	pipe_t input_pipe;
	// launch
	pid_t pid = fork();
	if(pid == -1) return "";
	if(pid == 0) {
		// child: attach pipe ends
		output_pipe.attach(STDOUT_FILENO, pipe_t::write_end);
		input_pipe .attach(pipe_t::read_end, STDIN_FILENO);
		execlp("cowsay", "cowsay", nullptr);
		_exit(1);
	}
	// parent: write to the input and read
	input_pipe.write(data);
	input_pipe.close();
	std::string output = output_pipe.read();
	// re-join child
	while(waitpid(pid, NULL, 0) == -1 && errno == EINTR); // -1 && errno == EINTR handles interrupt
	return output;
}
```

Vs:

```cpp
std::string cowsay(std::string_view data) {
	// setup pipes
	int output_pipe[2];
	int input_pipe[2];
	pipe(output_pipe);
	pipe(input_pipe);
	// launch
	pid_t pid = fork();
	if(pid == -1) return "";
	if(pid == 0) {
		// child: attach pipe ends
		dup2(output_pipe[1], STDOUT_FILENO);
		dup2(input_pipe[0], STDIN_FILENO);
		close(output_pipe[0]);
		close(output_pipe[1]);
		close(input_pipe[0]);
		close(input_pipe[1]);
		execlp("cowsay", "cowsay", nullptr);
		_exit(1);
	}
	// parent: write to the input and read
	close(output_pipe[1]);
	close(input_pipe[0]);
	write(input_pipe[1], data.data(), data.size());
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
