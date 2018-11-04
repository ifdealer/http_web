#pragma once 
#include<boost\asio.hpp>
#include<regex>
#include<unordered_map>


namespace http_web {
	//http�������ݽṹ�壬������ⲿ�ֲ����׿���ȥ�˽�һ��http���������
	struct Request {
		//����ķ���������·����HTTP�İ汾
		std::string method, path, http_version;
		//��������
		std::shared_ptr<std::istream> content;
		//��������ͷ������������map�У��������
		std::unordered_map<std::string, std::string>header;
		//����������ʽ���д���·����ƥ��
		std::smatch path_match;
	};
	//ʹ��typedef������ı�ʾ��ʽ
	//��������������ԴԼ������һ��stringΪ����·�����ڶ���stringΪ���󷽷�
	//map�ж��ض�������·���ͷ�����װһ���������ڴ����������Դ(����html�ļ�)
	//https://en.cppreference.com/w/cpp/utility/functional/function ��function���˽���Կ�һ�¹ٷ��ĵ�
	typedef std::map<std::string, std::unordered_map<std::string,
	std::function<void(std::ostream&, Request&) >> > resource_type;
	
	//http��������
	class Http_Server {
	private:
		typedef boost::asio::ip::tcp::socket HTTP;
		//io_service�����ڴ����첽�¼�
		boost::asio::io_service boost_io_service;
		boost::asio::ip::tcp::endpoint endpoint;
		boost::asio::ip::tcp::acceptor acceptor;
	public:
		//���캯�����ö˿ڽ��г�ʼ������Ҫ�Ľ������˿ڱ�ռ�û򲻿���ʱû�н������ƻ��쳣��׽
		Http_Server(unsigned short port):endpoint(boost::asio::ip::tcp::v4(),port),
			acceptor(boost_io_service,endpoint){}
		std::vector<resource_type::iterator> all_resources;

		//��Դ�ļ�
		resource_type resource;
		resource_type default_resource;

		void start();
		//��������
		void accept();
		//��socket���д���,����������Ϣ������respond
		void process_request_and_respond(std::shared_ptr<HTTP> socket) const;
		//��������
		Request parse_request(std::istream& stream) const;
		void respond(std::shared_ptr<HTTP> socket, std::shared_ptr<Request> request) const;

	};


	void Http_Server::start() {
		// Ĭ����Դ���� vector ��ĩβ, ����Ӧ�𷽷�
		for (auto it = resource.begin(); it != resource.end(); it++) {
			all_resources.push_back(it);
		}
		for (auto it = default_resource.begin(); it != default_resource.end(); it++) {
			all_resources.push_back(it);
		}
		accept();
		boost_io_service.run();
	}
	

	void Http_Server::accept() {
		//Ϊ��ǰ���Ӵ���һ��socket,ÿ��socket��ʼ��ʱ����Ҫ����һ��io_service
		//�����socket�������Ӳ�����lambda���ʽ�Ĳ����б��в������ڴ���socket�е���Ϣ
		auto socket = std::make_shared<HTTP>(boost_io_service);
		acceptor.async_accept(*socket, [this,socket](const boost::system::error_code& ec) {
			//���첽�¼����ٴε���accept���ȴ�����һ���µ����ӣ������Ϳ��Ա��ֲ��ϵĽ���������
			accept();
			//������Ϣ
			if (!ec) process_request_and_respond(socket);
		});
	}

