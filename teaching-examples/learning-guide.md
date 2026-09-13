# TCP와 TLS 비교 실습

수업 전 개념 설명용 TCP·TLS 기본 Echo 예제다. Linux/WSL Ubuntu, C++17, OpenSSL을 사용한다. 클라이언트가 인사말 한 줄을 보내면 서버가 그대로 돌려주고 종료한다. 메시지 길이는 고정하지 않으며 줄바꿈(`\n`)이 끝을 표시한다. 먼저 TCP 연결을 설명하고 TLS에서 추가되는 부분을 비교한다.

## 파일과 읽는 순서

| 파일 | 설명할 내용 |
| --- | --- |
| `tcp_server.cpp` | socket → bind → listen → accept → recv → send → close |
| `tcp_client.cpp` | socket → connect → send → recv → close |
| `tls_server.cpp` | 서버 인증서·개인키 설정, TCP 연결, SSL_accept, SSL_read/write |
| `tls_client.cpp` | 신뢰할 인증서·접속 IP 검증, TCP 연결, SSL_connect, SSL_write/read |

소켓 번호는 포트 번호가 아니다. `accept()`는 서버의 대기 소켓과 별개인 통신 소켓을 반환한다. TLS도 먼저 TCP 연결이 필요하며, `SSL_set_fd()`로 그 소켓과 TLS 객체를 연결한다. `SSL_CTX`는 설정, `SSL`은 개별 연결을 나타낸다. 오류 처리 때 반복되는 free/close는 자원 정리다.

## 준비와 빌드

WSL에서 이 `teaching-examples` 폴더로 이동한다. Visual Studio에서는 이 폴더를 열고 WSL Ubuntu 대상으로 빌드할 수 있다.

```bash
sudo apt update
sudo apt install build-essential cmake libssl-dev openssl
cmake -S . -B build
cmake --build build -j 2
```

인증서 경로는 실행 파일의 위치가 아니라 **실행한 터미널의 현재 폴더**를 기준으로 한다. 아래 모든 실행 명령은 이 폴더에서 수행한다.

## TCP 실행

터미널 1:

```bash
./build/tcp_server
```

터미널 2(같은 WSL):

```bash
./build/tcp_client
```

TCP 포트는 18080이다. 클라이언트는 `서버의 전체 답장: Hello, TCP!`를 출력한다. 다시 실행하려면 서버부터 시작한다.

## TLS 인증서 생성과 실행

이 폴더에서 실습용 자체 서명 인증서와 개인키를 생성한다. 기존 파일이 있으면 아래 명령이 덮어쓰므로, 처음 생성할 때 사용한다.

```bash
umask 077
openssl req -x509 -newkey rsa:2048 -sha256 -nodes \
  -keyout server-key.pem -out server-cert.pem -days 30 \
  -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
```

- `server-cert.pem`: 공개키와 대상 정보가 있는 인증서. 서버가 제시하고 클라이언트가 명시적으로 신뢰한다.
- `server-key.pem`: 서버만 사용하는 개인키. 클라이언트에게 보내거나 Git에 올리지 않는다.
- 서버의 인증서·개인키 짝 검사와 클라이언트의 신뢰·IP 검증은 서로 다른 검사다.
- 이 예제는 서버 인증 TLS이며 mTLS가 아니다. 두 프로그램은 같은 WSL에서 실행하도록 루프백에 고정되어 있다.

터미널 1:

```bash
./build/tls_server
```

터미널 2:

```bash
./build/tls_client
```

TLS 포트는 18443이다. 협상된 TLS 버전과 `서버의 답장: Hello, TLS!`를 확인한다. TLS 1.2 이상을 허용하므로 실제 협상 버전은 설치 환경에 따라 달라질 수 있다.

## Wireshark로 설명하기

WSL 안의 Wireshark에서 `lo`를 선택하고 프로그램 실행 전에 캡처를 시작한다.

```text
TCP 표시 필터: tcp.port == 18080
TLS 표시 필터: tcp.port == 18443
```

TCP에서는 패킷 우클릭 → Follow → TCP Stream으로 평문을 확인한다. TLS에서는 Client Hello/Server Hello와 암호화된 레코드를 확인한다. 자동으로 TLS로 인식하지 않으면 Decode As에서 TLS를 선택한다. 세션 키를 별도로 제공하지 않은 캡처에서는 메시지 평문을 직접 읽을 수 없지만 IP, 포트, 패킷 길이와 시각은 보인다. TLS 1.3의 암호화된 레코드에는 일부 핸드셰이크·종료 메시지도 포함될 수 있으므로 모든 Application Data 표시를 Echo 본문이라고 설명하지 않는다.

## 전달할 핵심과 실습 범위

1. TCP는 바이트 순서를 보존하지만 메시지 경계를 보존하지 않는다. send 한 번이 recv 한 번이라는 보장은 없다.
2. 코드를 쉽게 읽도록 한 바이트씩 읽으며 줄바꿈을 확인한다. 네트워크 패킷을 한 바이트로 나누는 설정이 아니다. 실제 서비스에서는 큰 버퍼에서 줄바꿈을 찾는 방식으로 효율을 높인다.
3. 클라이언트의 인사말은 바꿔도 된다. 끝의 `\n`은 유지한다. 줄바꿈 없이 연결을 유지하면 상대가 계속 기다릴 수 있다. 예제는 줄바꿈을 포함해 최대 4096바이트까지만 받으며 한 연결에서 한 줄만 처리한다.
4. TCP 예제는 평문이고, TLS 예제는 같은 애플리케이션 데이터를 암호화·무결성 보호한다. 수신 프로그램은 복호화된 평문을 받는다.
5. TLS 클라이언트는 신뢰할 인증서와 접속 IP를 검증한다. 인증서 공개키로 본문 전체를 직접 암호화하는 구조가 아니라 핸드셰이크에서 마련한 통신 키로 본문을 보호한다.
6. 기본 흐름을 보여주는 동기식 예제다. 다중 클라이언트 처리, 연결·수신 제한 시간, 비동기 TLS 재시도, 서비스 운영 기능은 범위 밖이다. 대기 중단은 Ctrl+C로 한다.

실패 예시도 함께 보여줄 수 있다. 서버를 끄고 클라이언트를 실행하면 연결 실패, 인증서가 없는 폴더에서 TLS 클라이언트를 실행하면 인증서 로드 실패가 나야 한다. 신뢰하지 않는 별도 인증서를 클라이언트에 설정하면 핸드셰이크가 실패해야 한다. 검증을 끄는 방식으로 해결하지 않는다.

## 참고

- [OpenSSL TLS API 개요](https://docs.openssl.org/3.0/man7/ssl/)
- [Linux socket](https://man7.org/linux/man-pages/man2/socket.2.html)
- [Linux recv](https://man7.org/linux/man-pages/man2/recv.2.html)
