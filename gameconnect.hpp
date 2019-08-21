#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <string_view>
#include <string>
#include <queue>
#include <winsock.h>

using boost::asio::ip::udp;

namespace Jin {

  constexpr std::size_t MAX_SIZE = 256;
  using Data = char[MAX_SIZE];

  [[nodiscard]] std::string getIP() {//���̊֐������s�����}�V����IP�A�h���X��Ԃ�
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(1, 1), &wsadata) != 0)throw "Cannot get your IP address.";
	char hostname[256];//�z�X�g�l�[���̎擾
	if (gethostname(hostname, sizeof(hostname)) != 0)throw "Cannot get your IP address.";
	HOSTENT * hostend = gethostbyname(hostname);
	if (hostend == NULL)throw "Cannot get your IP address.";
	IN_ADDR inaddr;
	memcpy(&inaddr, hostend->h_addr_list[0], 4);
	char tmpIP[256];
	strcpy_s(tmpIP, inet_ntoa(inaddr));
	std::string IP = std::string(tmpIP);
	WSACleanup();
	return IP;
  }

  class GameConnect {
  private:
	std::string IP;
	boost::asio::io_context ioc;
	udp::resolver resolver{ ioc };
	std::queue<std::decay_t<Data>> temporary_data;
	boost::thread sub;
	void serverside(unsigned short port) {//�T�[�o�[�𗧂Ă�
	  udp::socket socket(ioc, udp::endpoint(udp::v4(), port));
	  Data buf = {};
	  udp::endpoint endpoint;
	  for (;;) {
		socket.receive_from(boost::asio::buffer(buf), endpoint);
		temporary_data.push(buf);//�f�[�^��������L���[�Ƀv�b�V������
	  }
	}

  public:
	explicit GameConnect(unsigned short port = 3000) : sub(&GameConnect::serverside, this, port) {}//�|�[�g�ԍ����w�肵�ă\�P�b�g���J��
	~GameConnect() { sub.interrupt(); }

	void Send(std::string_view IP, unsigned short port, Data& data) {//IP��port��data�𑗐M����
	  udp::socket socket(ioc);
	  udp::endpoint endpoint = *resolver.resolve(udp::v4(), IP, std::to_string(port)).begin();
	  socket.open(udp::v4());
	  socket.send_to(boost::asio::buffer(data), endpoint);
	}

	const char* Get() {//���M���ꂽ�f�[�^��ǂ�
	  while (temporary_data.empty());//�����L���[�̒��g���󂾂����炸���Ƒ҂�
	  auto res = temporary_data.front();
	  temporary_data.pop();
	  return res;
	}

  };

}