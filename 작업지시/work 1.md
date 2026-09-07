# 목표

작업 지시/이미지 리스트.png 보이는 것처럼 
백엔드 서버의 경로에 존재하는 ( docker-compose 의 backend volumn 확인 )
이미지 리스트를 나열해주는 화면을 만들어줘.
한번에 20개 까지 표기하는걸 기본으로 하고, 
페이지 1 2 3 4 5 ~ 마지막 페이지 까지 표기할수 있도록 수정해줘 
ㅁㅁㅁㅁㅁ
ㅁㅁㅁㅁㅁ
ㅁㅁㅁㅁㅁ
ㅁㅁㅁㅁㅁ







# 시스템 환경 및 구조
- 실제 개발 ( 윈도우 VS Code 사용중 , VMWare의 Ubuntu 20.04 에서 수정내역을 
   git pull 로 내려받아 docker compose로 docker compose up -d --build 으로 빌드 )
- 프론트엔드: React 19 (Vite + Nginx 프로덕션 빌드 구조)
- 백엔드: Node.js (Express)
- 데이터베이스: MySQL 9.0.1
- 특이사항: 모든 서비스는 Docker Compose 환경 위에서 컨테이너로 맞물려 실행 중입니다. 코드 작성 시 컨테이너 간 통신(Host 이름 등)을 고려해 주세요.



# 버전 정보
1. React (frontend/package.json)  ( docker contianer - docker compose )
"dependencies": {
    "react": "^19.2.7",
    "react-dom": "^19.2.7"
}
2. Node.js: v22.16.0 ( docker contianer - docker compose )
3. MySQL: 9.0.1 ( docker contianer - docker compose )
4. Linux Host ( Ubuntu 20.04.6 )

# 요구사항
1. 모든 설명, 답변, 코드 내 주석은 항상 '한글'로 작성해 주세요.
2. 실제 `git commit` 명령은 실행하지 마세요. 대신 변경된 파일 목록과 수정 내용을 프로젝트 루트의 [참고자료/커밋로그] 
파일 안에 마크다운 형태로 기록해 주세요. 그리고 내가 요청하면 커밋로그를 가지고 커밋해주세요.

# 분석 및 수정해야 할 대상 경로 (이 파일들 위주로 컨텍스트를 파악하세요)
- 루트: docker-compose.yml, .gitignore
- 백엔드: backend/index.js, backend/package.json, backend/Dockerfile
- 프론트엔드: frontend/src/ 폴더 전체, frontend/package.json, frontend/Dockerfile, frontend/vite.config.js

# 분석에서 제외할 경로 (절대 읽지 마세요)
루트 디렉터리의 `AI Context Ignore List.md` 파일에 정의된 규칙(node_modules, package-lock.json 등)을 철저히 준수하여 분석에서 제외해 주세요.