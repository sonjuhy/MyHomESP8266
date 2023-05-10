# MyHome Project Ver 2.0

## 개요

---

기존 MyHome Project 1.0을 성공적으로 마무리 하고 모바일로만 가능한 접근 한계를 웹으로 넓히고 레거시 코드들을 리팩토링하며 안정성, 성능, 편의성을 업그레이드 하고자 한다.

## 구상

---

- 원격 클라이언트
    - 안드로이드
      - 가족 구성원 모두 갤럭시 스마트폰을 사용하므로 한번 개발하면 모두 사용 가능한 안드로이드 플랫폼을 선택
      - IoT 컨트롤 뿐만 아니라 파일 서버, 날씨, 일정 공유 또한 한 어플내에 가능하도록 올인원 플랫폼으로 제작하고자 함
      - 안드로이드 SDK 버전 업그레이드 및 레거시 코드 리팩토링 예정
    - Web(TypeScript)
      - 모바일 앱에서만 사용가능한 접근성을 Web을 추가하여 더 넓히고자 함.
      - 반응형으로 제작하여 모바일 또한 접근성 보장
      - 파일서버에서 클라우드로 업그레이드 하여 실시간 영상 스트리밍, 사진 관람도 가능하게 하고자 함
    - 로컬 컨트롤(음성인식 제어)
      - 집안에서 컨트롤 할 목적으로 음성인식 기반 제어
      - gTTS 서비스 사용
      
    
- 서버 : 우분투
    - 기존에 구축한 Ubuntu 18.04 LTS 서버 유지
    - 가족 구성원만 사용 가능한 파일 서버를 만들어 사진, 영상 등 정보를 최대 10TB 까지 지원
    - 백엔드 리팩토링 및 방화벽 재설정 등 보안성과 안정성 모두 크게 업그레이드 하고자 함
      - 백엔드 : PHP,Python Script -> SpringBoot, Django로 변경
      - CI/CD : 인터프리터 언어의 스크립트 수동배포 -> GitHub, Jenkins, Docker 통한 자동 배포
      - 보안 : ufw 활성화, 필요한 포트 일부만 오픈(DB 등은 로컬 네트워크에서만 접근 가능)
    
- iot : 전등 스위치
    - 도시가스 밸브, 방범문 도어락 과 같이 안전과 직결된 부분보다 안전 문제에 대해 비교적 안전하고 가장 사용성이 높은 전등 스위치부터 도전
    - 실 사용하며 불편했던 부분 개선 (서버 연결과 무관한 전등 작동 등)


## 개발 환경

---

- Android Studio
- IntelliJ
- Visual Studio Code
- PyCharm
- Arduino IDE

## 사용 라이브러리

---

- Switch(ESP8266)
    - Http (OTA Update)
    - MQTT Client
- Android
    - MQTT Client
    - REST
- Python (음성인식 제어)
  - gTTS
  - MQTT
- Back-End (Django)
    - MQTT Server
    - MQTT Client Python
    - Schedule
- Back-End(Spring Boot)
  - REST API
  - User
    - JWT
  - FileServer
  - MQTT
  - Security
  - WOL
- Server(DevOps)
    - Nginx
    - ~~PHP ~~(Spring Boot로 대체)
    - MariaDB

## 사용 언어

---

- JAVA
- Python
- TypeScript
- C++


## 사용 프레임 워크

---

 - Spring Boot
 - Django
 - NextJS

## CI/CD

---

 - GitHub
 - Jenkins
 - Docker