	void Http_Server::process_request_and_respond(std::shared_ptr<HTTP> socket) const {
		auto read_buffer = std::make_shared<boost::asio::streambuf>();
		boost::asio::async_read_until(*socket, *read_buffer, "\r\n\r\n",
			[this, socket, read_buffer](const boost::system::error_code& ec, size_t bytes) {
			if (!ec) {
				//�ٷ��ĵ���ָ�����ڶ�ȡ���ָ���ֹͣ��ʵ�ʻ����ȡ��һ���������ڷָ���֮��
				//�����ڷָ���"\r\n\r\n"֮����һ�������ݣ���ʵ�ⲿ�����ݾ���http������Ŀ�ͷ����
				//��������ͨ���Ƚ�����ͷ����������Ȼ����ƴ��������
				size_t total = read_buffer->size();
				// ת���� istream ����ȡ string-lines,get()�������ڻ�ȡԭʼָ��
				std::istream stream(read_buffer.get());
				auto request = std::make_shared<Request>();
				*request = parse_request(stream);

				//�ٷ��ĵ���ָ����������������û��ȡ���������ڻ������У����������read_buffer
				//�����������add_bytes�����Ѿ���ȡ�˵��������һ����,��������Ҫʹ��
				//["Content-Length"])-add_bytes,����ȡʣ�µ������岿��
				size_t add_bytes = total - bytes;
				
				if (request->header.count("Content-Length") > 0) {
					boost::asio::async_read(
						*socket, *read_buffer,
						boost::asio::transfer_exactly(std::stoull(request->header["Content-Length"])-add_bytes),
						[this, socket, read_buffer, request](const boost::system::error_code& ec, size_t bytes) {
						if (!ec) {
							// ��ָ����Ϊ istream ����洢�� read_buffer ��
							request->content = std::shared_ptr<std::istream>(new std::istream(read_buffer.get()));
							respond(socket, request);
						}
					});
				}
				else { respond(socket, request); }
			}
		});
	}

	Request Http_Server::parse_request(std::istream& stream) const {
		Request request;
		std::regex e("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
		std::smatch sub_match;

		//�ӵ�һ���н������󷽷���·���� HTTP �汾
		//�����������ʽ��python���ƣ�С�����еľ��Ƿ��飬��Щ�������ͨ���±���ʽ�����з���,sub_match[1]  Ϊ����1
		std::string line;
		getline(stream, line);
		line.pop_back(); //ȥ�����з�
		if (std::regex_match(line, sub_match, e)) {
			request.method = sub_match[1];
			request.path = sub_match[2];
			request.http_version = sub_match[3];

			bool matched;
			e = "^([^:]*): ?(.*)$";
			// ����ͷ����������Ϣ
			do {
				getline(stream, line);
				line.pop_back();
				matched = std::regex_match(line, sub_match, e);
				if (matched) {
					request.header[sub_match[1]] = sub_match[2];
				}

			} while (matched == true);
		}
		return request;
	}
	void Http_Server::respond(std::shared_ptr<HTTP> socket, std::shared_ptr<Request> request) const {
		// ������·���ͷ�������ƥ����ң���������Ӧ
		for (auto res_it : all_resources) {  //����������Դ
			std::regex e(res_it->first);  //��Դ��һ��Ϊ����·����������ʽ��������ƥ�����ж���������ĸ���Դ
			std::smatch sm_res;
			if (std::regex_match(request->path, sm_res, e)) {  //���·��������ƥ�������ͷ�е�pathһ�µĻ�
				if (res_it->second.count(request->method)>0) {  //��Դ�����secondΪ����ķ���
					request->path_match = sm_res;  //��ƥ��������path_match

					auto write_buffer = std::make_shared<boost::asio::streambuf>();
					//����һ�������
					std::ostream response(write_buffer.get());
					//������handler�з�װ�õĺ���������һ��ostream��һ��request
					//������ú���֮��Ὣ��Դ�ļ���������ʽ����ostream,Ȼ���ٺ�������첽д�뵽socket
					res_it->second[request->method](response, *request);
					// �� lambda �в��� write_buffer ʹ�䲻���� async_write ���ǰ������
					boost::asio::async_write(*socket, *write_buffer,
						[this, socket, request, write_buffer](const boost::system::error_code& ec, size_t bytes_transferred) {
						if (!ec)
							process_request_and_respond(socket);//�����첽������һ������
					});
					return;
				}
			}
		}
	}

}