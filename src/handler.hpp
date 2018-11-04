#pragma once 
#include"server.hpp"
#include<fstream>
#include<iostream>
using namespace std;
using namespace http_web;

void start_server(http_web::Http_Server& server) {
	server.resource["^/info/?$"]["GET"] = [](ostream& response, http_web::Request& request) {
		stringstream content_stream;
		content_stream << "<h1>Request:</h1>";
		content_stream << request.method << " " << request.path << " HTTP/" << request.http_version << "<br>";
		for (auto& header : request.header) {
			content_stream << header.first << ": " << header.second << "<br>";
		}

		// 获得 content_stream 的长度(使用 content.tellp() 获得)
		content_stream.seekp(0, ios::end);

		response << "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
	};

	//默认打开的文件
	server.default_resource["^/?(.*)$"]["GET"] = [](ostream& response, Request& request) {
		string filename = "www/";

		string path = request.path_match[1];

		// 防止使用 `..` 来访问 web/ 目录外的内容
		size_t last_pos = path.rfind(".");  //查找最后一个"."出现的位置
		size_t current_pos = 0;
		size_t pos;
		while ((pos = path.find('.', current_pos)) != string::npos && pos != last_pos) {  //npos为字符串结尾
			current_pos = pos;//每次搜索前进一次，find函数只搜索curren_pos之后的字符
			path.erase(pos, 1); //找到"."并删除
			last_pos--;
		}

		ifstream ifs;
		// 简单的平台无关的文件或目录检查
		if (filename.find('.') == string::npos) {
			if (filename[filename.length() - 1] != '/')
				filename += '/';
			filename += "index.html";
		}
		ifs.open(filename, ifstream::in);

		if (ifs) {
			ifs.seekg(0, ios::end);  //定位到输入流结尾
			size_t length = ifs.tellg();  //定位到结尾后可以获取总大小
			ifs.seekg(0, ios::beg);  //重定位回到开头
			// 文件内容拷贝到 response-stream 中，不应该用于大型文件
			response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n" << ifs.rdbuf();

			ifs.close();
		}
		else {
			// 文件不存在时，返回无法打开文件
			string content = "Could not open file " + filename;
			response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
		}
	};

	server.start();
} 
