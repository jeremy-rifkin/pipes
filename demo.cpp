#include <iostream>
#include <string>
#include <string_view>
#include "pipe.hpp"

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

int main() {
	std::cout<<cowsay("foo bar");
}
