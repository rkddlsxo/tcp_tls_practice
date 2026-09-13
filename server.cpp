#include <cstdio>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0); // socket 함수
	if (serverSocket == -1)
	{
		std::perror("socket");
		return 1;
	}

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_port = htons(8080); // 포트 지정
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 루프백 지정

	int reuse = 1;

	if (setsockopt(
		serverSocket,
		SOL_SOCKET,
		SO_REUSEADDR,
		&reuse,
		sizeof(reuse)) == -1)
	{
		std::perror("setsockopt");
		close(serverSocket);
		return 1;
	}

	if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1)
	{
		std::perror("bind");
		close(serverSocket);
		return 1;
	}

	if (listen(serverSocket, 5) == -1)
	{
		std::perror("listen:");
		close(serverSocket);
		return 1;
	}

	int clientSocket = accept(serverSocket, nullptr, nullptr);

	if (clientSocket == -1)
	{
		std::perror("accept");
		close(serverSocket);
		return 1;
	}

	char buffer[1024]{};

	ssize_t receivedBytes = recv(
		clientSocket,
		buffer,
		sizeof(buffer),
		0
	);

	if (receivedBytes == -1)
	{
		std::perror("recv");
		close(clientSocket);
		close(serverSocket);
		return 1;
	}

	if (receivedBytes == 0)
	{
		std::cout << "상대가 연결을 종료했어." << std::endl;
	}
	else
	{
		std::cout << "받은 내용: ";
		std::cout.write(buffer, receivedBytes);
		std::cout << std::endl;

		ssize_t sentBytes = send(
			clientSocket,
			buffer,
			receivedBytes,
			0
		);

		if (sentBytes == -1)
		{
			std::perror("send");
			close(clientSocket);
			close(serverSocket);
			return 1;
		}

		std::cout << "돌려보낸 바이트 수: "
			<< sentBytes << std::endl;
	}

	close(serverSocket);
	return 0;
}
