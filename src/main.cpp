#include "handler.hpp"
using namespace http_web;

int main() {
	http_web::Http_Server server(8102);
	start_server(server);
	return 0;
}