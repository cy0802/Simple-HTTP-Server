#include <stdio.h>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <set>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#define MAXLINE 3000
#define errquit(m)	{ perror(m); exit(-1); }

static int port_http = 80;
static int port_https = 443;
static const char *docroot = "/html";
std::set<std::string> files;
void proccessRequest(pid_t);
void getAllFiles();

class HttpResponse{
public:
	std::string version;
	int statusCode;
	std::string reason;
	std::string contentType;
	int contentLength;
	std::string body;
	HttpResponse(){
		contentLength = statusCode = 0;
	}
	void generatePacket(char* buffer, bool error){
		if(error) {
			sprintf(buffer, "%s %d %s\r\nContent-Length: 0\r\n\r\n\r\n", version.c_str(), statusCode, reason.c_str());
		} else {
			sprintf(buffer, "%s %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n%s\r\n", 
				version.c_str(), statusCode, reason.c_str(), contentType.c_str(), contentLength, body.c_str());
		}
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

	if(chdir(docroot) == -1) errquit("chdir");
	// std::cout << getcwd(NULL, 1024) << '\n';
	getAllFiles();

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
	// char method[10], uri[30];
	// bzero(&method, sizeof(method)); bzero(&uri, sizeof(uri));
	std::string method, uri;
	HttpResponse response;
	std::stringstream ss;
	std::set<std::string>::iterator it;
	ss.clear();
	ss << buffer;
	ss >> method >> uri >> response.version;
	bzero(&buffer, sizeof(buffer));
	if(method != "GET"){
		response.statusCode = 501;
		response.reason = "Not Implemented";
		response.generatePacket(buffer, 1);
	} else if((it = std::find(files.begin(), files.end(), uri)) == files.end()){
		if(uri.back() == '/'){
			uri.pop_back();
			if(std::find(files.begin(), files.end(), uri) != files.end()){
				// 301
				// redirect ?
				response.statusCode = 301;
				response.reason = "Moved Permanently";
				response.generatePacket(buffer, 1);
			} else {
				response.statusCode = 404;
				response.reason = "Not Found";
				response.generatePacket(buffer, 1);
			}
		} else {
			response.statusCode = 404;
			response.reason = "Not Found";
			response.generatePacket(buffer, 1);
		}
	} else {
		// check if this is a folder
		struct stat s;
		std::string path = "." + uri;
		if(stat(path.c_str(), &s) == 0){
			if(s.st_mode & S_IFDIR){
				// a directory
				response.statusCode = 404;
				response.reason = "Not Found";
				response.contentLength = 0;
				response.generatePacket(buffer, 1);
			} else {
				// regular response
				response.statusCode = 200;
				response.contentType = "text/plain;charset=utf-8";
				response.reason = "OK";
				response.body = "It Works.";
				response.contentLength = response.body.length() + 2;
				response.generatePacket(buffer, 0);
			}
		}
		
	}
	
	
	std::cout << buffer;
	if((n = write(c, buffer, strlen(buffer))) < 0) errquit("write");
	return;
}

void getAllFiles(){
	for (const auto& dir : std::filesystem::recursive_directory_iterator("./")){
		std::string path = dir.path().string().erase(0, 1);
		files.insert(path);
		// std::cout << path << '\n';
	}
}