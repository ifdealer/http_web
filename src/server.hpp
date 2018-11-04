#pragma once 
#include<boost\asio.hpp>
#include<regex>
#include<unordered_map>


namespace http_web {
	//http请求内容结构体，如果对这部分不明白可以去了解一下http的请求机制
	struct Request {
		//请求的方法，请求路径，HTTP的版本
		std::string method, path, http_version;
		//请求内容
		std::shared_ptr<std::istream> content;
		//其他请求头，保存在无序map中，方便查找
		std::unordered_map<std::string, std::string>header;
		//用于正则表达式进行处理路径的匹配
		std::smatch path_match;
	};
	//使用typedef简化请求的表示方式
	//这里做了请求资源约定，第一个string为请求路径，第二个string为请求方法
	//map中对特定的请求路径和方法封装一个函数用于处理请求的资源(例如html文件)
	//https://en.cppreference.com/w/cpp/utility/functional/function 对function不了解可以看一下官方文档
	typedef std::map<std::string, std::unordered_map<std::string,
	std::function<void(std::ostream&, Request&) >> > resource_type;
	
	//http服务器类
	class Http_Server {
	private:
		typedef boost::asio::ip::tcp::socket HTTP;
		//io_service是用于处理异步事件
		boost::asio::io_service boost_io_service;
		boost::asio::ip::tcp::endpoint endpoint;
		boost::asio::ip::tcp::acceptor acceptor;
	public:
		//构造函数，用端口进行初始化，需要改进，当端口被占用或不可用时没有进行限制或异常捕捉
		Http_Server(unsigned short port):endpoint(boost::asio::ip::tcp::v4(),port),
			acceptor(boost_io_service,endpoint){}
		std::vector<resource_type::iterator> all_resources;

		//资源文件
		resource_type resource;
		resource_type default_resource;

		void start();
		//接收连接
		void accept();
		//对socket进行处理,处理请求信息和做出respond
		void process_request_and_respond(std::shared_ptr<HTTP> socket) const;
		//解析请求
		Request parse_request(std::istream& stream) const;
		void respond(std::shared_ptr<HTTP> socket, std::shared_ptr<Request> request) const;

	};


	void Http_Server::start() {
		// 默认资源放在 vector 的末尾, 用作应答方法
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
		//为当前连接创建一个socket,每个socket初始化时，需要传入一个io_service
		//以这个socket建立连接并且在lambda表达式的捕获列表中捕获用于处理socket中的信息
		auto socket = std::make_shared<HTTP>(boost_io_service);
		acceptor.async_accept(*socket, [this,socket](const boost::system::error_code& ec) {
			//在异步事件中再次调用accept来等待接收一个新的连接，这样就可以保持不断的接收新连接
			accept();
			//处理信息
			if (!ec) process_request_and_respond(socket);
		});
	}

	void Http_Server::process_request_and_respond(std::shared_ptr<HTTP> socket) const {
		auto read_buffer = std::make_shared<boost::asio::streambuf>();
		boost::asio::async_read_until(*socket, *read_buffer, "\r\n\r\n",
			[this, socket, read_buffer](const boost::system::error_code& ec, size_t bytes) {
			if (!ec) {
				//官方文档中指出，在读取到分隔符停止后，实际还会读取多一部分数据在分隔符之后
				//所以在分隔符"\r\n\r\n"之后还有一部份数据，其实这部分数据就是http请求体的开头部分
				//所以我们通过先将请求头解析出来，然后在拼接请求体
				size_t total = read_buffer->size();
				// 转换到 istream 来提取 string-lines,get()方法用于获取原始指针
				std::istream stream(read_buffer.get());
				auto request = std::make_shared<Request>();
				*request = parse_request(stream);

				//官方文档中指出，如果多余的数据没有取出来会留在缓冲区中，在这里就是read_buffer
				//所以下面这个add_bytes就是已经读取了的请求体的一部分,所以我们要使用
				//["Content-Length"])-add_bytes,来获取剩下的请求体部分
				size_t add_bytes = total - bytes;
				
				if (request->header.count("Content-Length") > 0) {
					boost::asio::async_read(
						*socket, *read_buffer,
						boost::asio::transfer_exactly(std::stoull(request->header["Content-Length"])-add_bytes),
						[this, socket, read_buffer, request](const boost::system::error_code& ec, size_t bytes) {
						if (!ec) {
							// 将指针作为 istream 对象存储到 read_buffer 中
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

		//从第一行中解析请求方法、路径和 HTTP 版本
		//这里的正则表达式和python类似，小括号中的就是分组，这些分组可以通过下标形式来进行访问,sub_match[1]  为分组1
		std::string line;
		getline(stream, line);
		line.pop_back(); //去掉换行符
		if (std::regex_match(line, sub_match, e)) {
			request.method = sub_match[1];
			request.path = sub_match[2];
			request.http_version = sub_match[3];

			bool matched;
			e = "^([^:]*): ?(.*)$";
			// 解析头部的其他信息
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
		// 对请求路径和方法进行匹配查找，并生成响应
		for (auto res_it : all_resources) {  //遍历所有资源
			std::regex e(res_it->first);  //资源第一个为请求路径的正则表达式，用正则匹配来判断请求的是哪个资源
			std::smatch sm_res;
			if (std::regex_match(request->path, sm_res, e)) {  //如果路径的正则匹配和请求头中的path一致的话
				if (res_it->second.count(request->method)>0) {  //资源请求的second为请求的方法
					request->path_match = sm_res;  //将匹配结果存入path_match

					auto write_buffer = std::make_shared<boost::asio::streambuf>();
					//构建一个输出流
					std::ostream response(write_buffer.get());
					//调用在handler中封装好的函数，传入一个ostream和一个request
					//调用完该函数之后会将资源文件以流的形式存入ostream,然后再后面调用异步写入到socket
					res_it->second[request->method](response, *request);
					// 在 lambda 中捕获 write_buffer 使其不会再 async_write 完成前被销毁
					boost::asio::async_write(*socket, *write_buffer,
						[this, socket, request, write_buffer](const boost::system::error_code& ec, size_t bytes_transferred) {
						if (!ec)
							process_request_and_respond(socket);//继续异步处理下一个连接
					});
					return;
				}
			}
		}
	}

}