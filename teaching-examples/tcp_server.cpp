#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    // 1. IPv4 + TCP 소켓 생성. 반환값은 포트가 아닌 파일 디스크립터다.
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        std::perror("socket");
        return 1;
    }

    // 2. 서버 재실행 시 주소 재사용. 이미 실행 중인 서버와 공유하는 옵션은 아니다.
    int reuse = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) == -1)
    {
        std::perror("setsockopt");
        close(serverSocket);
        return 1;
    }

    // 3. 루프백 IP와 포트 지정. 같은 WSL 내부에서만 접속한다.
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(18080); // 포트 값을 네트워크 바이트 순서로 변환
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) == -1)
    {
        std::perror("bind");
        close(serverSocket);
        return 1;
    }

    // 4. 연결을 기다리는 소켓으로 전환한다.
    if (listen(serverSocket, 5) == -1)
    {
        std::perror("listen");
        close(serverSocket);
        return 1;
    }
    std::cout << "127.0.0.1:18080에서 연결을 기다립니다." << std::endl;

    // 5. 특정 클라이언트와 통신할 새 소켓을 받는다.
    int clientSocket;
    do
    {
        clientSocket = accept(serverSocket, nullptr, nullptr);
    } while (clientSocket == -1 && errno == EINTR);

    if (clientSocket == -1)
    {
        std::perror("accept");
        close(serverSocket);
        return 1;
    }

    // 6. TCP는 메시지 경계가 없다. 여기서는 줄바꿈을 메시지 끝으로 약속한다.
    std::string message;
    bool success = true;

    while (message.empty() || message.back() != '\n')
    {
        if (message.size() >= 4096)
        {
            std::cerr << "메시지가 최대 허용 길이(4096바이트)를 초과했습니다.\n";
            success = false;
            break;
        }
        char ch;
        ssize_t received = recv(clientSocket, &ch, 1, 0);
        if (received == -1 && errno == EINTR)
        {
            continue;
        }
        if (received <= 0)
        {
            if (received == -1) std::perror("recv");
            else std::cerr << "메시지 수신이 완료되기 전에 클라이언트가 연결을 종료했습니다.\n";
            success = false;
            break;
        }
        message.push_back(ch);

    }

    // 7. 수신한 데이터를 그대로 돌려준다(Echo).
    // send()가 일부만 처리할 수 있으므로 남은 바이트를 반복 전송한다.
    std::size_t totalSent = 0;
    while (success && totalSent < message.size())
    {
        ssize_t sent = send(clientSocket, message.data() + totalSent,
                            message.size() - totalSent, MSG_NOSIGNAL);
        if (sent == -1 && errno == EINTR) continue;
        if (sent <= 0)
        {
            if (sent == -1) std::perror("send");
            else std::cerr << "데이터를 전송하지 못했습니다.\n";
            success = false;
            break;
        }
        totalSent += static_cast<std::size_t>(sent);
    }

    if (success) std::cout << "Echo: " << message << std::endl;

    // 8. 통신 소켓과 대기 소켓은 별개이므로 각각 닫는다.
    close(clientSocket);
    close(serverSocket);
    return success ? 0 : 1;
}
