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

		// ��� content_stream �ĳ���(ʹ�� content.tellp() ���)
		content_stream.seekp(0, ios::end);

		response << "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
	};

	//Ĭ�ϴ򿪵��ļ�
	server.default_resource["^/?(.*)$"]["GET"] = [](ostream& response, Request& request) {
		string filename = "www/";

		string path = request.path_match[1];

		// ��ֹʹ�� `..` ������ web/ Ŀ¼�������
		size_t last_pos = path.rfind(".");  //�������һ��"."���ֵ�λ��
		size_t current_pos = 0;
		size_t pos;
		while ((pos = path.find('.', current_pos)) != string::npos && pos != last_pos) {  //nposΪ�ַ�����β
			current_pos = pos;//ÿ������ǰ��һ�Σ�find����ֻ����curren_pos֮����ַ�
			path.erase(pos, 1); //�ҵ�"."��ɾ��
			last_pos--;
		}

		ifstream ifs;
		// �򵥵�ƽ̨�޹ص��ļ���Ŀ¼���
		if (filename.find('.') == string::npos) {
			if (filename[filename.length() - 1] != '/')
				filename += '/';
			filename += "index.html";
		}
		ifs.open(filename, ifstream::in);

		if (ifs) {
			ifs.seekg(0, ios::end);  //��λ����������β
			size_t length = ifs.tellg();  //��λ����β����Ի�ȡ�ܴ�С
			ifs.seekg(0, ios::beg);  //�ض�λ�ص���ͷ
			// �ļ����ݿ����� response-stream �У���Ӧ�����ڴ����ļ�
			response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n" << ifs.rdbuf();

			ifs.close();
		}
		else {
			// �ļ�������ʱ�������޷����ļ�
			string content = "Could not open file " + filename;
			response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
		}
	};

	server.start();
}