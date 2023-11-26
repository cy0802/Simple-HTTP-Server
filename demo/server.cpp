#include <stdio.h>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <set>
#include <map>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#define MAXLINE 100000
#define errquit(m)	{ perror(m); exit(-1); }

static int port_http = 80;
static int port_https = 443;
static const char *docroot = "/html";
char buffer[MAXLINE];
std::set<std::string> files;
std::map<std::string, std::string> mime = {
	{"html", "text/html"},
	{"htm", "text/html"},
	{"txt", "text/plain;charset=utf-8"},
	{"jpg", "image/jpeg"},
	{"jpeg", "image/jpeg"},
	{"png", "image/png"},
	{"mp3", "audio/mpeg"}
};

class HttpResponse{
public:
	std::string version;
	int statusCode;
	std::string reason;
	std::string contentType;
	int contentLength;
	HttpResponse(){
		contentLength = statusCode = 0;
	}
	inline void generatePacket(char* buffer){
		sprintf(buffer, "%s %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", 
			version.c_str(), statusCode, reason.c_str(), contentType.c_str(), contentLength);
	}
};

void proccessRequest(pid_t);
void getAllFiles();
std::string getContentType(std::string uri);
void sendFile(int sockfd, HttpResponse response, std::string path);

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
	
	// get rid of not needed parameter
	int pos = uri.find_first_of("?");
	uri = uri.substr(0, pos);
	
	bzero(&buffer, sizeof(buffer));
	if(uri == "/"){
		uri = "/index.html";
		auto it = std::find(files.begin(), files.end(), uri);
		if(it == files.end()){
			response.statusCode = 403;
			response.reason = "Forbidden";
			response.generatePacket(buffer);
			if((n = write(c, buffer, strlen(buffer))) < 0) errquit("write");
			return;
		}
	}
	if(method != "GET"){
		// method not supported
		response.statusCode = 501;
		response.reason = "Not Implemented";
		response.generatePacket(buffer);
		if((n = write(c, buffer, strlen(buffer))) < 0) errquit("write");
		return;
	} else if((it = std::find(files.begin(), files.end(), uri)) == files.end()){
		if(uri.back() == '/'){
			uri.pop_back();
			if(std::find(files.begin(), files.end(), uri) != files.end()){
				// request a folder without slash
				response.statusCode = 301;
				response.reason = "Moved Permanently";
				response.generatePacket(buffer);
				if((n = write(c, buffer, strlen(buffer))) < 0) errquit("write");
				return;
			} else {
				// request not existed file
				response.statusCode = 404;
				response.reason = "Not Found";
				response.generatePacket(buffer);
				if((n = write(c, buffer, strlen(buffer))) < 0) errquit("write");
				return;
			}
		} else {
			// request not existed file
			response.statusCode = 404;
			response.reason = "Not Found";
			response.generatePacket(buffer);
			if((n = write(c, buffer, strlen(buffer))) < 0) errquit("write");
			return;
		}
	} else {
		struct stat s;
		std::string path = "." + uri;
		if(stat(path.c_str(), &s) == 0){
			if(s.st_mode & S_IFDIR){
				// a directory
				response.statusCode = 404;
				response.reason = "Not Found";
				response.contentLength = 0;
				response.generatePacket(buffer);
				if((n = write(c, buffer, strlen(buffer))) < 0) errquit("write");
				return;
			} else {
				// regular response
				response.statusCode = 200;
				response.contentType = getContentType(uri);
				response.reason = "OK";
				sendFile(c, response, path);
				return;
				// response.body = readFile(path);
				// response.contentLength = response.body.length();
				// response.generatePacket(buffer);
				// if((n = write(c, buffer, strlen(buffer))) < 0) errquit("write");

			}
		}
		
	}
	// std::cout << buffer;
	// if((n = write(c, buffer, strlen(buffer))) < 0) errquit("write");
	return;
}

std::string getContentType(std::string uri){
	int pos = uri.find_first_of(".");
	std::string ext = uri.substr(pos + 1, uri.length() - pos - 1);
	// std::cout << ext << "\n";
	auto it = mime.find(ext);
	if(it == mime.end()) return "application/octet-stream";
	else return mime[ext];
}

void sendFile(int c, HttpResponse response, std::string path){
	std::ifstream file;
	int n;
	file.open(path, std::ios::binary); 
	if(file.fail()) errquit("std::fstream open:");

	// send header
	std::streampos tmp = file.tellg();
    file.seekg(0, std::ios::end);
    response.contentLength = file.tellg() - tmp;
	bzero(&buffer, sizeof(buffer));
	response.generatePacket(buffer);
	if((n = write(c, buffer, strlen(buffer))) < 0) errquit("write");
	std::cout << "header sent\n";

	file.seekg(0, std::ios::beg);
	bzero(&buffer, sizeof(buffer));
	while(file.read(buffer, sizeof(buffer))){
		std::cout << "sending file...\n";
		if((n = write(c, buffer, sizeof(buffer))) < 0) errquit("write");
		bzero(&buffer, sizeof(buffer));
	}
	std::cout << "go out of while loop\n";
	std::cout << strlen(buffer) << "\n";
	if((n = write(c, buffer, sizeof(buffer))) < 0) errquit("write");
	file.close();
}

void getAllFiles(){
	for (const auto& dir : std::filesystem::recursive_directory_iterator("./")){
		std::string path = dir.path().string().erase(0, 1);
		files.insert(path);
		// std::cout << path << '\n';
	}
}