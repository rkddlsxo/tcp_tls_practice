// 교육용 TLS Echo: TCP 연결 위에 인증서 검증과 암호화 통신을 추가한다.
// 프로토콜 약속: 줄바꿈(\n)이 메시지의 끝이다. 한 연결 처리 후 종료한다.
#include <csignal>
#include <iostream>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <cstdio>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

int main()
{
    // 상대가 먼저 닫아도 프로세스 종료 대신 TLS 오류를 처리한다.
    std::signal(SIGPIPE, SIG_IGN);

    // 1. TLS 서버의 공통 설정을 담을 객체 생성
    SSL_CTX* context = SSL_CTX_new(TLS_server_method());

    if (context == nullptr)
    {
        ERR_print_errors_fp(stderr);
        return 1;
    }

    // 2. TLS 1.2 이상만 허용
    if (SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }

    // 3. 서버 인증서 읽기
    if (SSL_CTX_use_certificate_file(
        context,
        "server-cert.pem",
        SSL_FILETYPE_PEM) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }

    // 4. 서버 개인키 읽기
    if (SSL_CTX_use_PrivateKey_file(
        context,
        "server-key.pem",
        SSL_FILETYPE_PEM) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }

    // 5. 인증서의 공개키와 개인키가 짝인지 확인
    if (SSL_CTX_check_private_key(context) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }

    std::cout << "서버 인증서와 개인키를 불러왔습니다." << std::endl;

    // TCP 소켓 생성
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == -1)
    {
        std::perror("socket");
        SSL_CTX_free(context);
        return 1;
    }

    // 서버 재실행 시 주소 재사용 허용
    int reuse = 1;

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
        &reuse, sizeof(reuse)) == -1)
    {
        std::perror("setsockopt");
        close(serverSocket);
        SSL_CTX_free(context);
        return 1;
    }

    // TLS 실습 서버는 18443번 포트 사용
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(18443);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(serverSocket,
        reinterpret_cast<sockaddr*>(&address),
        sizeof(address)) == -1)
    {
        std::perror("bind");
        close(serverSocket);
        SSL_CTX_free(context);
        return 1;
    }

    if (listen(serverSocket, 5) == -1)
    {
        std::perror("listen");
        close(serverSocket);
        SSL_CTX_free(context);
        return 1;
    }

    std::cout << "127.0.0.1:18443에서 연결을 기다립니다." << std::endl;

    int clientSocket = accept(serverSocket, nullptr, nullptr);

    if (clientSocket == -1)
    {
        std::perror("accept");
        close(serverSocket);
        SSL_CTX_free(context);
        return 1;
    }

    std::cout << "TCP 연결에 성공했습니다." << std::endl;

    // 이 연결에서 사용할 TLS 객체 생성
    SSL* ssl = SSL_new(context);

    if (ssl == nullptr)
    {
        ERR_print_errors_fp(stderr);
        close(clientSocket);
        close(serverSocket);
        SSL_CTX_free(context);
        return 1;
    }

    // 연결된 TCP 소켓을 TLS 객체에 연결
    if (SSL_set_fd(ssl, clientSocket) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(clientSocket);
        close(serverSocket);
        SSL_CTX_free(context);
        return 1;
    }

    // 서버 측 TLS 핸드셰이크
    int result = SSL_accept(ssl);

    if (result != 1)
    {
        int error = SSL_get_error(ssl, result);
        std::cerr << "TLS 핸드셰이크 실패. 오류 코드: "
            << error << std::endl;
        ERR_print_errors_fp(stderr);
    }
    else
    {
        std::cout << "TLS 핸드셰이크 성공: " << SSL_get_version(ssl) << std::endl;

        // TCP와 마찬가지로 줄바꿈을 메시지 끝으로 사용
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
            char ch; // 기본 예제에서는 줄바꿈을 확인하며 한 바이트씩 읽는다.

            int received = SSL_read(
                ssl,
                &ch,
                1
            );

            if (received <= 0)
            {
                int error = SSL_get_error(ssl, received);
                std::cerr << "TLS 수신 종료 또는 실패: "
                    << error << std::endl;
                ERR_print_errors_fp(stderr);
                success = false;
                break;
            }

            message.push_back(ch);
        }

        if (success)
        {
            std::cout << "받은 내용: " << message << std::endl;

            std::size_t totalSent = 0;

            while (totalSent < message.size())
            {
                int sent = SSL_write(
                    ssl,
                    message.data() + totalSent,
                    static_cast<int>(message.size() - totalSent)
                );

                if (sent <= 0)
                {
                    int error = SSL_get_error(ssl, sent);
                    std::cerr << "TLS 전송 실패: " << error << std::endl;
                    ERR_print_errors_fp(stderr);
                    success = false;
                    break;
                }

                totalSent += static_cast<std::size_t>(sent);
            }
        }

        if (success)
        {
            std::cout << "응답 전송을 완료했습니다." << std::endl;

            // 종료 알림을 보내고 상대의 종료 알림도 기다림
            int shutdownResult = SSL_shutdown(ssl);

            if (shutdownResult == 0)
            {
                shutdownResult = SSL_shutdown(ssl);
            }

            if (shutdownResult != 1)
            {
                std::cerr << "TLS 종료 알림 교환이 완료되지 않았습니다.\n";
                success = false;
            }
        }

        result = success ? 1 : 0;
    }

    SSL_free(ssl);
    close(clientSocket);
    close(serverSocket);
    SSL_CTX_free(context);

    return result == 1 ? 0 : 1;

}
