// 교육용 TLS Echo: TCP 연결 위에 인증서 검증과 암호화 통신을 추가한다.
// 프로토콜 약속: 줄바꿈(\n)이 메시지의 끝이다. 한 연결 처리 후 종료한다.
#include <csignal>
#include <cstdio>
#include <iostream>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <string>

int main()
{
    // 상대가 먼저 닫아도 프로세스 종료 대신 TLS 오류를 처리한다.
    std::signal(SIGPIPE, SIG_IGN);

    // 1. 클라이언트용 TLS 설정 생성
    SSL_CTX* context = SSL_CTX_new(TLS_client_method());

    if (context == nullptr)
    {
        ERR_print_errors_fp(stderr);
        return 1;
    }

    if (SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }

    // 2. 서버 인증서 검증 활성화
    SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);

    // 실습에서 직접 만든 인증서를 신뢰하도록 설정
    if (SSL_CTX_load_verify_locations(
        context, "server-cert.pem", nullptr) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }

    // 3. TCP 소켓 생성
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket == -1)
    {
        std::perror("socket");
        SSL_CTX_free(context);
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(18443);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // 4. TCP 연결
    if (connect(clientSocket,
        reinterpret_cast<sockaddr*>(&address),
        sizeof(address)) == -1)
    {
        std::perror("connect");
        close(clientSocket);
        SSL_CTX_free(context);
        return 1;
    }

    std::cout << "TCP 연결에 성공했습니다." << std::endl;

    // 5. 연결 하나를 위한 TLS 객체 생성
    SSL* ssl = SSL_new(context);

    if (ssl == nullptr)
    {
        ERR_print_errors_fp(stderr);
        close(clientSocket);
        SSL_CTX_free(context);
        return 1;
    }

    // 6. 인증서에 접속 대상 IP가 포함되어 있는지도 검증
    if (X509_VERIFY_PARAM_set1_ip_asc(
        SSL_get0_param(ssl), "127.0.0.1") != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(clientSocket);
        SSL_CTX_free(context);
        return 1;
    }

    // 7. TCP 소켓과 TLS 객체 연결
    if (SSL_set_fd(ssl, clientSocket) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(clientSocket);
        SSL_CTX_free(context);
        return 1;
    }

    // 8. 클라이언트 측 TLS 핸드셰이크
    int result = SSL_connect(ssl);

    if (result != 1)
    {
        int error = SSL_get_error(ssl, result);

        std::cerr << "TLS 핸드셰이크 실패. 오류 코드: "
            << error << std::endl;
        ERR_print_errors_fp(stderr);
    }
    else
    {
        std::cout << "협상된 TLS 버전: " << SSL_get_version(ssl) << std::endl;
        std::cout << "서버 인증서 검증과 TLS 연결에 성공했습니다."
            << std::endl;

        const std::string message = "Hello, TLS!\n"; // 줄바꿈이 메시지의 끝
        std::size_t totalSent = 0;
        bool success = true;

        // 메시지 전송
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

        // 줄바꿈까지 Echo 답장 수신
        std::string reply;

        while (success && (reply.empty() || reply.back() != '\n'))
        {
            if (reply.size() >= 4096)
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

            reply.push_back(ch);
        }

        if (success)
        {
            std::cout << "서버의 답장: " << reply << std::endl;

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

    // 9. 자원 정리
    SSL_free(ssl);
    close(clientSocket);
    SSL_CTX_free(context);

    return result == 1 ? 0 : 1;
}
