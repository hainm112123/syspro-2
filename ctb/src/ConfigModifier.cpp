#include "ConfigModifier.h"

std::string ConfigModifier::loadConfigAsString() {
#ifdef _WIN32
	auto path = stringToWstring(configFilePath);
	HANDLE hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("INVALID_HANDLE_VALUE: %ul", GetLastError());
		return "";
	}
	DWORD fileSize = GetFileSize(hFile, NULL);
	if (fileSize == INVALID_FILE_SIZE) {
		printf("INVALID_FILE_SIZE: %ul", GetLastError());
		CloseHandle(hFile);
		return "";
	}

	std::vector<char> buffer(fileSize);
	DWORD bytesRead = 0;
	if (!ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL)) {
		printf("READ FAILED: %ul", GetLastError());
		CloseHandle(hFile);
		return "";
	}
	CloseHandle(hFile);
	return std::string(buffer.begin(), buffer.end());
#else
	int fd = open(configFilePath.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0) return "";
	struct stat st;
	fstat(fd, &st);
	std::vector<char> buffer(st.st_size);
	if (read(fd, buffer.data(), st.st_size) < 0) {
		close(fd);
		return "";
	}
	close(fd);
	return std::string(buffer.begin(), buffer.end());
#endif
}

bool ConfigModifier::start() {
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
	int server = socket(AF_INET, SOCK_STREAM, 0);
#endif 
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

#ifdef _WIN32
	if (bind(server, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		std::cout << "Bind failed\n";
		return false;
	}
	if (listen(server, 5) == SOCKET_ERROR) {
		std::cout << "Listen failed\n";
		return false;
	}
#else
	if (bind(server, (sockaddr*)&addr, sizeof(addr)) < 0) {
		return false;
	}
	if (listen(server, 5) < 0) {
		return false;
	}
#endif

	std::string config = loadConfigAsString();
	//std::cout << config << "\n
	//std::cout << static_cast<uint32_t>(config.size()) << "\n";
	uint32_t len = htonl(static_cast<uint32_t>(config.size()));
	//std::cout << len << "\n";

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(server, &fds);
	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	printf("Waiting for config socket connection...\n");

#ifdef _WIN32
	int result = select(0, &fds, nullptr, nullptr, &tv);
	if (result > 0 && FD_ISSET(server, &fds)) {
		SOCKET client = accept(server, nullptr, nullptr);
		send(client, (char*)&len, sizeof(len), 0);
		send(client, config.c_str(), config.size(), 0);
		closesocket(client);
	}
	else {
		printf("No incoming connection\n");
	}
	closesocket(server);
	WSACleanup();
#else
	int result = select(server + 1, &fds, nullptr, nullptr, &tv);
	if (result > 0 && FD_ISSET(server, &fds)) {
		int client = accept(server, nullptr, nullptr);
		printf("Client socket: %d\n", client);
		send(client, (char*)&len, sizeof(len), 0);
		send(client, config.c_str(), config.size(), 0);
		close(client);
	}
	
	close(server);
#endif
	return true;
}
