#include <stdio.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define MAXLINE 3000
#define errquit(m)	{ perror(m); exit(-1); }

static int port_http = 80;
static int port_https = 443;
static const char *docroot = "/html";
void proccessRequest(pid_t);

class HttpResponse{
public:
	std::string version;
	int statusCode;
	std::string reason;
	std::string contentType;
	int contentLength;
	std::string body;
	HttpResponse(){
		// bzero(&version, sizeof(version));
		// bzero(&reason, sizeof(reason));
		// bzero(&contentType, sizeof(contentType));
		// bzero(&body, sizeof(body));
		contentLength = statusCode = 0;
	}
	void generatePacket(char* buffer){
		sprintf(buffer, "%s %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n%s\r\n", 
			version.c_str(), statusCode, reason.c_str(), contentType.c_str(), contentLength, body.c_str());
	}
};

int main(int argc, char *argv[]) {
	int s;
	struct sockaddr_in sin;

	if(argc > 1) { port_http  = strtol(argv[1], NULL, 0); }
	if(argc > 2) { if((docroot = strdup(argv[2])) == NULL) errquit("strdup"); }
	if(argc > 3) { port_https = strtol(argv[3], NULL, 0); }

	if((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {errquit("socket");}
	else {std::cout << "socket created\n";}

	int v = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));

	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(80);
	sin.sin_addr.s_addr = htonl(INADDR_ANY); 
	if(bind(s, (struct sockaddr*) &sin, sizeof(sin)) < 0) {errquit("bind");}
	else {std::cout << "bind successfully\n";}
	if(listen(s, SOMAXCONN) < 0) {errquit("listen");}
	else {std::cout << "listening\n";}

	do {
		int c;
		struct sockaddr_in csin;
		socklen_t csinlen = sizeof(csin);

		if((c = accept(s, (struct sockaddr*) &csin, &csinlen)) < 0) {
			perror("accept");
			// continue; 
			exit(-1);
		} else {
			std::cout << "accept working\n";
		}
		pid_t child = fork();
		
		if(child == 0){
			// handle request here	
			close(s);
			proccessRequest(c);
			// char buffer[MAXLINE];
			// int n;
			// if((n = read(c, buffer, MAXLINE)) < 0) errquit("read");
			// std::cout << buffer << "\n";
			close(c);
			exit(EXIT_SUCCESS);
		}
	} while(1);

	return 0;
}
void proccessRequest(pid_t c){
	int n;
	char buffer[MAXLINE];
	if((n = read(c, buffer, MAXLINE)) < 0) errquit("read");
	char method[10], uri[30];
	bzero(&method, sizeof(method)); bzero(&uri, sizeof(uri));
	HttpResponse response;
	std::stringstream ss;
	ss.clear();
	ss << buffer;
	ss >> method >> uri >> response.version;

	response.statusCode = 200;
	response.contentType = "text/plain;charset=utf-8";
	response.reason = "OK";
	response.body = "It Works.";
	response.contentLength = response.body.length() + 2;
	bzero(&buffer, sizeof(buffer));
	response.generatePacket(buffer);
	std::cout << buffer;
	if((n = write(c, buffer, strlen(buffer))) < 0) errquit("write");
	return;
}